import time
import board
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
          #print("Send Telemetry : 0, 0, 1, 512, -5, 45.123, 33.456")
          print(Ra01S.ParceTelemetryPack())
    
    time.sleep(0.2)