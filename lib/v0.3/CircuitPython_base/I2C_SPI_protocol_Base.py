from busio import I2C, SPI
from digitalio import DigitalInOut
import struct

class I2C_Impl:
    "Protocol implementation for the I2C bus."

    def __init__(self, i2c: I2C, address: int) -> None:
        from adafruit_bus_device import (  # pylint: disable=import-outside-toplevel
            i2c_device,
        )

        self._i2c = i2c_device.I2CDevice(i2c, address)

    def read_register(self, register: int, length: int) -> bytearray:
        "Read from the device register."
        with self._i2c as i2c:
            i2c.write(bytes([register & 0xFF]))
            result = bytearray(length)
            i2c.readinto(result)
            return result

    def write_register_byte(self, register: int, value: int) -> None:
        """Write to the device register"""
        with self._i2c as i2c:
            i2c.write(bytes([register & 0xFF, value & 0xFF]))


class SPI_Impl:
    """Protocol implemenation for the SPI bus."""

    def __init__(
        self,
        spi: SPI,
        cs: DigitalInOut,
        baudrate: int = 100000,
    ) -> None:
        from adafruit_bus_device import (  # pylint: disable=import-outside-toplevel
            spi_device,
        )

        self._spi = spi_device.SPIDevice(spi, cs, baudrate=baudrate)

    def write_byte(self, value: int) -> None:
        with self._spi as spi:
            spi.write(bytes([value]))  # pylint: disable=no-member

    def read_write_byte(self, value: int) -> None:
        with self._spi as spi:
            result = bytearray(1)
            result_m = memoryview(result).cast('B')
            spi.write_readinto(bytes([value]), result_m)  # pylint: disable=no-member
            return result_m[0]

    def lock(self):
        with self._spi as spi:
            return spi.try_lock()

    def unlock(self):
        with self._spi as spi:
            spi.unlock()



    def read_register(self, register: int, length: int) -> bytearray:
        "Read from the device register."
        register = (register | 0x80) & 0xFF  # Read single, bit 7 high.
        with self._spi as spi:
            spi.write(bytearray([register]))  # pylint: disable=no-member
            result = bytearray(length)
            spi.readinto(result)  # pylint: disable=no-member
            # print("$%02X => %s" % (register, [hex(i) for i in result]))
            return result

    def write_register_byte(self, register: int, value: int) -> None:
        "Write value to the device register."
        register &= 0x7F  # Write, bit 7 low.
        with self._spi as spi:
            spi.write(bytes([register, value & 0xFF]))  # pylint: disable=no-member
