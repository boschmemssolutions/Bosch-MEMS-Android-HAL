# Bosch MEMS Android HAL

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![Android](https://img.shields.io/badge/Android-15-green.svg)](https://developer.android.com/)
[![HAL](https://img.shields.io/badge/HAL-AIDL%20Only-green.svg)](https://source.android.com/docs/core/architecture/hal)

## Table of Contents

- [Introduction](#introduction)
- [CTS/VTS Test Reports](#cts-vts-test-reports)
- [Features](#features) 
- [Supported Sensors](#supported-sensors)
- [Supported HAL Versions](#supported-hal-versions)
- [Architecture Overview](#architecture-overview)
- [Integration Guide](#integration-guide)
- [License](#license)

## Introduction

The **Bosch MEMS Android HAL** is a comprehensive Hardware Abstraction Layer implementation for Bosch MEMS automotive sensors on Android platforms. This HAL provides a standardized interface between the Android sensor framework and Bosch sensor hardware, enabling seamless integration of high-performance inertial measurement units (IMUs) in automotive and mobile applications.

### Key Benefits

- **Universal Compatibility**: Single HAL supports multiple Bosch sensor variants simultaneously
- **High Performance**: Optimized for automotive-grade applications with low latency and high reliability  
- **Android Standards Compliant**: Fully compliant with Android Sensor HAL specifications
- **Multi-HAL Architecture**: Supports both standalone and multi-HAL configurations
- **Comprehensive Sensor Types**: Supports accelerometer, gyroscope, gravity, linear acceleration, and uncalibrated variants

## CTS/VTS Test Reports

The Bosch MEMS Android HAL passes all Google CTS and VTS tests.

- [CTS Test Report](test/CTS/)
- [VTS Test Report](test/VTS/)

## Features

- ✅ **Multi-sensor support** - Simultaneous operation of multiple Bosch MEMS sensors
- ✅ **Real-time data streaming** - Low-latency sensor data with configurable sample rates
- ✅ **Direct channel support** - High-performance direct memory access for sensor data
- ✅ **Composite sensor fusion** - Gravity and linear acceleration sensors derived from accelerometer and gyroscope
- ✅ **IIO subsystem integration** - Leverages Linux Industrial I/O framework
- ✅ **Multiple scan modes** - Support for both polling and buffer-based data acquisition

## Supported Sensors

| Sensor Model | Accelerometer | Gyroscope | Temperature | Notes |
|--------------|---------------|-----------|-------------|-------|
| **SMI230** | ✅ 16-bit | ✅ 16-bit | ✅ | High-performance automotive IMU |
| **SMI240** | ✅ 16-bit | ✅ 16-bit | ✅ | ASIL B automotive IMU |  
| **SMI330** | ✅ 16-bit | ✅ 16-bit | ✅ | Next-generation automotive IMU |

### Derived Sensor Types

All supported sensors provide the following Android sensor types:

- **Primary Sensors**
  - `SENSOR_TYPE_ACCELEROMETER` - 3-axis accelerometer
  - `SENSOR_TYPE_GYROSCOPE` - 3-axis gyroscope

- **Composite Sensors** (calculated from primary sensors)
  - `SENSOR_TYPE_GRAVITY` - Gravity vector estimation
  - `SENSOR_TYPE_LINEAR_ACCELERATION` - Linear acceleration (gravity removed)
  
- **Uncalibrated Sensors**
  - `SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED` - Raw accelerometer data
  - `SENSOR_TYPE_GYROSCOPE_UNCALIBRATED` - Raw gyroscope data

## Supported HAL Versions

| HAL Type | Android Version | Status |
|----------|-----------------|--------|
| **AIDL** | Android 15 | ✅ Supported |
| **Multi-AIDL** | Android 15 | ✅ Supported |
| **AIDL** | Android 13 | ✅ Supported/untested |
| **Multi-AIDL** | Android 13 | ✅ Supported/untested |
| **AIDL** | Android 13 | ✅ Supported |
| **Multi-AIDL** | Android 13 | ✅ Supported |
| **HIDL 2.0/2.1** | Android 13 and below | ❌ Not Supported |
| **Multi-HIDL** | Android 13 and below | ❌ Not Supported |

> ⚠️ **Important**: This implementation is designed specifically for Android 13, Android 15. Only AIDL HALs are supported. It may work with other Android versions, but this is not tested.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                    Android Apps                         │
│   (SensorManager API, NDK Sensor API)                   │
└─────────────────┬───────────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────────┐
│              Android Framework                          │
│        (Sensor Service, HAL Interface)                  │
└─────────────────┬───────────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────────┐
│               Bosch Sensor HAL                          │
│  ┌──────────────────────────┬─────────────────────────┐ │
│  │   AIDL HAL               │     MultiHAL Proxy      │ │
│  └──────────────────────────┴─────────────────────────┘ │
│  ┌────────────────────────────────────────────────────┐ │
│  │              Common Core Framework                 │ │
│  │    (SensorCore, SensorList, SensorThread)          │ │
│  └────────────────────────────────────────────────────┘ │
└─────────────────┬───────────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────────┐
│                Linux Kernel                             │
│  ┌─────────────┬─────────────────────┬────────────────┐ │
│  │   sysfs     │      dev nodes      │   IIO buffer   │ │
│  └─────────────┴─────────────────────┴────────────────┘ │
│  ┌────────────────────────────────────────────────────┐ │
│  │              IIO Subsystem                         │ │
│  └────────────────────────────────────────────────────┘ │
│  ┌────────────────────────────────────────────────────┐ │
│  │              Bosch IIO Drivers                     │ │
│  │              (smi230, smi240, smi330)              │ │
│  └────────────────────────────────────────────────────┘ │
└─────────────────┬───────────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────────┐
│                Hardware                                 │
│          (I2C/SPI Sensor Devices)                       │
└─────────────────────────────────────────────────────────┘
```

### Single-Threaded Polling Architecture

The HAL uses an efficient unified threading model where a single `SensorThread` per IIO device manages all sensor polling:

```
┌─────────────────────────────────────────────────────┐
│         SensorThread (per IIO device)               │
│  ┌─────────────────────────────────────────────────┐│
│  │              Main Loop                          ││
│  │  ┌─────────────────────────────────────────────┐││
│  │  │  1. Calculate next poll interval (min ODR)  │││
│  │  │  2. Read IIO buffer                         │││  
│  │  │  3. Apply rotation matrix transformation    │││
│  │  │  4. Notify SensorCore observers             │││
│  │  │  5. Trigger composite sensor fusion         │││
│  │  └─────────────────────────────────────────────┘││
│  └─────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────┘
```

**Key Benefits:**
- **Low overhead**: One thread shared by accelerometer + gyroscope
- **Synchronized data**: Same timestamp for accel + gyro samples
- **Flexible modes**: HW FIFO, interrupts, or software polling
- **Dynamic ODR**: Polls at highest requested rate from all active sensors

## Integration Guide

See [Integration Guide](doc/IntegrationGuide.md) for comprehensive platform integration instructions.

## License

This project is licensed under the Apache License 2.0 - see the [LICENSE](LICENSE) file for details.

```
Copyright (C) 2023 Robert Bosch GmbH

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```

---

**Bosch MEMS Android HAL** - Enabling next-generation sensor experiences on Android 🚗📱
