
import time
import board
import adafruit_hcsr04

# trigger_pin=board.IO5 #Shared with RADIO_BUSY
# echo_pin=board.IO16 #Shared with MAG_DATA_READY(NOT USED)
#or
trigger_pin=board.IO21 #Shared with motor A (pin1)
echo_pin=board.IO14 #Shared with ACC_MAG_DATA_READY (NOT USED)

sonar = adafruit_hcsr04.HCSR04(trigger_pin=trigger_pin, echo_pin=echo_pin)

while True:
    try:
            print((sonar.distance,))
    except RuntimeError:
            print("Retrying!")
            pass
    time.sleep(0.1)