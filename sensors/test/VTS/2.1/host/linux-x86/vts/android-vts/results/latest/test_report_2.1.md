![Bosch Logo](bosch_logo.png)

# smi230 Bosch VTS Android HAL - System Test

Test summary:
  - Total tests: 63
  - Passed: 63
  - Failed: 0
  - Ignored: 0

# Test case 1: SensorsHidlTest

## MEMS HIDL2X TC1: SensorListValid

### Description

Test if sensor hal can list all valid sensors

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
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

## MEMS HIDL2X TC2: SensorListContainsBoschSensor

### Description

Test if sensor hal can list Bosch sensors

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
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

## MEMS HIDL2X TC3: SetOperationMode

### Description

Test that SetOperationMode returns the expected value

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
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

## MEMS HIDL2X TC4: InjectSensorEventData

### Description

Test that an injected event is written back to the Event FMQ

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
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

## MEMS HIDL2X TC5: CallInitializeTwice

### Description

Test that if initialize is called twice, then the HAL writes events to the FMQs from the second  call to the function.

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
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

## MEMS HIDL2X TC6: CleanupConnectionsOnInitialize

### Description

Test Cleanup Connections On Initialize

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
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

## MEMS HIDL2X TC7: FlushSensor

### Description

Test flush a sensor that is not a one-shot sensor

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
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

## MEMS HIDL2X TC8: FlushOneShotSensor

### Description

Test flush a sensor that is a one-shot sensor

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
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

## MEMS HIDL2X TC9: FlushInactiveSensor

### Description

Test flush a sensor that is inactive

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
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

## MEMS HIDL2X TC10: Batch

### Description

Test Call batch on an inactive/active/invalid sensor

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
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

## MEMS HIDL2X TC11: Activate

### Description

Test Call Activate on an inactive/active/invalid sensor

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
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

## MEMS HIDL2X TC12: NoStaleEvents

### Description

Test event received is not stale

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
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

## MEMS HIDL2X TC13: DirectChannelAshmem

### Description

Test verify direct channel ashmem

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
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

## MEMS HIDL2X TC14: DirectChannelGralloc

### Description

Test verify direct channel gralloc

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
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

## MEMS HIDL2X TC15: AccelerometerStreamingOperationSlow

### Description

Test if sensor hal can do UI speed accelerometer streaming properly

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor hal shall do UI speed accelerometer streaming properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC16: AccelerometerStreamingOperationNormal

### Description

Test if sensor hal can do normal speed accelerometer streaming properly

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor hal shall do normal speed accelerometer streaming properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC17: AccelerometerStreamingOperationFast

### Description

Test if sensor hal can do game speed accelerometer streaming properly

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor hal shall do game speed accelerometer streaming properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC18: GyroscopeStreamingOperationSlow

### Description

Test if sensor hal can do UI speed gyroscope streaming properly

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor hal shall do UI speed gyroscope streaming properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC19: GyroscopeStreamingOperationNormal

### Description

Test if sensor hal can do normal speed gyroscope streaming properly

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor hal shall do normal speed gyroscope streaming properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC20: GyroscopeStreamingOperationFast

### Description

Test if sensor hal can do game speed gyroscope streaming properly

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor hal shall do game speed gyroscope streaming properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC21: AccelerometerSamplingPeriodHotSwitchOperation

### Description

Test if sensor hal can do accelerometer sampling rate switch properly when sensor is active

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor hal shall do accelerometer sampling rate switch properly when sensor is active

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC22: GyroscopeSamplingPeriodHotSwitchOperation

### Description

Test if sensor hal can do gyroscope sampling rate switch properly when sensor is active

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor hal shall do gyroscope sampling rate switch properly when sensor is active

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC23: AccelerometerBatchingOperation

### Description

Test if sensor hal can do accelerometer batching properly

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor hal shall do accelerometer batching properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC24: GyroscopeBatchingOperation

### Description

Test if sensor hal can do gyroscope batching properly

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor hal shall do gyroscope batching properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC25: AccelerometerAshmemDirectReportOperationNormal

### Description

Test sensor event direct report with ashmem for accel sensor at normal rate

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor event direct report with ashmem for accel sensor at normal rate shall work properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC26: AccelerometerAshmemDirectReportOperationFast

### Description

Test sensor event direct report with ashmem for accel sensor at fast rate

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor event direct report with ashmem for accel sensor at fast rate shall work properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC27: AccelerometerAshmemDirectReportOperationVeryFast

### Description

Test sensor event direct report with ashmem for accel sensor at very fast rate

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor event direct report with ashmem for accel sensor at very fast rate shall work properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC28: GyroscopeAshmemDirectReportOperationNormal

### Description

Test sensor event direct report with ashmem for gyro sensor at normal rate

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor event direct report with ashmem for gyro sensor at normal rate shall work proerly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC29: GyroscopeAshmemDirectReportOperationFast

### Description

Test sensor event direct report with ashmem for gyro sensor at fast rate

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor event direct report with ashmem for gyro sensor at fast rate shall work properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC30: GyroscopeAshmemDirectReportOperationVeryFast

### Description

Test sensor event direct report with ashmem for gyro sensor at very fast rate

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor event direct report with gralloc for accel sensor at normal rate shall work properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC31: AccelerometerGrallocDirectReportOperationNormal

### Description

Test sensor event direct report with gralloc for accel sensor at normal rate

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor event direct report with gralloc for accel sensor at normal rate shall work properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC32: AccelerometerGrallocDirectReportOperationFast

### Description

Test sensor event direct report with gralloc for accel sensor at fast rate

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor event direct report with gralloc for accel sensor at fast rate shall work properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC33: AccelerometerGrallocDirectReportOperationVeryFast

### Description

Test sensor event direct report with gralloc for accel sensor at very fast rate

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor event direct report with gralloc for accel sensor at very fast rate shall work properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC34: GyroscopeGrallocDirectReportOperationNormal

### Description

Test sensor event direct report with gralloc for gyro sensor at normal rate

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor event direct report with gralloc for gyro sensor at normal rate shall work properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC35: GyroscopeGrallocDirectReportOperationFast

### Description

Test sensor event direct report with gralloc for gyro sensor at fast rate

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor event direct report with gralloc for gyro sensor at fast rate shall work properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC36: GyroscopeGrallocDirectReportOperationVeryFast

### Description

Test sensor event direct report with gralloc for gyro sensor at very fast rate

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor event direct report with gralloc for gyro sensor at very fast rate shall work Properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC37: ConfigureDirectChannelWithInvalidHandle

### Description

Test configure direct channel with invalid handle

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor shall behaivor as expected

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC38: CleanupDirectConnectionOnInitialize

### Description

Test cleanup direct connection on initialize

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor shall behaivor as expected

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC39: SensorListDoesntContainInvalidType

### Description

Test sensor list doesnt contain invalid type

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor shall behaivor as expected

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC40: FlushNonexistentSensor

### Description

Test flush nonexistent sensor

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor shall behaivor as expected

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC41: AccelerometerCheckSensorVector

### Description

Test if accelerometer sensor vector is in range

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
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

## MEMS HIDL2X TC42: GyroscopeCheckSensorVector

### Description

Test if gyroscope sensor vector is in range

### Pre-Condition

Hardware and SPI connection to MEMS is set up properly
System started with MEMS IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
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

## MEMS HIDL2X TC43: GravityCheckSensorVector

### Description

Test if gravity sensor vector is in range

### Pre-Condition

Hardware and SPI connection to SMI 240 is set up properly
System started with SMI240 IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
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

## MEMS HIDL2X TC44: LinearAccelerationCheckSensorVector

### Description

Test if Linear acceleration sensor vector is in range

### Pre-Condition

Hardware and SPI connection to SMI 240 is set up properly
System started with SMI240 IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
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

## MEMS HIDL2X TC45: GravityStreamingOperationFast

### Description

Test if sensor hal can do game speed gravity streaming properly

### Pre-Condition

Hardware and SPI connection to SMI 240 is set up properly
System started with SMI240 IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor hal shall do game speed gravity streaming properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC46: LinearAccelerationStreamingOperationFast

### Description

Test if sensor hal can do game speed linear acceleration streaming properly

### Pre-Condition

Hardware and SPI connection to SMI 240 is set up properly
System started with SMI240 IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor hal shall do game speed linear acceleration streaming properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC47: GravitySamplingPeriodHotSwitchOperation

### Description

Test if sensor hal can do gravity sampling rate switch properly when sensor is active

### Pre-Condition

Hardware and SPI connection to SMI 240 is set up properly
System started with SMI240 IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor hal shall do gravity sampling rate switch properly when sensor is active

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC48: LinearAccelerationSamplingPeriodHotSwitchOperation

### Description

Test if sensor hal can do linear acceleration sampling rate switch properly when sensor is active

### Pre-Condition

Hardware and SPI connection to SMI 240 is set up properly
System started with SMI240 IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor hal shall do linear acceleration sampling rate switch properly when sensor is active

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC49: GravityBatchingOperation

### Description

Test if sensor hal can do gravity batching properly

### Pre-Condition

Hardware and SPI connection to SMI 240 is set up properly
System started with SMI240 IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor hal shall do gravity batching properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC50: LinearAccelerationBatchingOperation

### Description

Test if sensor hal can do linear acceleration batching properly

### Pre-Condition

Hardware and SPI connection to SMI 240 is set up properly
System started with SMI240 IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor hal shall do linear acceleration batching properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC51: GravityAshmemDirectReportOperationNormal

### Description

Test sensor event direct report with ashmem for gravity sensor at normal rate

### Pre-Condition

Hardware and SPI connection to SMI 240 is set up properly
System started with SMI240 IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor event direct report with ashmem for gravity sensor at normal rate shall work properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC52: GravityAshmemDirectReportOperationFast

### Description

Test sensor event direct report with ashmem for gravity sensor at fast rate

### Pre-Condition

Hardware and SPI connection to SMI 240 is set up properly
System started with SMI240 IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor event direct report with ashmem for gravity sensor at fast rate shall work properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC53: GravityAshmemDirectReportOperationVeryFast

### Description

Test sensor event direct report with ashmem for gravity sensor at very fast rate

### Pre-Condition

Hardware and SPI connection to SMI 240 is set up properly
System started with SMI240 IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor event direct report with ashmem for gravity sensor at very fast rate shall work properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC54: LinearAccelerationAshmemDirectReportOperationNormal

### Description

Test sensor event direct report with ashmem for linear accel sensor at normal rate

### Pre-Condition

Hardware and SPI connection to SMI 240 is set up properly
System started with SMI240 IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor event direct report with ashmem for linear accel sensor at normal rate shall work properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC55: LinearAccelerationAshmemDirectReportOperationFast

### Description

Test sensor event direct report with ashmem for linear accel sensor at fast rate

### Pre-Condition

Hardware and SPI connection to SMI 240 is set up properly
System started with SMI240 IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor event direct report with ashmem for linear accel sensor at fast rate shall work properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC56: LinearAccelerationAshmemDirectReportOperationVeryFast

### Description

Test sensor event direct report with ashmem for linear accel sensor at very fast rate

### Pre-Condition

Hardware and SPI connection to SMI 240 is set up properly
System started with SMI240 IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor event direct report with ashmem for linear accel sensor at very fast rate shall work properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC57: GravityGrallocDirectReportOperationNormal

### Description

Test sensor event direct report with gralloc for gravity sensor at normal rate

### Pre-Condition

Hardware and SPI connection to SMI 240 is set up properly
System started with SMI240 IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor event direct report with gralloc for gravity sensor at normal rate shall work properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC58: GravityGrallocDirectReportOperationFast

### Description

Test sensor event direct report with gralloc for gravity sensor at fast rate

### Pre-Condition

Hardware and SPI connection to SMI 240 is set up properly
System started with SMI240 IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor event direct report with gralloc for gravity sensor at fast rate shall work properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC59: GravityGrallocDirectReportOperationVeryFast

### Description

Test sensor event direct report with gralloc for gravity sensor at very fast rate

### Pre-Condition

Hardware and SPI connection to SMI 240 is set up properly
System started with SMI240 IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor event direct report with gralloc for gravity sensor at very fast rate shall work properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC60: LinearAccelerationGrallocDirectReportOperationNormal

### Description

Test sensor event direct report with gralloc for linear accel sensor at normal rate

### Pre-Condition

Hardware and SPI connection to SMI 240 is set up properly
System started with SMI240 IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor event direct report with gralloc for linear accel sensor at normal rate shall work properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC61: LinearAccelerationGrallocDirectReportOperationFast

### Description

Test sensor event direct report with gralloc for linear accel sensor at fast rate

### Pre-Condition

Hardware and SPI connection to SMI 240 is set up properly
System started with SMI240 IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor event direct report with gralloc for linear accel sensor at fast rate shall work properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

## MEMS HIDL2X TC62: LinearAccelerationGrallocDirectReportOperationVeryFast

### Description

Test sensor event direct report with gralloc for linear accel sensor at very fast rate

### Pre-Condition

Hardware and SPI connection to SMI 240 is set up properly
System started with SMI240 IIO Linux Driver
System started with Sensor HAL HIDL v2.X Configuration
VTS tests implementation is built properly
ADB connection to Host (Raspberry Pi) works properly

### Platform

Raspberry Pi

### Test Procedure

Connect to Raspberry pi via ADB
invoke VTS command to execute the tests

### Expected Results

Sensor event direct report with gralloc for linear accel sensor at very fast rate shall work properly

### Test Result

| Pass/Fail | Remarks |
|-----------|---------|
| **pass** |  |

