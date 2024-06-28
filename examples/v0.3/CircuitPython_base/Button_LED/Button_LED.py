import board
import digitalio

################################### Setup
#Create button object
button           = digitalio.DigitalInOut(board.IO9)
button.direction = digitalio.Direction.INPUT
button.pull      = digitalio.Pull.UP

#Create led object
led = digitalio.DigitalInOut(board.IO8)
led.direction = digitalio.Direction.OUTPUT

################################### Work
while True:
    #led != button
    led.value = not button.value