import struct
from time import sleep
from busio import I2C
from I2C_SPI_protocol_Base import I2C_Impl

PCF8563_DEFAULT_ADDRESS = 0x51

PCF8563_REG_CONTROL_STATUS1 = 0x00
PCF8563_REG_CONTROL_STATUS2 = 0x01
PCF8563_REG_TIME            = 0x02
PCF8563_REG_SECONDS         = 0x02
PCF8563_REG_MINUTES         = 0x03
PCF8563_REG_HOURS          	= 0x04
PCF8563_REG_DATE            = 0x05
PCF8563_REG_WEEKDAY         = 0x06
PCF8563_REG_MONTH           = 0x07
PCF8563_REG_YEAR            = 0x08
PCF8563_REG_ALARM_MINUTE    = 0x09
PCF8563_REG_ALARM_HOUR      = 0x0A
PCF8563_REG_ALARM_DAY       = 0x0B
PCF8563_REG_ALARM_WEEKDAY   = 0x0C
PCF8563_REG_CLKOUT    	    = 0x0D
PCF8563_REG_TIMER_CONTROL   = 0x0E
PCF8563_REG_TIMER     	    = 0x0F

# Controll register 1 0x00
PCF8563_CONTROL1_TEST1		= 7
PCF8563_CONTROL1_STOP		= 5
PCF8563_CONTROL1_TESTC		= 3

# Controll register 2 0x01
PCF8563_CONTROL2_TI_TP		= 4
PCF8563_CONTROL2_AF		    = 3
PCF8563_CONTROL2_TF			= 2
PCF8563_CONTROL2_AIE		= 1
PCF8563_CONTROL2_TIE		= 0

# CLKOUT control register 0x0D
PCF8563_CLKOUT_CONTROL_FE	= 7
PCF8563_CLKOUT_CONTROL_FD1	= 1
PCF8563_CLKOUT_CONTROL_FD0  = 0

CLKOUT_Freq = {
    "CLKOUT_FREQ_1HZ"        : (0x3),
    "CLKOUT_FREQ_32HZ"       : (0x2),
    "CLKOUT_FREQ_1024HZ"     : (0x1),
    "CLKOUT_FREQ_32768HZ"    : (0x0),
}


class PCF8563:
    def __init__(self, bus_implementation: I2C_Impl) -> None:
        self._bus_implementation = bus_implementation

        #buf = self._read_byte(PCF8563_REG_CONTROL_STATUS1)  #set all to normal mode
        buf = 0
        self._write_register_byte(PCF8563_REG_CONTROL_STATUS1, buf)

        #buf = self._read_byte(PCF8563_REG_CONTROL_STATUS2)  #set all to normal mode
        buf = 0
        self._write_register_byte(PCF8563_REG_CONTROL_STATUS2, buf)

    def setDate(self, Year, Month, Day):
        Year       = self.dec2bcd(Year- 2000)
        Month      = self.dec2bcd(Month)
        Day        = self.dec2bcd(Day)

        self._write_register_byte(PCF8563_REG_YEAR,  Year)
        self._write_register_byte(PCF8563_REG_MONTH, Month)
        self._write_register_byte(PCF8563_REG_DATE,  Day)

    def setTime(self, Hour, Minute, Second):
        Second       = self.dec2bcd(Second)
        Minute      = self.dec2bcd(Minute)
        Hour        = self.dec2bcd(Hour)

        self._write_register_byte(PCF8563_REG_SECONDS, Second)
        self._write_register_byte(PCF8563_REG_MINUTES, Minute)
        self._write_register_byte(PCF8563_REG_HOURS,   Hour)

    def getDate(self):
        Year       = self.bcd2dec(self._read_byte(PCF8563_REG_YEAR)) + 2000
        Month      = self.bcd2dec(self._read_byte(PCF8563_REG_MONTH) & 0x1F)
        Day        = self.bcd2dec(self._read_byte(PCF8563_REG_DATE)  & 0x3F)
        return Year, Month, Day

    def getTime(self):
        Second     = self.bcd2dec(self._read_byte(PCF8563_REG_SECONDS) & 0x7F)
        Minute     = self.bcd2dec(self._read_byte(PCF8563_REG_MINUTES) & 0x7F)
        Hour       = self.bcd2dec(self._read_byte(PCF8563_REG_HOURS)   & 0x3F)
        return Hour, Minute, Second

    def bcd2dec(self, BCD):
        return (((BCD & 0xF0)>>4) *10) + (BCD & 0xF);

    def dec2bcd(self, DEC):
        return (int(DEC / 10)<<4) + (DEC % 10)

    def _read_byte(self, register: int) -> int:
        """Read a byte register value and return it"""
        return self._read_register(register, 1)[0]
    
    def _read_register(self, register: int, length: int) -> bytearray:
        return self._bus_implementation.read_register(register, length)
    
    def _write_register_byte(self, register: int, value: int) -> None:
        self._bus_implementation.write_register_byte(register, value)


class PCF8563_I2C(PCF8563):
    def __init__(self, i2c: I2C, address: int = 0x51) -> None:  # PCF8563_ADDRESS
        super().__init__(I2C_Impl(i2c, address))
