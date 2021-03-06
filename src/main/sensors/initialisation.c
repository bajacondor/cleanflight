/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform.h"
#include "common/axis.h"

#include "drivers/accgyro.h"

#include "drivers/accgyro_adxl345.h"
#include "drivers/accgyro_bma280.h"
#include "drivers/accgyro_l3g4200d.h"
#include "drivers/accgyro_mma845x.h"
#include "drivers/accgyro_mpu3050.h"
#include "drivers/accgyro_mpu6050.h"
#ifdef STM32F3DISCOVERY
#include "drivers/accgyro_l3gd20.h"
#include "drivers/accgyro_lsm303dlhc.h"
#endif
#ifdef CC3D
#include "drivers/accgyro_spi_mpu6000.h"
#endif

#include "drivers/barometer.h"
#include "drivers/barometer_bmp085.h"
#include "drivers/barometer_ms5611.h"
#include "drivers/compass_hmc5883l.h"
#include "drivers/sonar_hcsr04.h"
#include "drivers/system.h"

#include "flight/flight.h"
#include "config/runtime_config.h"

#include "sensors/sensors.h"
#include "sensors/acceleration.h"
#include "sensors/barometer.h"
#include "sensors/gyro.h"
#include "sensors/compass.h"
#include "sensors/sonar.h"


// Use these to help with porting to new boards
//#define USE_FAKE_GYRO
#define USE_GYRO_L3G4200D
#define USE_GYRO_L3GD20
#define USE_GYRO_MPU6050
#define USE_GYRO_SPI_MPU6000
#define USE_GYRO_MPU3050
//#define USE_FAKE_ACC
#define USE_ACC_ADXL345
#define USE_ACC_BMA280
#define USE_ACC_MMA8452
#define USE_ACC_LSM303DLHC
#define USE_ACC_MPU6050
#define USE_ACC_SPI_MPU6000
#define USE_BARO_MS5611
#define USE_BARO_BMP085

#ifdef NAZE
#undef USE_ACC_LSM303DLHC
#undef USE_ACC_SPI_MPU6000
#undef USE_GYRO_SPI_MPU6000
#undef USE_GYRO_L3GD20
#endif

#ifdef NAZE32PRO
#define USE_FAKE_ACC
#define USE_FAKE_GYRO
#undef USE_ACC_LSM303DLHC
#undef USE_ACC_ADXL345
#undef USE_ACC_BMA280
#undef USE_ACC_MMA8452
#undef USE_ACC_LSM303DLHC
#undef USE_ACC_MPU6050
#undef USE_ACC_SPI_MPU6000
#undef USE_GYRO_L3G4200D
#undef USE_GYRO_L3GD20
#undef USE_GYRO_MPU3050
#undef USE_GYRO_MPU6050
#undef USE_GYRO_SPI_MPU6000
#endif

#if defined(OLIMEXINO)
#undef USE_GYRO_L3GD20
#undef USE_GYRO_L3G4200D
#undef USE_GYRO_MPU3050
#undef USE_GYRO_SPI_MPU6000
#undef USE_ACC_LSM303DLHC
#undef USE_ACC_BMA280
#undef USE_ACC_MMA8452
#undef USE_ACC_ADXL345
#undef USE_ACC_SPI_MPU6000
#undef USE_BARO_MS5611
#endif

#ifdef STM32F3DISCOVERY
#undef USE_ACC_SPI_MPU6000
#undef USE_GYRO_SPI_MPU6000
#endif

#ifdef CHEBUZZF3
#undef USE_GYRO_L3G4200D
#undef USE_GYRO_MPU6050
#undef USE_GYRO_MPU3050
#undef USE_GYRO_SPI_MPU6000
#undef USE_ACC_ADXL345
#undef USE_ACC_BMA280
#undef USE_ACC_MPU6050
#undef USE_ACC_MMA8452
#undef USE_ACC_SPI_MPU6000
#endif

#ifdef CC3D
#undef USE_GYRO_L3GD20
#undef USE_GYRO_L3G4200D
#undef USE_GYRO_MPU6050
#undef USE_GYRO_MPU3050
#undef USE_ACC_LSM303DLHC
#undef USE_ACC_ADXL345
#undef USE_ACC_BMA280
#undef USE_ACC_MPU6050
#undef USE_ACC_MMA8452
#endif

#ifdef CJMCU
#undef USE_GYRO_SPI_MPU6000
#undef USE_GYRO_L3GD20
#undef USE_GYRO_L3G4200D
#undef USE_GYRO_MPU3050
#undef USE_ACC_LSM303DLHC
#undef USE_ACC_SPI_MPU6000
#undef USE_ACC_ADXL345
#undef USE_ACC_BMA280
#undef USE_ACC_MMA8452
#endif

extern uint16_t batteryWarningVoltage;
extern uint8_t batteryCellCount;
extern float magneticDeclination;

extern gyro_t gyro;
extern baro_t baro;
extern acc_t acc;

#ifdef USE_FAKE_GYRO
static void fakeGyroInit(void) {}
static void fakeGyroRead(int16_t *gyroData) {}
static void fakeGyroReadTemp(int16_t *tempData) {}

bool fakeGyroDetect(gyro_t *gyro, uint16_t lpf)
{
    gyro->init = fakeGyroInit;
    gyro->read = fakeGyroRead;
    gyro->temperature = fakeGyroReadTemp;
    return true;
}
#endif

#ifdef USE_FAKE_ACC
static void fakeAccInit(void) {}
static void fakeAccRead(int16_t *accData) {}

bool fakeAccDetect(acc_t *acc)
{
    acc->init = fakeAccInit;
    acc->read = fakeAccRead;
    acc->revisionCode = 0;
    return true;
}
#endif

bool detectGyro(uint16_t gyroLpf)
{
    gyroAlign = ALIGN_DEFAULT;
#ifdef USE_FAKE_GYRO
    if (fakeGyroDetect(&gyro, gyroLpf)) {
        return true;
    }
#endif

#ifdef USE_GYRO_MPU6050
    if (mpu6050GyroDetect(&gyro, gyroLpf)) {
#ifdef NAZE
        gyroAlign = CW0_DEG;
#endif
        return true;
    }
#endif

#ifdef USE_GYRO_L3G4200D
    if (l3g4200dDetect(&gyro, gyroLpf)) {
#ifdef NAZE
        gyroAlign = CW0_DEG;
#endif
        return true;
    }
#endif

#ifdef USE_GYRO_MPU3050
    if (mpu3050Detect(&gyro, gyroLpf)) {
#ifdef NAZE
        gyroAlign = CW0_DEG;
#endif
        return true;
    }
#endif

#ifdef USE_GYRO_L3GD20
    if (l3gd20Detect(&gyro, gyroLpf)) {
        return true;
    }
#endif

#ifdef USE_GYRO_SPI_MPU6000
    if (mpu6000SpiGyroDetect(&gyro, gyroLpf)) {
#ifdef CC3D
        gyroAlign = CW270_DEG;
#endif
        return true;
    }
#endif
    return false;
}

static void detectAcc(uint8_t accHardwareToUse)
{
#ifdef USE_ACC_ADXL345
    drv_adxl345_config_t acc_params;
#endif

retry:
    accAlign = ALIGN_DEFAULT;

    switch (accHardwareToUse) {
#ifdef USE_FAKE_ACC
        default:
            if (fakeAccDetect(&acc)) {
                accHardware = ACC_FAKE;
                if (accHardwareToUse == ACC_FAKE)
                    break;
            }
#endif
        case ACC_NONE: // disable ACC
            sensorsClear(SENSOR_ACC);
            break;
        case ACC_DEFAULT: // autodetect
#ifdef USE_ACC_ADXL345
        case ACC_ADXL345: // ADXL345
            acc_params.useFifo = false;
            acc_params.dataRate = 800; // unused currently
            if (adxl345Detect(&acc_params, &acc)) {
                accHardware = ACC_ADXL345;
#ifdef NAZE
                accAlign = CW270_DEG;
#endif
            }
            if (accHardwareToUse == ACC_ADXL345)
                break;
            ; // fallthrough
#endif
#ifdef USE_ACC_MPU6050
        case ACC_MPU6050: // MPU6050
            if (mpu6050AccDetect(&acc)) {
                accHardware = ACC_MPU6050;
#ifdef NAZE
                accAlign = CW0_DEG;
#endif
                if (accHardwareToUse == ACC_MPU6050)
                    break;
            }
            ; // fallthrough
#endif
#ifdef USE_ACC_MMA8452
        case ACC_MMA8452: // MMA8452
            if (mma8452Detect(&acc)) {
                accHardware = ACC_MMA8452;
#ifdef NAZE
                accAlign = CW90_DEG;
#endif
                if (accHardwareToUse == ACC_MMA8452)
                    break;
            }
            ; // fallthrough
#endif
#ifdef USE_ACC_BMA280
        case ACC_BMA280: // BMA280
            if (bma280Detect(&acc)) {
                accHardware = ACC_BMA280;
#ifdef NAZE
                accAlign = CW0_DEG;
#endif
                if (accHardwareToUse == ACC_BMA280)
                    break;
            }
            ; // fallthrough
#endif
#ifdef USE_ACC_LSM303DLHC
        case ACC_LSM303DLHC:
            if (lsm303dlhcAccDetect(&acc)) {
                accHardware = ACC_LSM303DLHC;
                if (accHardwareToUse == ACC_LSM303DLHC)
                    break;
            }
            ; // fallthrough
#endif
#ifdef USE_GYRO_SPI_MPU6000
        case ACC_SPI_MPU6000:
            if (mpu6000SpiAccDetect(&acc)) {
                accHardware = ACC_SPI_MPU6000;
#ifdef CC3D
                accAlign = CW270_DEG;
#endif
                if (accHardwareToUse == ACC_SPI_MPU6000)
                    break;
            }
            ; // fallthrough
#endif
            ; // prevent compiler error
    }

    // Found anything? Check if user fucked up or ACC is really missing.
    if (accHardware == ACC_DEFAULT) {
        if (accHardwareToUse > ACC_DEFAULT) {
            // Nothing was found and we have a forced sensor type. Stupid user probably chose a sensor that isn't present.
            accHardwareToUse = ACC_DEFAULT;
            goto retry;
        } else {
            // No ACC was detected
            sensorsClear(SENSOR_ACC);
        }
    }
}

static void detectBaro()
{
#ifdef BARO
#ifdef USE_BARO_MS5611
    // Detect what pressure sensors are available. baro->update() is set to sensor-specific update function
    if (ms5611Detect(&baro)) {
        return;
    }
#endif

#ifdef USE_BARO_BMP085
    // ms5611 disables BMP085, and tries to initialize + check PROM crc. if this works, we have a baro
    if (bmp085Detect(&baro)) {
        return;
    }
#endif
    sensorsClear(SENSOR_BARO);
#endif
}

void reconfigureAlignment(sensorAlignmentConfig_t *sensorAlignmentConfig)
{
    if (sensorAlignmentConfig->gyro_align != ALIGN_DEFAULT) {
        gyroAlign = sensorAlignmentConfig->gyro_align;
    }
    if (sensorAlignmentConfig->acc_align != ALIGN_DEFAULT) {
        accAlign = sensorAlignmentConfig->acc_align;
    }
    if (sensorAlignmentConfig->mag_align != ALIGN_DEFAULT) {
        magAlign = sensorAlignmentConfig->mag_align;
    }
}

bool sensorsAutodetect(sensorAlignmentConfig_t *sensorAlignmentConfig, uint16_t gyroLpf, uint8_t accHardwareToUse, int16_t magDeclinationFromConfig)
{
    int16_t deg, min;
    memset(&acc, sizeof(acc), 0);
    memset(&gyro, sizeof(gyro), 0);

    if (!detectGyro(gyroLpf)) {
        return false;
    }
    detectAcc(accHardwareToUse);
    detectBaro();

    reconfigureAlignment(sensorAlignmentConfig);

    // Now time to init things, acc first
    if (sensors(SENSOR_ACC))
        acc.init();
    // this is safe because either mpu6050 or mpu3050 or lg3d20 sets it, and in case of fail, we never get here.
    gyro.init();

#ifdef MAG
    if (hmc5883lDetect()) {
        magAlign = CW180_DEG; // default NAZE alignment
    } else {
        sensorsClear(SENSOR_MAG);
    }
#endif

    // FIXME extract to a method to reduce dependencies, maybe move to sensors_compass.c
    if (sensors(SENSOR_MAG)) {
        // calculate magnetic declination
        deg = magDeclinationFromConfig / 100;
        min = magDeclinationFromConfig % 100;

        magneticDeclination = (deg + ((float)min * (1.0f / 60.0f))) * 10; // heading is in 0.1deg units
    } else {
        magneticDeclination = 0.0f; // TODO investigate if this is actually needed if there is no mag sensor or if the value stored in the config should be used.
    }

    return true;
}

