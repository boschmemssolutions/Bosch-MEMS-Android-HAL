![Bosch Logo](bosch_logo.png)

# smi230 Bosch VTS Android HAL - System Test

Test summary:
  - Total tests: 23
  - Passed: 22
  - Failed: 0
  - Ignored: 1

# Test case 1: SensorsAidlTest

## MEMS AIDL TC1: SensorListValid

### Description

Test if sensor hal can list all valid sensors

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL AIDL Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor hal shall list all valid sensors

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS AIDL TC2: SensorListContainsBoschSensor

### Description

Test if sensor hal can list Bosch sensors

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL AIDL Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor hal shall list Bosch sensors

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS AIDL TC3: SetOperationMode

### Description

Test that SetOperationMode returns the expected value

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL AIDL Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

SetOperationMode shall returns the expected value

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS AIDL TC4: InjectSensorEventData

### Description

Test that an injected event is written back to the Event FMQ

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL AIDL Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Injected event shall be written back to the Event FMQ

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS AIDL TC5: CallInitializeTwice

### Description

Test that if initialize is called twice, then the HAL writes events to the FMQs from the second  call to the function.

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL AIDL Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

If initialize is called twice, then the HAL shall writes events to the FMQs from the second  call to the function.

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS AIDL TC6: CleanupConnectionsOnInitialize

### Description

Test Cleanup Connections On Initialize

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL AIDL Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

The environment shall be reset properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS AIDL TC7: FlushSensor

### Description

Test flush a sensor that is not a one-shot sensor

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL AIDL Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor hal shall flush the sensor properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS AIDL TC8: FlushOneShotSensor

### Description

Test flush a sensor that is a one-shot sensor

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL AIDL Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor hal shall flush the sensor properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS AIDL TC9: FlushInactiveSensor

### Description

Test flush a sensor that is inactive

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL AIDL Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor hal shall flush the sensor properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS AIDL TC10: Batch

### Description

Test Call batch on an inactive/active/invalid sensor

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL AIDL Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Bach shall handel the different cases properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS AIDL TC11: Activate

### Description

Test Call Activate on an inactive/active/invalid sensor

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL AIDL Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Activate shall handel the different cases properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS AIDL TC12: AccelerometerCheckSensorVector

### Description

Test event received is not stale

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL AIDL Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor event received shall be not stale

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS AIDL TC13: GyroscopeCheckSensorVector

### Description

Test verify direct channel ashmem

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL AIDL Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Direct channel verification shall be success

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS AIDL TC14: GravityCheckSensorVector

### Description

Test verify direct channel gralloc

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL AIDL Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Direct channel verification shall be success

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS AIDL TC15: LinearAccelerationCheckSensorVector

### Description

Test if accelerometer sensor vector is in range

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL AIDL Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor vector shall be in expected range

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS AIDL TC16: ConfigCheck

### Description

Test if gyroscope sensor vector is in range

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL AIDL Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor vector shall be in expected range

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS AIDL TC17: NoStaleEvents

### Description

Test if gravity sensor vector is in range

### Pre-Condition

Hardware and SPI connection to SMI 240 is set up properly
System started with SMI240 IIO Linux Driver
System started with Sensor HAL AIDL Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor vector shall be in expected range

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS AIDL TC18: DirectChannelAshmem

### Description

Test if linear acceleration sensor vector is in range

### Pre-Condition

Hardware and SPI connection to SMI 240 is set up properly
System started with SMI240 IIO Linux Driver
System started with Sensor HAL AIDL Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor vector shall be in expected range

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS AIDL TC19: DirectChannelGralloc

### Description

Test verify direct channel gralloc

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL AIDL Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Direct channel verification shall be success

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

# Test case 2: SensorManagerTest

## MEMS AIDL TC20: List

### Description

TODO ADD DESCRIPTION

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL AIDL Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

TODO ADD HERE

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS AIDL TC21: Ashmem

### Description

TODO ADD DESCRIPTION

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL AIDL Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

TODO ADD HERE

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS AIDL TC22: GetDefaultAccelerometer

### Description

TODO ADD DESCRIPTION

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL AIDL Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

TODO ADD HERE

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS AIDL TC23: Accelerometer

### Description

TODO ADD DESCRIPTION

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL AIDL Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

TODO ADD HERE

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **IGNORED** |  |

