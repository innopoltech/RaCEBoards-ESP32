import pwmio
import digitalio
from microcontroller import Pin


class MOTOR:
    #H-bridge specification
    # xIN1 | xIN2 |  FUNCTION
    # PWM  | 0    | Forward PWM, fast decay
    # 1    | PWM  | Forward PWM, slow decay
    # 0    | PWM  | Reverse PWM, fast decay
    # PWM  | 1    | Reverse PWM, slow decay

    ################################### Setup

    def __init__(self, pin1:Pin, pin2:Pin):
        self.xIN1 = pin1
        self.xIN2 = pin2

        self.xIN1 = pwmio.PWMOut(pin=self.xIN1, duty_cycle=0, frequency=50)

        self.xIN2           = digitalio.DigitalInOut(self.xIN2)
        self.xIN2.direction = digitalio.Direction.OUTPUT

    def __del__(self):
        try:
            self.xIN1.deinit()
        except Exception:
            pass
        try:
            self.xIN2.deinit()
        except Exception:
            pass

    def DCMotorSetSpeed(self, speed):
        #DC motor speed control -100 ... 100 %
        #clamp
        if(speed < -100):   
            speed = -100
        if(speed > 100):
            speed = 100
        
        #"Clear" duty cycle
        duty = abs(speed) * 65535.0/100.0

        #Inversion if reverse 
        duty = (65535.0-duty) if speed < 0 else duty
        self.xIN2.value = True if speed < 0 else False

        self.xIN1.duty_cycle = int(duty)