# Android HAL for Bosch Sensors

## Table of Contents
 - [Introduction](#Intro)
 - [Documentaion](#doc)
 - [Architecture](#Arch)

## Introduction <a name=Intro></a>

This repository contains the Android Sensor Hardware Abstraction Layer (HAL) for Bosch MEMS Automotive sensors.

The Android HAL is generic and supports all implemented sensors simultaneously.
Following Sensors are supported:
* smi230
* smi240
* smi330

Following HAL versions are supported:
* hidl 2.0
* hidl 2.1
* aidl
* multi_hidl_2.1
* multi_aidl

All hidls are only supported in android v13

The Android HAL passes all the google CTS and VTS tests

[CTS Test Report](test/CTS/2025.02.03_17.22.10/test_result.html)

[VTS Test Report](test/VTS/multihal/aidl/host/linux-x86/vts/android-vts/results/latest/test_result.html)


## Documentaion <a name=doc></a>
https://boschmemssolutions.github.io/hal/bosch_MEMS-Android-HAL.html

## Architecture <a name=Arch></a>
```
                 Android Apps
                      |
                 Android HAL
                      |
               Bosch Sensor HAL
                      |
-------------------------------------------------------
                 |          |
               sysfs       dev
                 \          /
                 iio-subsystem
	                    |
                  bosch-driver
                      |
-------------------------------------------------------
                  Hardware
```
