import board
import time

from lib import DC_MOTOR

################################### Setup

#MOTOR A
xIN1_PWM_pin  = board.IO21
xIN2_GPIO_pin = board.IO47

#MOTOR B - shared with servo 3/4
# xIN1_PWM_pin  = board.IO42
# xIN2_GPIO_pin = board.IO41

dc_mot = DC_MOTOR.MOTOR(xIN1_PWM_pin, xIN2_GPIO_pin)

################################### Work
#Start point
speed = 0
dir   = 1
while True:
    time.sleep(0.05)
    #Change direction
    if(dir == 1  and speed == 100):
        dir = -1
    if(dir == -1 and speed == -100):
        dir = 1

    #Change speed
    speed += dir
    dc_mot.DCMotorSetSpeed(speed)

    #print
    print(f"Direction : {dir}, Speed: {speed}")