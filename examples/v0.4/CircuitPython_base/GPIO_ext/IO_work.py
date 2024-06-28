import board
import busio
import digitalio
import time 

from lib import GPIO_IO

################################### Setup
#Create i2c module object
i2c1_module = busio.I2C(scl=board.IO2, sda=board.IO3)

io_pins = GPIO_IO.IO_I2C(i2c1_module)

#Get pin (0...7)
#The pins can only operate in BASIC mode and do not support things like PWM ...
pin_test_0_out = io_pins.get_pin(0)
pin_test_1_in = io_pins.get_pin(1)

#Set directions
#Short-circuit pins 0 and 1!
pin_test_0_out.direction = digitalio.Direction.OUTPUT
pin_test_1_in.direction = digitalio.Direction.INPUT

################################### Work
while True:
    print("#########################################")

    print("Set pin_0 to HI")
    pin_test_0_out.value = 1

    for i in range(5):
        print(f"\tRead pin_1 = {pin_test_1_in.value}")
        time.sleep(0.5)

    print("Set pin_0 to LO")
    pin_test_0_out.value = 0

    for i in range(5):
        print(f"\tRead pin_1 = {pin_test_1_in.value}")
        time.sleep(0.5)

    print("#########################################")
    time.sleep(1)