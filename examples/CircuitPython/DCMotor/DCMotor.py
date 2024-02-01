import pwmio
import board
import time
import digitalio

#H-bridge specification
# xIN1 | xIN2 |  FUNCTION
# PWM  | 0    | Forward PWM, fast decay
# 1    | PWM  | Forward PWM, slow decay
# 0    | PWM  | Reverse PWM, fast decay
# PWM  | 1    | Reverse PWM, slow decay

################################### Setup
#Create xIN1_PWM_pin object
xIN1_PWM_pin  = board.IO21
#Create xIN2_GPIO_pin object
xIN2_GPIO_pin = board.IO47

#Create xIN1 object
xIN1 = pwmio.PWMOut(pin=xIN1_PWM_pin, duty_cycle=0, frequency=50)

#Create xIN2 object
xIN2           = digitalio.DigitalInOut(xIN2_GPIO_pin)
xIN2.direction = digitalio.Direction.OUTPUT

#DC motor speed control -100 ... 100 %
def DCMotorSetSpeed(speed):
    #clamp
    if(speed < -100):   
        speed = -100
    if(speed > 100):
        speed = 100
    
    #"Clear" duty cycle
    duty = abs(speed) * 65535.0/100.0

    #Inversion if reverse 
    duty = (65535.0-duty) if speed < 0 else duty
    xIN2.value = True if speed < 0 else False

    xIN1.duty_cycle = int(duty)

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
    DCMotorSetSpeed(speed)

    #print
    print(f"Direction : {dir}, Speed: {speed}")