/*
 * Copyright (C) 2023 Robert Bosch GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "CompositeSensors.h"

#include <log/log.h>

using namespace bosch::sensors;

static constexpr float NOMINAL_GRAVITY = 9.80665f;
static constexpr float SQRT_3 = 1.732f;
static constexpr float WVEC_EPS = 1e-4f / SQRT_3;
static constexpr float SYMMETRY_TOLERANCE = 1e-10f;
static constexpr float FREE_FALL_THRESHOLD = 0.1f * (NOMINAL_GRAVITY);
static constexpr float DEFAULT_ACC_STDEV = 0.015f;     // m/s^2 (measured 0.08 / CDD 0.05)
static constexpr float DEFAULT_MAG_STDEV = 0.1f;       // uT    (measured 0.7  / CDD 0.5)
static constexpr float DEFAULT_GYRO_VAR = 1e-6;        // (rad/s)^2 / s
static constexpr float DEFAULT_GYRO_BIAS_VAR = 1e-12;  // (rad/s)^2 / s (guessed)

void CompositeSensorCore::activate(bool enable) {
  for (const auto& sensor : mDependencyList) {
    sensor->activateByType(mSensorData.type, enable);
  }
  mJustStarted = true;
}

bool CompositeSensorCore::readSensorTemperature(float* temperature) {
  for (const auto& sensor : mDependencyList) {
    if (sensor->readSensorTemperature(temperature)) {
      return true;
    }
  }
  return false;
}

void CompositeSensorCore::batch(int64_t samplingPeriodNs, int64_t maxReportLatencyNs) {
  for (const auto& sensor : mDependencyList) {
    sensor->batchByType(mSensorData.type, samplingPeriodNs, maxReportLatencyNs);
  }
  mSamplingPeriodNs = samplingPeriodNs;
}

static android::mat<float, 3, 3> crossMatrix(const android::vec<float, 3>& p, float diag) {
  android::mat<float, 3, 3> r;
  r[0][0] = diag;
  r[1][1] = diag;
  r[2][2] = diag;
  r[0][1] = p.z;
  r[1][0] = -p.z;
  r[0][2] = -p.y;
  r[2][0] = p.y;
  r[1][2] = p.x;
  r[2][1] = -p.x;
  return r;
}

static android::mat33_t scaleCovariance(const android::mat33_t& A, const android::mat33_t& P) {
  // A*P*transpose(A);
  android::mat33_t APAt;
  for (size_t r = 0; r < 3; r++) {
    for (size_t j = r; j < 3; j++) {
      double apat(0);
      for (size_t c = 0; c < 3; c++) {
        double v(A[c][r] * P[c][c] * 0.5);
        for (size_t k = c + 1; k < 3; k++) v += A[k][r] * P[c][k];
        apat += 2 * v * A[c][j];
      }
      APAt[j][r] = apat;
      APAt[r][j] = apat;
    }
  }

  return APAt;
}

void CompositeSensorCore::checkState() {
  // P needs to stay positive semidefinite or the fusion diverges. When we
  // detect divergence, we reset the fusion.
  // TODO(braun): Instead, find the reason for the divergence and fix it.
  if (!isPositiveSemidefinite(mP[0][0], SYMMETRY_TOLERANCE) || !isPositiveSemidefinite(mP[1][1], SYMMETRY_TOLERANCE)) {
    ALOGW("Sensor fusion diverged; resetting state.");
    mP = 0;
  }
}

android::mat<float, 3, 4> CompositeSensorCore::getF(const android::vec4_t& q) {
  android::mat<float, 3, 4> F;

  // This is used to compute the derivative of q
  // F = | [q.xyz]x |
  //     |  -q.xyz  |

  F[0].x = q.w;
  F[1].x = -q.z;
  F[2].x = q.y;
  F[0].y = q.z;
  F[1].y = q.w;
  F[2].y = -q.x;
  F[0].z = -q.y;
  F[1].z = q.x;
  F[2].z = q.w;
  F[0].w = -q.x;
  F[1].w = -q.y;
  F[2].w = -q.z;
  return F;
}

void CompositeSensorCore::predict(const android::vec3_t& w, float dT) {
  const android::vec4_t q = mX0;
  const android::vec3_t b = mX1;
  android::vec3_t we = w - b;

  if (length(we) < WVEC_EPS) {
    we = (we[0] > 0.f) ? WVEC_EPS : -WVEC_EPS;
  }
  // q(k+1) = O(w)*q(k)
  // --------------------
  //
  // O(w) = | cos(0.5*||w||*dT)*I33 - [psi]x                   psi |
  //        | -psi'                              cos(0.5*||w||*dT) |
  //
  // psi = sin(0.5*||w||*dT)*w / ||w||
  //
  //
  // P(k+1) = Phi(k)*P(k)*Phi(k)' + G*Q(k)*G'
  // ----------------------------------------
  //
  // G = | -I33    0 |
  //     |    0  I33 |
  //
  //  Phi = | Phi00 Phi10 |
  //        |   0     1   |
  //
  //  Phi00 =   I33
  //          - [w]x   * sin(||w||*dt)/||w||
  //          + [w]x^2 * (1-cos(||w||*dT))/||w||^2
  //
  //  Phi10 =   [w]x   * (1        - cos(||w||*dt))/||w||^2
  //          - [w]x^2 * (||w||*dT - sin(||w||*dt))/||w||^3
  //          - I33*dT

  const android::mat33_t I33(1);
  const android::mat33_t I33dT(dT);
  const android::mat33_t wx(crossMatrix(we, 0));
  const android::mat33_t wx2(wx * wx);
  const float lwedT = length(we) * dT;
  const float hlwedT = 0.5f * lwedT;
  const float ilwe = 1.f / length(we);
  const float k0 = (1 - cosf(lwedT)) * (ilwe * ilwe);
  const float k1 = sinf(lwedT);
  const float k2 = cosf(hlwedT);
  const android::vec3_t psi(sinf(hlwedT) * ilwe * we);
  const android::mat33_t O33(crossMatrix(-psi, k2));
  android::mat44_t O;
  O[0].xyz = O33[0];
  O[0].w = -psi.x;
  O[1].xyz = O33[1];
  O[1].w = -psi.y;
  O[2].xyz = O33[2];
  O[2].w = -psi.z;
  O[3].xyz = psi;
  O[3].w = k2;

  mPhi[0][0] = I33 - wx * (k1 * ilwe) + wx2 * k0;
  mPhi[1][0] = wx * k0 - I33dT - wx2 * (ilwe * ilwe * ilwe) * (lwedT - k1);

  mX0 = O * q;

  if (mX0.w < 0) mX0 = -mX0;

  mP = mPhi * mP * transpose(mPhi) + mGQGt;

  checkState();
}

void CompositeSensorCore::update(const android::vec3_t& z, const android::vec3_t& Bi, float sigma) {
  android::vec4_t q(mX0);
  // measured vector in body space: h(p) = A(p)*Bi
  const android::mat33_t A(quatToMatrix(q));
  const android::vec3_t Bb(A * Bi);

  // Sensitivity matrix H = dh(p)/dp
  // H = [ L 0 ]
  const android::mat33_t L(crossMatrix(Bb, 0));

  // gain...
  // K = P*Ht / [H*P*Ht + R]
  android::vec<android::mat33_t, 2> K;
  const android::mat33_t R(sigma * sigma);
  const android::mat33_t S(scaleCovariance(L, mP[0][0]) + R);
  const android::mat33_t Si(invert(S));
  const android::mat33_t LtSi(transpose(L) * Si);
  K[0] = mP[0][0] * LtSi;
  K[1] = transpose(mP[1][0]) * LtSi;

  // update...
  // P = (I-K*H) * P
  // P -= K*H*P
  // | K0 | * | L 0 | * P = | K0*L  0 | * | P00  P10 | = | K0*L*P00  K0*L*P10 |
  // | K1 |                 | K1*L  0 |   | P01  P11 |   | K1*L*P00  K1*L*P10 |
  // Note: the Joseph form is numerically more stable and given by:
  //     P = (I-KH) * P * (I-KH)' + K*R*R'
  const android::mat33_t K0L(K[0] * L);
  const android::mat33_t K1L(K[1] * L);
  mP[0][0] -= K0L * mP[0][0];
  mP[1][1] -= K1L * mP[1][0];
  mP[1][0] -= K0L * mP[1][0];
  mP[0][1] = transpose(mP[1][0]);

  const android::vec3_t e(z - Bb);
  const android::vec3_t dq(K[0] * e);

  q += getF(q) * (0.5f * dq);
  mX0 = normalize_quat(q);

  const android::vec3_t db(K[1] * e);
  mX1 += db;

  checkState();
}

android::vec3_t getOrthogonal(const android::vec3_t& v) {
  android::vec3_t w;
  if (fabsf(v[0]) <= fabsf(v[1]) && fabsf(v[0]) <= fabsf(v[2])) {
    w[0] = 0.f;
    w[1] = v[2];
    w[2] = -v[1];
  } else if (fabsf(v[1]) <= fabsf(v[2])) {
    w[0] = v[2];
    w[1] = 0.f;
    w[2] = -v[0];
  } else {
    w[0] = v[1];
    w[1] = -v[0];
    w[2] = 0.f;
  }
  return normalize(w);
}

void CompositeSensorCore::initFusion(const android::vec4_t& q) {
  if (mGyroVar == 0) mGyroVar = DEFAULT_GYRO_VAR;
  mBa.x = 0;
  mBa.y = 0;
  mBa.z = 1;

  mBm.x = 0;
  mBm.y = 1;
  mBm.z = 0;

  // initial estimate: E{ x(t0) }
  mX0 = q;
  mX1 = 0;

  // process noise covariance matrix: G.Q.Gt, with
  //
  //  G = | -1 0 |        Q = | q00 q10 |
  //      |  0 1 |            | q01 q11 |
  //
  // q00 = sv^2.dt + 1/3.su^2.dt^3
  // q10 = q01 = 1/2.su^2.dt^2
  // q11 = su^2.dt
  //

  const float dT = mSamplingPeriodNs / 1e9f;
  const float dT2 = dT * dT;
  const float dT3 = dT2 * dT;

  // variance of integrated output at 1/dT Hz (random drift)
  const float q00 = mGyroVar * dT + 0.33333f * DEFAULT_GYRO_BIAS_VAR * dT3;

  // variance of drift rate ramp
  const float q11 = DEFAULT_GYRO_BIAS_VAR * dT;
  const float q10 = 0.5f * DEFAULT_GYRO_BIAS_VAR * dT2;
  const float q01 = q10;

  mGQGt[0][0] = q00;  // rad^2
  mGQGt[1][0] = -q10;
  mGQGt[0][1] = -q01;
  mGQGt[1][1] = q11;  // (rad/s)^2

  // initial covariance: Var{ x(t0) }
  // TODO: initialize P correctly
  mP = 0;
}

void CompositeSensorCore::initRodrParams(const android::vec3_t& acc) {
  android::mat33_t R;
  android::vec3_t up(normalize(acc));
  android::vec3_t east(getOrthogonal(up));
  android::vec3_t north(cross_product(up, east));
  R << east << north << up;
  CompositeSensorCore::initFusion(android::matrixToQuat(R));
}

SensorValues CompositeSensorCore::calculateGravity(std::vector<SensorValues> accValues,
                                                   std::vector<SensorValues> gyroValues) {
  android::vec3_t pulse;
  android::vec3_t accel;
  android::vec3_t g;
  float deltaTime;
  SensorValues result;

  accel.x = accValues[0].data[0];
  accel.y = accValues[0].data[1];
  accel.z = accValues[0].data[2];

  if (mJustStarted) {
    CompositeSensorCore::initRodrParams(accel);
    mJustStarted = false;
  }

  const float l = android::length(accel);
  if (l >= FREE_FALL_THRESHOLD) {
    // unless in free-fall do error correction
    const float l_inv = 1.0f / l;
    android::vec3_t m;
    m = android::quatToMatrix(mX0) * mBm;
    update(m, mBm, DEFAULT_MAG_STDEV);
    android::vec3_t unityA = accel * l_inv;
    const float d = sqrtf(fabsf(l - NOMINAL_GRAVITY));
    const float p = l_inv * DEFAULT_ACC_STDEV * expf(d);

    update(unityA, mBa, p);
  }

  pulse.x = gyroValues[0].data[0];
  pulse.y = gyroValues[0].data[1];
  pulse.z = gyroValues[0].data[2];
  deltaTime = gyroValues[0].timestamp - mLastTimestamp;
  mLastTimestamp = gyroValues[0].timestamp;

  predict(pulse, deltaTime / 1e9f);

  const android::mat33_t R(android::quatToMatrix(mX0));

  g = R[2] * NOMINAL_GRAVITY;
  result.data.push_back(g.x);
  result.data.push_back(g.y);
  result.data.push_back(g.z);
  result.timestamp = mLastTimestamp;

  return result;
}

// TODO: use proper sensor fusion algorithm
std::vector<SensorValues> LinearAcceleration::readSensorValues() {
  std::vector<SensorValues> accValues{};
  std::vector<SensorValues> gyroValues{};
  SensorValues result;
  std::vector<SensorValues> sensorValues{};
  for (const auto& sensor : mDependencyList) {
    if (sensor->getSensorData().type == BoschSensorType::ACCEL) {
      accValues = sensor->readSensorValues();
    } else if (sensor->getSensorData().type == BoschSensorType::GYRO) {
      gyroValues = sensor->readSensorValues();
    }
  }

  result = calculateGravity(accValues, gyroValues);
  accValues[0].data[0] -= result.data[0];
  accValues[0].data[1] -= result.data[1];
  accValues[0].data[2] -= result.data[2];

  sensorValues.push_back(accValues[0]);

  return sensorValues;
}

// TODO: use proper sensor fusion algorithm
std::vector<SensorValues> Gravity::readSensorValues() {
  std::vector<SensorValues> accValues{};
  std::vector<SensorValues> gyroValues{};
  SensorValues result;
  std::vector<SensorValues> sensorValues{};
  for (const auto& sensor : mDependencyList) {
    if (sensor->getSensorData().type == BoschSensorType::ACCEL) {
      accValues = sensor->readSensorValues();
    } else if (sensor->getSensorData().type == BoschSensorType::GYRO) {
      gyroValues = sensor->readSensorValues();
    }
  }

  result = calculateGravity(accValues, gyroValues);

  sensorValues.push_back(result);

  return sensorValues;
}
