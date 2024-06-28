import os
import time 
import busio
import board
import sdcardio
import storage

my_dir = "11_log"

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
#List directory
list_dir = os.listdir('/sd') 
print(list_dir) 

#Create new dir if not exist
if my_dir not in list_dir:
    os.mkdir("/sd/" + my_dir)

#List directory
list_dir = os.listdir('/sd') 
print(list_dir) 

#Delete dir if exist (the folder may contain files, but not other folders)
if my_dir in list_dir:
    for file in os.listdir("/sd/" + my_dir):
        os.remove("/sd/" + my_dir + '/' + file)
    os.rmdir("/sd/"+my_dir)

#List directory
list_dir = os.listdir('/sd') 
print(list_dir) 

#Unmount FileSystem
storage.umount('/sd')