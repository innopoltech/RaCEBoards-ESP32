import pwmio
from microcontroller import Pin

class BUZZ:
    def __init__(self, pin:Pin, freq:int) -> None:
        self.pin = pin
        self.freq = freq

        #Create buzzer object
        self.buzzer = pwmio.PWMOut(pin=self.pin, duty_cycle=0, frequency=self.freq)
    
    def __del__(self):
        try:
            self.buzzer.deinit()
        except Exception:
            pass
        
    def BuzzerOn(self):
        #Turn On buzzer
        self.buzzer.duty_cycle = 32768

    def BuzzerOff(self):
        #Turn Off buzzer
        self.buzzer.duty_cycle = 0