import time
import board
from rainbowio import colorwheel
import neopixel #Custom for RaCEBoard v0.4

################################### Setup
#Shared with LED
pixel_pin = board.IO8
num_pixels = 12

#Create pixels object
pixels = neopixel.NeoPixel(pixel_pin, num_pixels, brightness=0.01, auto_write=False)

################################### Work
def rainbow_cycle(wait):
    for j in range(255):
        for i in range(num_pixels):
            rc_index = (i * 256 // num_pixels) + j
            pixels[i] = colorwheel(rc_index & 255)
        pixels.show()
        time.sleep(wait)

while True:
    rainbow_cycle(0)