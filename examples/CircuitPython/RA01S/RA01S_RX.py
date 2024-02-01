import time
import board
import digitalio
import busio
import math
from lib import Ra01S

################################### Setup
#initial delay
time.sleep(1)

#Create SDCard_cs_pin object
SDCard_cs_pin  = board.IO15

#Create SDCard_cs object , (!) before init spi
SDCard_cs = digitalio.DigitalInOut(SDCard_cs_pin)
SDCard_cs.direction = digitalio.Direction.OUTPUT
SDCard_cs.value = True

#Create spi module object
spi0_speed = 2000000
spi0_module     = busio.SPI(clock=board.IO12, MOSI=board.IO11, MISO=board.IO13)

#Create pins object
Ra01S_cs_pin    = board.IO7
Ra01S_nRst_pin  = board.IO6
Ra01S_nInt_pin  = board.IO5

#Create Ra01S_cs object
Ra01S_cs = digitalio.DigitalInOut(Ra01S_cs_pin)
Ra01S_cs.direction = digitalio.Direction.OUTPUT
Ra01S_cs.value = True

#Create Ra01S_nRst object
Ra01S_nRst = digitalio.DigitalInOut(Ra01S_nRst_pin)
Ra01S_nRst.direction = digitalio.Direction.OUTPUT
Ra01S_nRst.value = True

#Create Ra01S_nInt object
Ra01S_nInt = digitalio.DigitalInOut(Ra01S_nInt_pin)
Ra01S_nInt.direction = digitalio.Direction.INPUT

#Create Ra01S object
Ra01S     = Ra01S.Ra01S_SPI(spi0_module, Ra01S_cs, Ra01S_nRst, Ra01S_nInt, spi0_speed)

#First init and turn on
Ra01S.on()

#Set power mode
Ra01S.SetLowPower()
#Ra01S.SetLowPower()

#Select channel 0-6
Ra01S.SetChannel(0) 

################################### Work
num_string_msg = 0
while True:
    test = 0 # 0- Rec string, 1- Rec telemetry pack

    if(test == 0):
        if(Ra01S.AvailablePacket()):
            num_string_msg+=1
            print(f"Number: {num_string_msg}, Message: {Ra01S.ReciveS()}" , end="")
        # else:
        #     print("empty")

    else:
       if(Ra01S.AvailablePacket()):
          print(Ra01S.ParceTelemetryPack())
    
    time.sleep(0.2)