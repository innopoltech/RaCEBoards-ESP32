import struct
from time import sleep
from busio import I2C
from I2C_SPI_protocol_Base import I2C_Impl

LSM6DSL_DEFAULT_ADDRESS = 0x6A

LSM6DSL_CHIP_ID = 0x6A

_LSM6DSL_WHOAMI = 0xF
_LSM6DSL_CTRL1_XL = 0x10
_LSM6DSL_CTRL2_G = 0x11
_LSM6DSL_CTRL3_C = 0x12

_LSM6DSL_OUT_TEMP_L = 0x20
_LSM6DSL_OUT_TEMP_H = 0x21

_MILLI_G_TO_ACCEL = 0.00980665
_TEMPERATURE_SENSITIVITY = 256
_TEMPERATURE_OFFSET = 25.0

_LSM6DSL_OUTX_L_G = 0x22
_LSM6DSL_OUTX_H_G = 0x23
_LSM6DSL_OUTY_L_G = 0x24
_LSM6DSL_OUTY_H_G = 0x25
_LSM6DSL_OUTZ_L_G = 0x26
_LSM6DSL_OUTZ_H_G = 0x27

_LSM6DSL_OUTX_L_XL = 0x28
_LSM6DSL_OUTX_H_XL = 0x29
_LSM6DSL_OUTY_L_XL = 0x2A
_LSM6DSL_OUTY_H_XL = 0x2B
_LSM6DSL_OUTZ_L_XL = 0x2C
_LSM6DSL_OUTZ_H_XL = 0x2D

Rate = {
        "RATE_SHUTDOWN":    ( 0, 0),
        "RATE_12_5_HZ":     ( 1, 12.5),
        "RATE_26_HZ":       ( 2, 26.0),
        "RATE_52_HZ":       ( 3, 52.0),
        "RATE_104_HZ":      ( 4, 104.0),
        "RATE_208_HZ":      ( 5, 208.0),
        "RATE_416_HZ":      ( 6, 416.0),
        "RATE_833_HZ":      ( 7, 833.0),
        "RATE_1_66K_HZ":    ( 8, 1666.0),
        "RATE_3_33K_HZ":    ( 9, 3332.0),
        "RATE_6_66K_HZ":    ( 10, 6664.0),
        "RATE_1_6_HZ":      ( 11, 1.6),
}

AccelHPF = {
        "SLOPE":            ( 0, 0),
        "HPF_DIV100":       ( 1, 0),
        "HPF_DIV9":         ( 2, 0),
        "HPF_DIV400":       ( 3, 0),
    }

GyroRange = {
       # "RANGE_125_DPS":       ( 125, 125, 4.375),
        "RANGE_250_DPS":       ( 0, 250, 8.75),
        "RANGE_500_DPS":       ( 1, 500, 17.50),
        "RANGE_1000_DPS":      ( 2, 1000, 35.0),
        "RANGE_2000_DPS":      ( 3, 2000, 70.0),
}

AccelRange = {
        "RANGE_2G":       ( 0, 2, 0.061),
        "RANGE_16G":      ( 1, 16, 0.488),
        "RANGE_4G":       ( 2, 4, 0.122),
        "RANGE_8G":       ( 3, 8, 0.244),
}

class LSM6DSL:
    def __init__(self, bus_implementation: I2C_Impl) -> None:
        self._bus_implementation = bus_implementation

        self._chip_id = self._read_byte(_LSM6DSL_WHOAMI)
        if self._chip_id != LSM6DSL_CHIP_ID:
            raise RuntimeError(
                "Failed to find %s - check your wiring!" % self.__class__.__name__
            )

        self.reset()

        buf = self._read_byte(_LSM6DSL_CTRL3_C)
        buf |= 0x40                                 #BDU enable
        self._write_register_byte(_LSM6DSL_CTRL3_C, buf)

        buf = self._read_byte(_LSM6DSL_CTRL1_XL)
        buf &= ~0xFC;                               #Reset acc odr, range
        buf |= (Rate["RATE_104_HZ"][0]<<4)          #Set acc odr
        buf |= (AccelRange["RANGE_16G"][0]<<2)      #Set acc range
        self._write_register_byte(_LSM6DSL_CTRL1_XL, buf)

        buf = self._read_byte(_LSM6DSL_CTRL2_G)
        buf &= ~0xFC;                               #Reset gyro odr, range
        buf |= (Rate["RATE_104_HZ"][0]<<4)          #Set gyro odr
        buf |= (GyroRange["RANGE_2000_DPS"][0]<<2)  #Set acc range
        self._write_register_byte(_LSM6DSL_CTRL2_G, buf)

        self._cached_accel_range = AccelRange["RANGE_16G"][2]
        self._cached_gyro_range  = GyroRange["RANGE_2000_DPS"][2]

    def reset(self) -> None:
        "Resets the sensor's configuration into an initial state"
        buf = self._read_byte(_LSM6DSL_CTRL3_C)
        buf |= 1
        self._write_register_byte(_LSM6DSL_CTRL3_C, buf)
        while self._read_byte(_LSM6DSL_CTRL3_C) & 1:
            sleep(0.001)

    def acceleration(self):
        """The x, y, z acceleration values returned in a 3-tuple and are in m/s^2."""
        raw_accel_data = [0,0,0]

        l = self._read_byte(_LSM6DSL_OUTX_L_XL)
        h = self._read_byte(_LSM6DSL_OUTX_H_XL)
        raw_accel_data[0] =  struct.unpack('<h', bytes([l, h]))[0]

        l = self._read_byte(_LSM6DSL_OUTY_L_XL)
        h = self._read_byte(_LSM6DSL_OUTY_H_XL)
        raw_accel_data[1] =  struct.unpack('<h', bytes([l, h]))[0]

        l = self._read_byte(_LSM6DSL_OUTZ_L_XL)
        h = self._read_byte(_LSM6DSL_OUTZ_H_XL)
        raw_accel_data[2] =  struct.unpack('<h', bytes([l, h]))[0]

        x = self._scale_xl_data(raw_accel_data[0])
        y = self._scale_xl_data(raw_accel_data[1])
        z = self._scale_xl_data(raw_accel_data[2])

        return (x, y, z)

    def _scale_xl_data(self, raw_measurement: int) -> float:
        return (raw_measurement * self._cached_accel_range * _MILLI_G_TO_ACCEL)

    def gyro(self):
        raw_gyro_data = [0,0,0]

        l = self._read_byte(_LSM6DSL_OUTX_L_G)
        h = self._read_byte(_LSM6DSL_OUTX_H_G)
        raw_gyro_data[0] =  struct.unpack('<h', bytes([l, h]))[0]

        l = self._read_byte(_LSM6DSL_OUTY_L_G)
        h = self._read_byte(_LSM6DSL_OUTY_H_G)
        raw_gyro_data[1] =  struct.unpack('<h', bytes([l, h]))[0]

        l = self._read_byte(_LSM6DSL_OUTZ_L_G)
        h = self._read_byte(_LSM6DSL_OUTZ_H_G)
        raw_gyro_data[2] =  struct.unpack('<h', bytes([l, h]))[0]

        x = self._scale_gyro_data(raw_gyro_data[0])
        y = self._scale_gyro_data(raw_gyro_data[1])
        z = self._scale_gyro_data(raw_gyro_data[2])

        return (x, y, z)

    def _scale_gyro_data(self, raw_measurement: int) -> float:
        return raw_measurement * self._cached_gyro_range / 1000.0


    def temperature(self) -> float:
        temp = [0]

        l = self._read_byte(_LSM6DSL_OUT_TEMP_L)
        h = self._read_byte(_LSM6DSL_OUT_TEMP_H)
        temp[0] =  struct.unpack('<h', bytes([l, h]))[0]

        return temp[0] / _TEMPERATURE_SENSITIVITY + _TEMPERATURE_OFFSET


    def _read_byte(self, register: int) -> int:
        """Read a byte register value and return it"""
        return self._read_register(register, 1)[0]
    
    def _read_register(self, register: int, length: int) -> bytearray:
        return self._bus_implementation.read_register(register, length)
    
    def _write_register_byte(self, register: int, value: int) -> None:
        self._bus_implementation.write_register_byte(register, value)


class LSM6DSL_I2C(LSM6DSL):
    def __init__(self, i2c: I2C, address: int = 0x6A) -> None:  # LSM6DSL_ADDRESS
        super().__init__(I2C_Impl(i2c, address))
