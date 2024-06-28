import board
import time
from lib import BUZZER

buzz = BUZZER.BUZZ(pin=board.IO4, freq=1000)

################################### Work
while True:
    time.sleep(1)
    buzz.BuzzerOn()
    time.sleep(0.3)
    buzz.BuzzerOff()