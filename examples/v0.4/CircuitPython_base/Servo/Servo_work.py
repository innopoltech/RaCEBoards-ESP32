import board
import time

from lib import SERVO

################################### Setup
# Servo 1
PWM1_pin = board.IO48
# Servo 2
PWM2_pin = board.IO45
# Servo 3 - shared with MOTOR B
PWM3_pin = board.IO42
# Servo 4 - shared with MOTOR B
PWM4_pin = board.IO41

servo1 = SERVO.SERVO(PWM1_pin)
servo2 = SERVO.SERVO(PWM2_pin)
################################### Work
while True:
    time.sleep(1)
    servo1.ServoSetAngle(0)
    servo2.ServoSetAngle(180)
    time.sleep(1)
    servo1.ServoSetAngle(180)
    servo2.ServoSetAngle(0)