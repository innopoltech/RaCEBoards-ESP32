import time
import board
import busio
import math
from lib import QMC5883L

################################### Setup
#initial delay
time.sleep(1)
#Create i2c module object
i2c1_module = busio.I2C(scl=board.IO2, sda=board.IO3)
#Create qmc5883l object
qmc5883l     = QMC5883L.QMC5883L_I2C(i2c1_module, QMC5883L.QMC5883L_DEFAULT_ADDRESS)

################################### Work
while True:
    #Get data from device
    temp = qmc5883l.temperature()
    mag = qmc5883l.magnetometer()

    #print
    print(f"\nTemperature   : {temp} C") #Relative !
    print(f"Magnetometer: X:{mag[0]}, Y: {mag[1]}, Z: {mag[2]} mTesla")

    time.sleep(0.5)