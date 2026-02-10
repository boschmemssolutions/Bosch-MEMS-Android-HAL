![logo](bosch_logo.png)
Test Report



|  |
| --- |
|  |

| Summary | |
| --- | --- |
| Suite / Plan | VTS / vts-bosch |
| Suite / Build | 13\_r4 / eng.she2rt.20250514.110827 |
| Host Info | Result/@start RT-C-008FN (Linux - 5.15.0-139-generic) |
| Start time / End Time | Thu Jan 22 12:12:10 UTC 2026 / Thu Jan 22 12:18:06 UTC 2026 |
| Tests Passed | 29 |
| Tests Failed | 0 |
| Modules Done | 3 |
| Modules Total | 3 |
| Fingerprint | Raspberry/bosch\_hal\_aidl/rpi4:13/TQ3A.230901.001/3:userdebug/test-keys |
| Security Patch | 2023-09-01 |
| Release (SDK) | 13 (33) |
| ABIs | arm64-v8a,armeabi-v7a,armeabi |

  

| Module | Passed | Failed | Assumption Failure | Ignored | Total Tests | Done | Reason |
| --- | --- | --- | --- | --- | --- | --- | --- |
| [arm64-v8a BoschAidlHalSensorsTargetTest](#arm64-v8a%C2%A0BoschAidlHalSensorsTargetTest) | 14 | 0 | 0 | 0 | 14 | true |  |
| [arm64-v8a VtsAidlHalSensorsTargetTest](#arm64-v8a%C2%A0VtsAidlHalSensorsTargetTest) | 12 | 0 | 0 | 0 | 12 | true |  |
| [arm64-v8a VtsHalSensorManagerV1\_0TargetTest](#arm64-v8a%C2%A0VtsHalSensorManagerV1_0TargetTest) | 3 | 0 | 0 | 1 | 4 | true |  |  

|  |  |  |
| --- | --- | --- |
| <span id="arm64-v8a BoschAidlHalSensorsTargetTest">arm64-v8a BoschAidlHalSensorsTargetTest</span> | | |
| Test | Result | Details |
| Sensors/BoschSensorTest#SensorListContainsBoschSensor/0\_android\_hardware\_sensors\_ISensors\_default | pass |  |
| Sensors/BoschSensorTest#AccelerometerCheckSensorVector/0\_android\_hardware\_sensors\_ISensors\_default | pass |  |
| Sensors/BoschSensorTest#GyroscopeCheckSensorVector/0\_android\_hardware\_sensors\_ISensors\_default | pass |  |
| Sensors/BoschSensorTest#GravityCheckSensorVector/0\_android\_hardware\_sensors\_ISensors\_default | pass |  |
| Sensors/BoschSensorTest#LinearAccelerationCheckSensorVector/0\_android\_hardware\_sensors\_ISensors\_default | pass |  |
| Sensors/BoschSensorTest#AccelerometerCheckDirectChVector/0\_android\_hardware\_sensors\_ISensors\_default | pass |  |
| Sensors/BoschSensorTest#GyroscopeCheckDirectChVector/0\_android\_hardware\_sensors\_ISensors\_default | pass |  |
| Sensors/BoschSensorTest#GravityCheckDirectChVector/0\_android\_hardware\_sensors\_ISensors\_default | pass |  |
| Sensors/BoschSensorTest#LinearAccelerationCheckDirectChVector/0\_android\_hardware\_sensors\_ISensors\_default | pass |  |
| Sensors/BoschSensorTest#AccelerometerCheckMultiChannelVector/0\_android\_hardware\_sensors\_ISensors\_default | pass |  |
| Sensors/BoschSensorTest#GyroscopeCheckMultiChannelVector/0\_android\_hardware\_sensors\_ISensors\_default | pass |  |
| Sensors/BoschSensorTest#LinearAccelerationCheckMultiChannelVector/0\_android\_hardware\_sensors\_ISensors\_default | pass |  |
| Sensors/BoschSensorTest#GravityCheckMultiChannelVector/0\_android\_hardware\_sensors\_ISensors\_default | pass |  |
| Sensors/BoschSensorTest#ConfigCheck/0\_android\_hardware\_sensors\_ISensors\_default | pass |  |

|  |  |  |
| --- | --- | --- |
| <span id="arm64-v8a VtsAidlHalSensorsTargetTest">arm64-v8a VtsAidlHalSensorsTargetTest</span> | | |
| Test | Result | Details |
| Sensors/SensorsAidlTest#SensorListValid/0\_android\_hardware\_sensors\_ISensors\_default | pass |  |
| Sensors/SensorsAidlTest#SetOperationMode/0\_android\_hardware\_sensors\_ISensors\_default | pass |  |
| Sensors/SensorsAidlTest#InjectSensorEventData/0\_android\_hardware\_sensors\_ISensors\_default | pass |  |
| Sensors/SensorsAidlTest#CallInitializeTwice/0\_android\_hardware\_sensors\_ISensors\_default | pass |  |
| Sensors/SensorsAidlTest#CleanupConnectionsOnInitialize/0\_android\_hardware\_sensors\_ISensors\_default | pass |  |
| Sensors/SensorsAidlTest#FlushSensor/0\_android\_hardware\_sensors\_ISensors\_default | pass |  |
| Sensors/SensorsAidlTest#FlushOneShotSensor/0\_android\_hardware\_sensors\_ISensors\_default | pass |  |
| Sensors/SensorsAidlTest#FlushInactiveSensor/0\_android\_hardware\_sensors\_ISensors\_default | pass |  |
| Sensors/SensorsAidlTest#Batch/0\_android\_hardware\_sensors\_ISensors\_default | pass |  |
| Sensors/SensorsAidlTest#Activate/0\_android\_hardware\_sensors\_ISensors\_default | pass |  |
| Sensors/SensorsAidlTest#NoStaleEvents/0\_android\_hardware\_sensors\_ISensors\_default | pass |  |
| Sensors/SensorsAidlTest#DirectChannelAshmem/0\_android\_hardware\_sensors\_ISensors\_default | pass |  |

|  |  |  |
| --- | --- | --- |
| <span id="arm64-v8a VtsHalSensorManagerV1_0TargetTest">arm64-v8a VtsHalSensorManagerV1\_0TargetTest</span> | | |
| Test | Result | Details |
| PerInstance/SensorManagerTest#List/0\_default | pass |  |
| PerInstance/SensorManagerTest#Ashmem/0\_default | pass |  |
| PerInstance/SensorManagerTest#GetDefaultAccelerometer/0\_default | pass |  |
| PerInstance/SensorManagerTest#Accelerometer/0\_default | IGNORED |  |