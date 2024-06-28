import board
import time 

from lib import GPIO_MUX

################################### Setup
mux = GPIO_MUX.MUX(s0=board.IO46, s1=board.IO0, s2=board.IO39, inout=board.IO1)

################################### Work
while True:
    print("#########################################")

##### Test analog
    #Switch pin mode
    mux.mux2Analog()
    #Select Line 
    mux.muxSelectLine(0)

    #Calculate
    raw_bit  = mux.mux_inout.value
    percent  = raw_bit/65535.0
    raw_volt = percent*mux.mux_inout.reference_voltage
    volt     = raw_volt*2.0
    print("\r\nTest Analog Line 0")
    print(f"My_batttery: {raw_bit}, In %: {percent}, In raw volt: {raw_volt}, With res div: {volt}")

##### Test In or Out
    test_in = False
    if(test_in):
        print("\r\nTest Digital In Line 0-7")
        #Switch pin mode
        mux.mux2In()
        for i in range(0,7):
            #Select Line 
            mux.muxSelectLine(i)
            print(f"Line {i} is : {mux.mux_inout.value}")
    else:
        print("\r\nTest Digital Out Line 1-7")
        #Switch pin mode
        mux.mux2Out()
        i = 7   #Line 1-7, (!) Line 0 Only input  (!)
        #Select Line 
        mux.muxSelectLine(i)
        mux.mux_inout.value = True
        print(f"Line {i} is : {mux.mux_inout.value}")
        time.sleep(2)

    print("#########################################")
    time.sleep(1)