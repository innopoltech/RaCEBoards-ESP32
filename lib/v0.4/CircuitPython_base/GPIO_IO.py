import digitalio
from digitalio import DigitalInOut
from busio import I2C
from microcontroller import Pin
from micropython import const
from I2C_SPI_protocol_Base import I2C_Impl

# PCA9554 Command Byte
INPUTPORT: int = const(0x00)
OUTPUTPORT: int = const(0x01)
POLINVPORT: int = const(0x02)
CONFIGPORT: int = const(0x03)

class IO:
    def __init__(self, bus_implementation: I2C_Impl) -> None:
        self._bus_implementation = bus_implementation

    def get_pin(self, pin: int) -> DigitalInOut:
        if(pin < 0 or pin > 7):
            return None
        return DigitalInOut(pin, self)

    def write_gpio(self, register: int, val: int) -> None:
        self._write_register_byte(register & 0xFF, val & 0xFF)

    def read_gpio(self, register) -> int:
        return self._read_register(register & 0xFF, 1)[0]

    def set_pin_mode(self, pin: int, val: bool) -> None:
        current_value = self.read_gpio(CONFIGPORT)
        if val:
            # Set as input and turn on the pullup
            self.write_gpio(CONFIGPORT, current_value | (1 << pin))
        else:
            # Set as output and turn off the pullup
            self.write_gpio(CONFIGPORT, current_value & ~(1 << pin))

    def get_pin_mode(self, pin: int) -> bool:
        return bool((self.read_gpio(CONFIGPORT) >> pin) & 0x1)

    def write_pin(self, pin: int, val: bool) -> None:
        current_value = self.read_gpio(OUTPUTPORT)
        if val:
            # turn on the pullup (write high)
            self.write_gpio(OUTPUTPORT, current_value | (1 << pin))
        else:
            # turn on the transistor (write low)
            self.write_gpio(OUTPUTPORT, current_value & ~(1 << pin))

    def read_pin(self, pin: int) -> bool:
        return bool((self.read_gpio(INPUTPORT) >> pin) & 0x1)

    def _read_register(self, register: int, length: int) -> bytearray:
        return self._bus_implementation.read_register(register, length)

    def _write_register_byte(self, register: int, value: int) -> None:
        self._bus_implementation.write_register_byte(register, value)

class DigitalInOut:
    """Digital input/output of the PCA9554.  The interface is exactly the
    same as the digitalio.DigitalInOut class, however:

      - PCA9554 does not support pull-down resistors
      - PCA9554 does not actually have a sourcing transistor, instead there's
        an internal pullup

    Exceptions will be thrown when attempting to set unsupported pull
    configurations.
    """

    def __init__(self, pin_number: int, pcf: IO) -> None:
        """Specify the pin number of the PCA9554 0..7, and instance."""
        self._pin = pin_number
        self._pcf = pcf

    def switch_to_output(self, value: bool = False, **kwargs) -> None:
        """Switch the pin state to a digital output with the provided starting
        value (True/False for high or low, default is False/low).
        """
        self.direction = digitalio.Direction.OUTPUT
        self.value = value

    def switch_to_input(self, pull = None, **kwargs) -> None:
        """Switch the pin state to a digital input which is the same as
        setting the light pullup on.  Note that true tri-state or
        pull-down resistors are NOT supported!
        """
        self.direction = digitalio.Direction.INPUT
        self.pull = pull

    @property
    def value(self) -> bool:
        """The value of the pin, either True for high or False for
        low.
        """
        return self._pcf.read_pin(self._pin)

    @value.setter
    def value(self, val: bool) -> None:
        self._pcf.write_pin(self._pin, val)

    @property
    def direction(self) -> digitalio.Direction:
        """
        Setting a pin to OUTPUT drives it low, setting it to
        an INPUT enables the light pullup.
        """
        pinmode = self._pcf.get_pin_mode(self._pin, True)
        return digitalio.Direction.INPUT if pinmode else digitalio.Direction.OUTPUT

    @direction.setter
    def direction(self, val: digitalio.Direction) -> None:
        if val == digitalio.Direction.INPUT:
            # for inputs, turn on the pullup (write high)
            self._pcf.set_pin_mode(self._pin, True)
        elif val == digitalio.Direction.OUTPUT:
            # for outputs, turn on the transistor (write low)
            self._pcf.set_pin_mode(self._pin, False)
        else:
            raise ValueError("Expected INPUT or OUTPUT direction!")

    @property
    def pull(self) -> digitalio.Pull.UP:
        """
        Pull-up is always activated so always return the same thing
        """
        return digitalio.Pull.UP

    @pull.setter
    def pull(self, val: digitalio.Pull.UP) -> None:
        if val is digitalio.Pull.UP:
            # for inputs, turn on the pullup (write high)
            self._pcf.write_pin(self._pin, True)
        else:
            raise NotImplementedError("Pull-down resistors not supported.")


class IO_I2C(IO):
    def __init__(self, i2c: I2C, address: int = 0x25) -> None:  # BMP280_ADDRESS
        super().__init__(I2C_Impl(i2c, address))