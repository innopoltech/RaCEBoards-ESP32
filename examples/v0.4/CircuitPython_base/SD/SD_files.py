import os
import time 
import busio
import board
import sdcardio
import storage

my_file = "my_log.txt"

################################### Setup
#initial delay
time.sleep(1)
#Create spi module object
spi0_module     = busio.SPI(clock=board.IO12, MOSI=board.IO11, MISO=board.IO13)
#Create cs object
sd_card_cs_pin  = board.IO15


#Create SDCard object
sd_card = sdcardio.SDCard(spi0_module, sd_card_cs_pin)
#Create FileSystem object
vfs     = storage.VfsFat(sd_card)
#Mount FileSystem
storage.mount(vfs, '/sd')

################################### Work

#Open File to write (append mode)
f = open("/sd/"+my_file, "a")
#Write (next line = "\r\n")
f.write("Hello, world!\r\n")
f.close()

#Open File to read 
f = open("/sd/"+my_file, "r")
#Read All ((!)Only 5-6 MB RAM available(!))
contents = f.read()
print(contents)
f.close()

storage.umount('/sd')