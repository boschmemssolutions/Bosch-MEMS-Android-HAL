# Bosch MEMS Android HAL – Integration Guide

Comprehensive step‑by‑step instructions to integrate Bosch MEMS sensor kernel drivers and the Bosch Sensors HAL into an Android platform build and verify runtime functionality.

---

## Table of Contents

1. [Prerequisites](#1-prerequisites)
2. [Obtain Source Trees](#2-obtain-source-trees)
3. [Kernel Integration](#3-kernel-integration)
4. [Device / Policy Configuration](#4-device--policy-configuration)
5. [Runtime Configuration & Properties](#5-runtime-configuration--properties)
6. [Platform (HAL) Integration](#6-platform-hal-integration)
7. [Verification & Usage](#7-verification--usage)
8. [Troubleshooting](#8-troubleshooting)

---

## 1. Prerequisites

### 1.1 Build Environment
* Ubuntu 22.04 (recommended) with Android build prereqs per [AOSP docs](https://source.android.com/setup/build/initializing)
* RAM ≥ 32 GB (>= 64 GB recommended), Disk ≥ 400 GB free
* Cross compiler toolchain (example: `aarch64-linux-gnu-`)
* Stable network connection for large repo sync

### 1.2 Hardware
* Target board (e.g., Raspberry Pi 4, other ARM64 dev board)
* Bosch sensors (SMI230 / SMI240 / SMI330) connected via SPI or I2C + GPIO interrupt lines
> ⚠️ **Warning**: SMI230 Data Sync mode is not supported.

### 1.3 Software
* Linux kernel common tree (example branch `common-android15-6.6`)
* Android platform tree (example tag `android-15.0.0_r32`)
* Bosch sensor Linux driver sources (Industrial I/O compliant)
* Bosch Android Sensor HAL sources (this repository)

---

## 2. Obtain Source Trees

Create two separate working directories: one for kernel, one for platform.

Kernel (example):
```bash
repo init -u https://android.googlesource.com/kernel/manifest -b common-android15-6.6
repo sync
```

Platform (example Raspberry Pi manifest layering):
```bash
repo init -u https://android.googlesource.com/platform/manifest -b android-15.0.0_r32 --depth=1
curl -o .repo/local_manifests/manifest_brcm_rpi.xml -L \
  https://raw.githubusercontent.com/raspberry-vanilla/android_local_manifest/android-15.0/manifest_brcm_rpi.xml --create-dirs
curl -o .repo/local_manifests/remove_projects.xml -L \
  https://raw.githubusercontent.com/raspberry-vanilla/android_local_manifest/android-15.0/remove_projects.xml
repo sync
```

---

## 3. Kernel Integration

### 3.1 Add Bosch Driver
1. Copy driver directory (e.g. `smi330`) into `drivers/iio/imu/`.
2. Edit `drivers/iio/imu/Kconfig` -> add: `source "drivers/iio/imu/smi330/Kconfig"`
3. Edit `drivers/iio/imu/Makefile` -> add: `obj-y += smi330/`

### 3.2 Enable via Configuration
Load the standard kernel configuration for your hardware. Dependent on the HW, you can find the standard configuration under build.config.xxx. Example: for the Qualcomm db845c board, the standard configuration is db845c_gki_defconfig defined in build.config.db845c.

```bash
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- <board>_gki_defconfig
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- menuconfig
```
Enable:
```
Device Drivers  --->
  Industrial I/O support  --->
    Inertial measurement units  --->
      <*> Bosch SMIxxx driver
    Triggers - standalone  --->
      <*> High resolution timer trigger
```

> 📝 **Note**: The `High resolution timer trigger` (`CONFIG_IIO_HRTIMER_TRIGGER`) is required if you plan to use `sw_timer` scan type mode. This is mandatory for SMI240 and optional for SMI230/SMI330.

### 3.3 Device Tree (Examples)
SPI:
```dts
imu@0 {
    compatible = "bosch,smi330";
    reg = <0>;
    spi-max-frequency = <10000000>;
    interrupt-parent = <&gpio>;
    interrupts = <26 IRQ_TYPE_EDGE_RISING>, <20 IRQ_TYPE_EDGE_RISING>;
    interrupt-names = "INT1", "INT2";
};
```
I2C:
```dts
imu@68 {
    compatible = "bosch,smi330";
    reg = <0x68>;
    vddio-supply = <&vddio>;
    vdd-supply = <&vdd>;
    interrupt-parent = <&gpio>;
    interrupts = <26 IRQ_TYPE_EDGE_RISING>;
    interrupt-names = "INT1";
};
```
Adjust GPIO numbers, interrupt flags, and bus addresses for your hardware. Consult your board vendor if unsure.

### 3.4 Build Kernel
```bash
make -j$(nproc) ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- Image modules dtbs
```

### 3.5 Provide Kernel to Platform
Two options:
1. Export environment variable before platform build:
   ```bash
   export TARGET_PREBUILT_KERNEL="/absolute/path/to/arch/arm64/boot/Image"
   ```
2. Or replace the prebuilt referenced in `device/<vendor>/<device>/device.mk` with your new built kernel.

---

## 4. Device / Policy Configuration

### 4.1 SELinux File Contexts
Append to `$(ANDROID_ROOT)/device/<vendor>/<device>/sepolicy/file_contexts`:
```bash
# Bosch Sensor HAL (AIDL only)
/vendor/bin/hw/android\.hardware\.sensors@aidl-service\.bosch            u:object_r:hal_sensors_default_exec:s0
/vendor/bin/hw/android\.hardware\.sensors@aidl-service\.multihal\.bosch  u:object_r:hal_sensors_default_exec:s0
```

### 4.2 SELinux Policy Rules
Append to `$(ANDROID_ROOT)/device/<vendor>/<device>/sepolicy/hal_sensors.te`:
```te
# Allow hal_sensors_default to create IIO hrtimer triggers in configfs
allow hal_sensors_default configfs:dir { add_name create };
allow hal_sensors_default configfs:dir { write remove_name };
allow hal_sensors_default configfs:file { create write open read getattr setattr };

# Allow hal_sensors_default access to sysfs device iio
allow hal_sensors_default sysfs:dir { open read };
allow hal_sensors_default sysfs:file { open read write getattr };
allow hal_sensors_default sensors_device:chr_file rw_file_perms;
```

These policy rules grant the sensor HAL the necessary permissions to:
- Create and manage IIO hrtimer triggers in configfs for timer-based sampling modes
- Read and write sysfs attributes for IIO sensor device configuration
- Access IIO device character files for reading sensor data

### 4.3 UEvent Rules (Permissions to IIO)
Append to `$(ANDROID_ROOT)/device/<vendor>/<device>/ramdisk/ueventd.rc`:
```plaintext
# Bosch IIO Sensor Permissions
/dev/iio:device*                                                  0666 system system
/sys/bus/iio/devices/iio:device* in_accel_en                      0666 system system
/sys/bus/iio/devices/iio:device* in_anglvel_en                    0666 system system
/sys/bus/iio/devices/iio:device* in_sampling_frequency            0666 system system
/sys/bus/iio/devices/iio:device* in_accel_sampling_frequency      0666 system system
/sys/bus/iio/devices/iio:device* in_anglvel_sampling_frequency    0666 system system
/sys/bus/iio/devices/iio:device* in_accel_scale                   0666 system system
/sys/bus/iio/devices/iio:device* in_anglvel_scale                 0666 system system
/sys/bus/iio/devices/iio:device* trigger/current_trigger          0666 system system
/sys/bus/iio/devices/trigger*    sampling_frequency               0666 system system

# Buffer controls
/sys/bus/iio/devices/iio:device* buffer0/enable                   0666 system system
/sys/bus/iio/devices/iio:device* buffer0/length                   0666 system system
/sys/bus/iio/devices/iio:device* buffer0/watermark                0666 system system
/sys/bus/iio/devices/iio:device* buffer0/in_accel_x_en            0666 system system
/sys/bus/iio/devices/iio:device* buffer0/in_accel_y_en            0666 system system
/sys/bus/iio/devices/iio:device* buffer0/in_accel_z_en            0666 system system
/sys/bus/iio/devices/iio:device* buffer0/in_anglvel_x_en          0666 system system
/sys/bus/iio/devices/iio:device* buffer0/in_anglvel_y_en          0666 system system
/sys/bus/iio/devices/iio:device* buffer0/in_anglvel_z_en          0666 system system
/sys/bus/iio/devices/iio:device* buffer0/in_timestamp_en          0666 system system
# SMI230-specific attributes
/sys/bus/iio/devices/iio:device* pwr                              0666 system system
/sys/bus/iio/devices/iio:device* odr                              0666 system system
/sys/bus/iio/devices/iio:device* bw_odr                           0666 system system
/sys/bus/iio/devices/iio:device* range                            0666 system system
```

### 4.4 Init Script
Append to `$(ANDROID_ROOT)/device/<vendor>/<device>/ramdisk/init.rc`:
```plaintext
on post-fs
    # Change ownership so the Sensor HAL group (usually 'system') can write
    chown system system /config/iio/triggers/hrtimer
    chmod 0775 /config/iio/triggers/hrtimer
```

---

## 5. Runtime Configuration & Properties

Adapt your vendor property makefiles. Example values:
```properties
# SMI230
persist.vendor.bosch.sensor.smi230.rotation_matrix.config = 1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0
persist.vendor.bosch.sensor.smi230.location_vector.config = 0.0,0.0,0.0
persist.vendor.bosch.sensor.smi230.scan_type.config = drdy
persist.vendor.bosch.sensor.smi230.acc_range.config = 4
persist.vendor.bosch.sensor.smi230.gyro_range.config = 2000

# SMI240
persist.vendor.bosch.sensor.smi240.rotation_matrix.config = 1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0
persist.vendor.bosch.sensor.smi240.location_vector.config = 0.0,0.0,0.0
persist.vendor.bosch.sensor.smi240.scan_type.config = sw_timer

# SMI330
persist.vendor.bosch.sensor.smi330.rotation_matrix.config = 1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0
persist.vendor.bosch.sensor.smi330.location_vector.config = 0.0,0.0,0.0
persist.vendor.bosch.sensor.smi330.scan_type.config = drdy
persist.vendor.bosch.sensor.smi330.acc_range.config = 8
persist.vendor.bosch.sensor.smi330.gyro_range.config = 250
```

> 📝 **Note**: SMI240 does not support configurable range properties as its measurement ranges are hardware-dependent.

> ⚠️ **Warning**: Changing the scan type requires a power cycle for the sensor to work correctly.

**Property Descriptions:**
- `rotation_matrix.config`: 3x3 rotation matrix to align sensor coordinate system with device
- `location_vector.config`: X,Y,Z position vector relative to device center
- `scan_type.config`: Data acquisition method 
  - `drdy` - Data ready interrupt mode (supported by SMI230, SMI330)
  - `hw_fifo` - Hardware FIFO buffer mode (supported by SMI230, SMI330)
  - `sw_timer` - Software timer polling mode (supported by all sensors)
- `acc_range.config`: Accelerometer measurement range in g-force
  - **SMI230/SMI330**: Valid values are `2`, `4`, `8`, `16` (±2g, ±4g, ±8g, ±16g)
  - **SMI240**: Not configurable (hardware dependent, fixed at ±16g)
- `gyro_range.config`: Gyroscope measurement range in degrees per second
  - **SMI230**: Valid values are `125`, `250`, `500`, `1000`, `2000` (±125°/s, ±250°/s, ±500°/s, ±1000°/s, ±2000°/s)
  - **SMI330**: Valid values are `125`, `250`, `500` (±125°/s, ±250°/s, ±500°/s)
  - **SMI240**: Not configurable (hardware dependent, fixed at ±300°/s)

---

## 6. Platform (HAL) Integration

### 6.1 Copy HAL Sources
Create folder:
```
$(ANDROID_ROOT)/hardware/bosch/sensors
```
Copy the Bosch HAL source contents there (core, hwctl, sensors, multihal, aidl directories etc.).

### 6.2 Choose HAL Variant (device.mk)
Add exactly one of the following blocks depending on your needs:
```makefile
# AIDL Single-HAL
PRODUCT_PACKAGES += android.hardware.sensors@aidl-service.bosch

# AIDL Multi-HAL
PRODUCT_PACKAGES += android.hardware.sensors@aidl-service.multihal.bosch
PRODUCT_COPY_FILES += \
  hardware/bosch/sensors/multihal/common/hals.conf:$(TARGET_COPY_OUT_VENDOR)/etc/sensors/hals.conf
```
> **Note**: This HAL implementation is designed for Android 15 and only supports AIDL interfaces.

### 6.3 Build 
```bash
source build/envsetup.sh
lunch <your-lunch-target>

# Build images containing the HAL service
make bootimage systemimage vendorimage
```

Check binary output (example path for rpi4):
```
out/target/product/rpi4/vendor/bin/hw/
```
Expect: `android.hardware.sensors@aidl-service.bosch` or `android.hardware.sensors@aidl-service.multihal.bosch`

---

## 7. Verification & Usage

After boot, verify:
```bash
adb shell service list | grep -i sensor
adb shell getprop | grep -i bosch
adb shell ls /sys/bus/iio/devices/
adb logcat -d | grep -i bosch
```

Typical log snippet indicates registration:
```
AddSensor[0] SMI330 BOSCH Accelerometer Sensor
```

List sensors via framework:
```bash
adb shell dumpsys sensorservice | grep -i bosch -A3
```

---

## 8. Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|--------------|-----|
| HAL binary missing under `vendor/bin/hw` | Not included in `PRODUCT_PACKAGES` or build failed | Re-check `device.mk`, rebuild modules/images |
| `service list` shows no sensor service | SELinux denial or binary crash | Inspect `logcat` for `avc: denied`; confirm file_contexts entries |
| Permission denied reading `/sys/bus/iio/...` | Missing `ueventd.rc` rules | Add rules and rebuild ramdisk / vendor |
| No IIO devices appear | Driver not loaded or DT wrong | Validate kernel config, dmesg for probe errors, verify DT addresses |
| Wrong / zero sensor values | Incorrect rotation matrix or range | Adjust properties, reboot (or setprop + restart service if supported) |
| Sensor fails with scan type error | Kernel mode incompatible with HAL scan type | SMI240 requires `sw_timer`, check kernel driver operating mode matches scan type |
| No sensor data in `hw_fifo` or `drdy` mode | Missing interrupt configuration | Verify device tree interrupt lines, check kernel driver compiled with interrupt support |

Quick SELinux check:
```bash
adb logcat | grep -i "avc: denied" | grep sensors
```

---

## Notes
* This HAL implementation is designed specifically for Android 15 and only supports AIDL interfaces.
* Re-run full images after SELinux or uevent permission changes; incremental pushes may miss ramdisk updates.
* Scan type `hw_fifo` provides the best power efficiency through hardware batching.
* Scan type `drdy` provides the lowest latency for real-time applications.
* SMI240 does not support hardware interrupts and must use `sw_timer` mode.

---

