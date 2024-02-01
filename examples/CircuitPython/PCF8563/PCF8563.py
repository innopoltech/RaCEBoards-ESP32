import time
import board
import busio
import math
from lib import PCF8563

################################### Setup
#initial delay
time.sleep(1)
#Create i2c module object
i2c1_module = busio.I2C(scl=board.IO2, sda=board.IO3)
#Create PCF8563 object
PCF8563     = PCF8563.PCF8563_I2C(i2c1_module, PCF8563.PCF8563_DEFAULT_ADDRESS)

#Setup Date {Year, Month, Day}
PCF8563.setDate(2023,01,01)
#Setup Time {Hour, Minute, Second}
PCF8563.setTime(00,00,00)

################################### Work
while True:
    Year, Month, Day     = PCF8563.getDate()
    Hour, Minute, Second = PCF8563.getTime()

    time.sleep(0.5)
    print(f" Year: {Year}, Month: {Month}, Day: {Day}, Hour: {Hour}, Minute: {Minute}, Second: {Second}")