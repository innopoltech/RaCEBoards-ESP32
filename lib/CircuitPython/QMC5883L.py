import struct
from time import sleep
from busio import I2C
from I2C_SPI_protocol_Base import I2C_Impl

QMC5883L_DEFAULT_ADDRESS = 0xD

_QMC5883L_DEFAULT_ID = 0xFF

_QMC5883L_DATA_READ_X_LSB = 0x00
_QMC5883L_DATA_READ_X_MSB = 0x01
_QMC5883L_DATA_READ_Y_LSB = 0x02
_QMC5883L_DATA_READ_Y_MSB = 0x03
_QMC5883L_DATA_READ_Z_LSB = 0x04
_QMC5883L_DATA_READ_Z_MSB = 0x05
_QMC5883L_TEMP_READ_LSB   = 0x07
_QMC5883L_TEMP_READ_MSB	  = 0x08 
_QMC5883L_STATUS          = 0x06 # DOR | OVL | DRDY
_QMC5883L_CONFIG_1        = 0x09 # OSR | RNG | ODR | MODE
_QMC5883L_CONFIG_2        = 0x0A # SOFT_RST | ROL_PNT | INT_ENB
_QMC5883L_CONFIG_3        = 0x0B # SET/RESET Period FBR [7:0]
_QMC5883L_ID              = 0x0D

_QMC5883L_SCALE_FACTOR 	        = 0.732421875
_QMC5883L_CONVERT_GAUSS_2G      = 12000.0
_QMC5883L_CONVERT_GAUSS_8G      = 3000.0
_QMC5883L_CONVERT_MICROTESLA    = 100
_QMC5883L_DECLINATION_ANGLE     = 93.67/1000.0  # radian, Tekirdag/Turkey

STATUS_VARIABLES = {
    "NORMAL"                    : (0x0),
    "NO_NEW_DATA"               : (0x1),
    "NEW_DATA_IS_READY"         : (0x2),
    "DATA_OVERFLOW"             : (0x3),
    "DATA_SKIPPED_FOR_READING"  : (0x4)
}

MODE_VARIABLES = {
    "MODE_CONTROL_STANDBY"      : (0x0),
    "MODE_CONTROL_CONTINUOUS"   : (0x1),
}

ODR_VARIABLES = {
    "OUTPUT_DATA_RATE_10HZ"     : (0x00),
    "OUTPUT_DATA_RATE_50HZ"     : (0x04),
    "OUTPUT_DATA_RATE_100HZ"    : (0x08),
    "OUTPUT_DATA_RATE_200HZ"    : (0x0C),
}

RNG_VARIABLES = {
    "FULL_SCALE_2G"   : (0x00),
    "FULL_SCALE_8G"   : (0x10),
}

OSR_VARIABLES = {
    "OVER_SAMPLE_RATIO_512"   : (0x00),
    "OVER_SAMPLE_RATIO_256"   : (0x40),
    "OVER_SAMPLE_RATIO_128"   : (0x80),
    "OVER_SAMPLE_RATIO_64"    : (0xC0),
}



class QMC5883L:
    def __init__(self, bus_implementation: I2C_Impl) -> None:
        self._bus_implementation = bus_implementation

        self._chip_id = self._read_byte(_QMC5883L_ID)
        if self._chip_id != _QMC5883L_DEFAULT_ID:
            raise RuntimeError(
                "Failed to find %s - check your wiring!" % self.__class__.__name__
            )

        self._write_register_byte(_QMC5883L_CONFIG_3, 0x01) #set FBR

        buf = MODE_VARIABLES["MODE_CONTROL_CONTINUOUS"] | \
              ODR_VARIABLES["OUTPUT_DATA_RATE_200HZ"]   | \
              RNG_VARIABLES["FULL_SCALE_8G"]            | \
              OSR_VARIABLES["OVER_SAMPLE_RATIO_64"]
        self._write_register_byte(_QMC5883L_CONFIG_1, buf);  #setup

        self.magnetometer()


    def magnetometer(self):
        raw_mag_data = [0,0,0]

        l = self._read_byte(_QMC5883L_DATA_READ_X_LSB)
        h = self._read_byte(_QMC5883L_DATA_READ_X_MSB)
        raw_mag_data[0] =  struct.unpack('<h', bytes([l, h]))[0]

        l = self._read_byte(_QMC5883L_DATA_READ_Y_LSB)
        h = self._read_byte(_QMC5883L_DATA_READ_Y_MSB)
        raw_mag_data[1] =  struct.unpack('<h', bytes([l, h]))[0]

        l = self._read_byte(_QMC5883L_DATA_READ_Z_LSB)
        h = self._read_byte(_QMC5883L_DATA_READ_Z_MSB)
        raw_mag_data[2] =  struct.unpack('<h', bytes([l, h]))[0]

        x=(_QMC5883L_CONVERT_MICROTESLA*raw_mag_data[0]/_QMC5883L_SCALE_FACTOR)/_QMC5883L_CONVERT_GAUSS_8G;
        y=(_QMC5883L_CONVERT_MICROTESLA*raw_mag_data[1]/_QMC5883L_SCALE_FACTOR)/_QMC5883L_CONVERT_GAUSS_8G;
        z=(_QMC5883L_CONVERT_MICROTESLA*raw_mag_data[2]/_QMC5883L_SCALE_FACTOR)/_QMC5883L_CONVERT_GAUSS_8G;
    
        return (x,y,z)

   
    def temperature(self) -> float:
        temp = [0]

        l = self._read_byte(_QMC5883L_TEMP_READ_LSB)
        h = self._read_byte(_QMC5883L_TEMP_READ_MSB)
        temp[0] =  struct.unpack('<h', bytes([l, h]))[0]

        return temp[0] / 100.0


    def _read_byte(self, register: int) -> int:
        """Read a byte register value and return it"""
        return self._read_register(register, 1)[0]
    
    def _read_register(self, register: int, length: int) -> bytearray:
        return self._bus_implementation.read_register(register, length)
    
    def _write_register_byte(self, register: int, value: int) -> None:
        self._bus_implementation.write_register_byte(register, value)


class QMC5883L_I2C(QMC5883L):
    def __init__(self, i2c: I2C, address: int = 0x1A) -> None:  # QMC5883L_ADDRESS
        super().__init__(I2C_Impl(i2c, address))
