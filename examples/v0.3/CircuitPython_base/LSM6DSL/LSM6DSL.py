import time
import board
import busio
from lib import LSM6DSL

################################### Setup
#initial delay
time.sleep(1)
#Create i2c module object
i2c1_module = busio.I2C(scl=board.IO2, sda=board.IO3)
#Create lsm6dsl object
lsm6dsl     = LSM6DSL.LSM6DSL_I2C(i2c1_module, LSM6DSL.LSM6DSL_DEFAULT_ADDRESS)

################################### Work
while True:
    #Get data from device
    acc = lsm6dsl.acceleration()
    gyro = lsm6dsl.gyro()
    temp = lsm6dsl.temperature()
    
    #print
    print(f"\nTemperature   : {temp} C")
    print(f"Acceleration: X:{acc[0]}, Y: {acc[1]}, Z: {acc[2]} m/s^2")
    print(f"Gyro: X:{gyro[0]}, Y: {gyro[1]}, Z: {gyro[2]} degree/s")
    time.sleep(0.5)