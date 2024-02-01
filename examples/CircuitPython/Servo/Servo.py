import pwmio
import board
import time

##### Servo specification
# Rotational Range: 180°
# Pulse Cycle: ca. 20 ms
# Pulse Width: 500-2400 µs
min_angle = 0
max_angle = 180

##### PWM specification
# Frequency = 50 Hz
# Duty cycle range = 0 ... 65535
# lsb/us = 3.28
# Duty cycle 0°   = 1640
# Duty cycle 180° = 7872
min_width = 1640
max_width = 7872

################################### Setup
#Create PWM1_pin object
PWM1_pin = board.IO48
#Create PWM2_pin object
PWM2_pin = board.IO45

#Create Servo1 object
Servo1 = pwmio.PWMOut(pin=PWM1_pin, duty_cycle=min_width, frequency=50)
#Create Servo2 object
Servo2 = pwmio.PWMOut(pin=PWM2_pin, duty_cycle=min_width, frequency=50)

#Similar to arduino Servo.write() 
def ServoSetAngle(servo, angle):
    #Clamp
    if(angle < min_angle):
        angle = min_angle
    if(angle > max_angle):
        angle = max_angle
    
    #Similar to map() 
    duty = (angle - min_angle) * (max_width - min_width) / (max_angle - min_angle) + min_width
    servo.duty_cycle = int(duty)

################################### Work
while True:
    time.sleep(1)
    ServoSetAngle(Servo1, 0)
    ServoSetAngle(Servo2, 180)
    time.sleep(1)
    ServoSetAngle(Servo1, 180)
    ServoSetAngle(Servo2, 0)