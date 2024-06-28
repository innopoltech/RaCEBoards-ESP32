import board
import time
import busio
from lib import BMP280

################################### Setup
#initial delay
time.sleep(1)
#Create i2c module object
i2c1_module = busio.I2C(scl=board.IO2, sda=board.IO3)
#Create bmp280 object
bmp280      = BMP280.Adafruit_BMP280_I2C(i2c1_module, BMP280._BMP280_ADDRESS)
#Set zero pressure
bmp280.sea_level_pressure = 101.325 #kPa

################################### Work
while True:
    #Get data from device
    temp    = bmp280.temperature()
    press   = bmp280.pressure()
    alt     = bmp280.altitude()

    #print
    print(f"\nTemperature   : {temp} C")
    print(f"Pressure      : {press} kPa")
    print(f"Altitude      : {alt} meters")
    time.sleep(2)