import pwmio
import board
import time

frequency_buzzer = 1000 # Hz

################################### Setup
#Create buzzer_pin object
buzzer_pin = board.IO4
#Create buzzer object
buzzer = pwmio.PWMOut(pin=buzzer_pin, duty_cycle=0, frequency=frequency_buzzer)

#Turn On buzzer
def BuzzerOn():
    buzzer.duty_cycle = 32768
#Turn Off buzzer
def BuzzerOff():
    buzzer.duty_cycle = 0

################################### Work
while True:
    time.sleep(0.3)
    BuzzerOn()
    time.sleep(0.3)
    BuzzerOff()