import time
import board
import digitalio

################################### Setup
#Create led object
led           = digitalio.DigitalInOut(board.IO8)
led.direction = digitalio.Direction.OUTPUT

################################### Work
while True:
    led.value = True
    time.sleep(0.1)
    led.value = False
    time.sleep(0.1)