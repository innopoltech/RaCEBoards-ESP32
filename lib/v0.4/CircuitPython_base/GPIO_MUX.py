import analogio
import digitalio
from microcontroller import Pin


class MUX:
    def __init__(self, s0:Pin, s1:Pin, s2:Pin, inout:Pin):
        #Create mux_sources objects
        self.mux_s0           = digitalio.DigitalInOut(s0)
        self.mux_s0.direction = digitalio.Direction.OUTPUT
        self.mux_s0.value     = False

        self.mux_s1           = digitalio.DigitalInOut(s1)
        self.mux_s1.direction = digitalio.Direction.OUTPUT
        self.mux_s1.value     = False

        self.mux_s2           = digitalio.DigitalInOut(s2)
        self.mux_s2.direction = digitalio.Direction.OUTPUT
        self.mux_s2.value     = False

        #default state - analog input
        self.inout = inout
        self.mux_inout        = analogio.AnalogIn(inout)

    def __del__(self):
        try:
            self.mux_s0.deinit()
            self.mux_s1.deinit()
            self.mux_s2.deinit()
        except Exception:
            pass
        try: 
            self.mux_inout.deinit()
        except Exception:
            pass
        
    #Select Line
    def muxSelectLine(self, line):
        if(line<0 or line>7):
            return; 
        self.mux_s2.value = (int(line) >> 2) & 1
        self.mux_s1.value = (int(line) >> 1) & 1
        self.mux_s0.value = (int(line) >> 0) & 1

    # Toogle mux2In, (!) Line 1 and 2 have pull-down 10k, Line 3-7 nothing
    def mux2In(self):
        self.mux_inout.deinit()
        self.mux_inout                   = digitalio.DigitalInOut(self.inout)
        self.mux_inout.direction         = digitalio.Direction.INPUT

    # Toogle mux2In, reference_voltage = 3.3V,(0-65535, 16-bit), Line 0 have res div (k=0.5)
    def mux2Analog(self):
        self.mux_inout.deinit()
        self.mux_inout                   = analogio.AnalogIn(self.inout)

    # Toogle mux2Out
    def mux2Out(self):
        self.mux_inout.deinit()
        self.mux_inout                   = digitalio.DigitalInOut(self.inout)
        self.mux_inout.direction         = digitalio.Direction.OUTPUT
