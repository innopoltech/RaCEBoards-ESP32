import time
import board
import digitalio
import busio
import math
from lib import Ra01S

################################### Setup
#initial delay
time.sleep(1)

#Create spi module object
spi0_speed = 2000000
spi0_module     = busio.SPI(clock=board.IO12, MOSI=board.IO11, MISO=board.IO13)

#Create pins object
SDCard_cs  = board.IO15    #be sure to specify!
Ra01S_cs    = board.IO7
Ra01S_nRst  = board.IO6
Ra01S_nInt  = board.IO5

#Create Ra01S object
#The SDcard must be initialized first!
Ra01S     = Ra01S.Ra01S_SPI(spi0_module, Ra01S_cs, Ra01S_nRst, Ra01S_nInt, SDCard_cs, spi0_speed)

#First init and turn on
Ra01S.on()

#Set power mode
Ra01S.SetLowPower()
#Ra01S.SetLowPower()

#Select channel 0-6
Ra01S.SetChannel(0) 

################################### Work
while True:
    test = 0 # 0- Send string, 1- Send telemetry pack
    if(test == 0):
        Ra01S.SendS("Hello!\n")
        print("Send string : Hello")
    else:
        Ra01S.SendTelemetryPack(0, 0, 1, 512, -5, 45.123, 33.456)
        print("Send Telemetry : 0, 0, 1, 512, -5, 45.123, 33.456")
    
    time.sleep(0.5)