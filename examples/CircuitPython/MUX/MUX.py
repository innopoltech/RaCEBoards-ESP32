import board
import digitalio
import analogio
import time 

################################### Setup
#Create mux_sources objects
mux_s0           = digitalio.DigitalInOut(board.IO46)
mux_s0.direction = digitalio.Direction.OUTPUT
mux_s0.value     = False

mux_s1           = digitalio.DigitalInOut(board.IO0)
mux_s1.direction = digitalio.Direction.OUTPUT
mux_s1.value     = False

mux_s2           = digitalio.DigitalInOut(board.IO39)
mux_s2.direction = digitalio.Direction.OUTPUT
mux_s2.value     = False

#Select Line
def muxSelectLine(line):
    global mux_s0,mux_s1,mux_s2
    if(line<0 or line>7):
        return; 
    mux_s2.value = (int(line) >> 2) & 1
    mux_s1.value = (int(line) >> 1) & 1
    mux_s0.value = (int(line) >> 0) & 1

#default state - analog input
mux_inout        = analogio.AnalogIn(board.IO1)

# Toogle mux2In, (!) Line 1 and 2 have pull-down 10k, Line 3-7 nothing
def mux2In():
    global mux_inout
    mux_inout.deinit()
    mux_inout                   = digitalio.DigitalInOut(board.IO1)
    mux_inout.direction         = digitalio.Direction.INPUT

# Toogle mux2In, reference_voltage = 3.3V,(0-65535, 16-bit), Line 0 have res div (k=0.5)
def mux2Analog():
    global mux_inout
    mux_inout.deinit()
    mux_inout                   = analogio.AnalogIn(board.IO1)

# Toogle mux2Out
def mux2Out():
    global mux_inout
    mux_inout.deinit()
    mux_inout                   = digitalio.DigitalInOut(board.IO1)
    mux_inout.direction         = digitalio.Direction.OUTPUT

################################### Work
while True:
    print("#########################################")

##### Test analog
    #Switch pin mode
    mux2Analog()
    #Select Line 
    muxSelectLine(0)

    #Calculate
    raw_bit  = mux_inout.value
    percent  = raw_bit/65535.0
    raw_volt = percent*mux_inout.reference_voltage
    volt     = raw_volt*2.0
    print("\r\nTest Analog Line 0")
    print(f"My_batttery: {raw_bit}, In %: {percent}, In raw volt: {raw_volt}, With res div: {volt}")

##### Test In or Out
    test_in = False
    if(test_in):
        print("\r\nTest Digital In Line 0-7")
        #Switch pin mode
        mux2In()
        for i in range(0,7):
            #Select Line 
            muxSelectLine(i)
            print(f"Line {i} is : {mux_inout.value}")
    else:
        print("\r\nTest Digital Out Line 1-7")
        #Switch pin mode
        mux2Out()
        i = 7   #Line 1-7, (!) Line 0 Only input  (!)
        #Select Line 
        muxSelectLine(i)
        mux_inout.value = True
        print(f"Line {i} is : {mux_inout.value}")
        time.sleep(2)

    print("#########################################")
    time.sleep(1)