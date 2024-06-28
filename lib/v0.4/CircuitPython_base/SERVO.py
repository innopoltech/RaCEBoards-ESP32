import pwmio
from microcontroller import Pin


class SERVO:
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

    def __init__(self, pin:Pin) -> None:
        #Create Servo object
        self.pin = pin
        self.servo = pwmio.PWMOut(pin=pin, duty_cycle=self.min_width, frequency=50)

    def __del__(self):
        try:
            self.servo.deinit()
        except Exception:
            pass

    def ServoSetAngle(self, angle):
        #Similar to arduino Servo.write() 
        #Clamp
        if(angle < self.min_angle):
            angle = self.min_angle
        if(angle > self.max_angle):
            angle =  self.max_angle
        
        #Similar to map() 
        duty = (angle - self.min_angle) * (self.max_width - self.min_width) / (self.max_angle - self.min_angle) + self.min_width
        self.servo.duty_cycle = int(duty)
