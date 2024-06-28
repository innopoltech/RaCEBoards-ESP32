import struct
import time
import math
from busio import SPI
from I2C_SPI_protocol_Base import SPI_Impl
from digitalio import DigitalInOut

from LLCC68 import *

class Ra01S:
############################################################# HI CODE ZONE
    def __init__(self, bus_implementation: SPI_Impl, cs: DigitalInOut, nRst: DigitalInOut, nInt: DigitalInOut) -> None:
        self._bus_implementation = bus_implementation

        self.SX126x_SPI_SELECT = cs
        self.SX126x_RESET      = nRst
        self.SX126x_BUSY       = nInt

        self.powerTx = 10
        self.freq    = 435E6

        self.on()

    def on(self):
        self.packet = 0
        self._begin(self.freq, self.powerTx)
                                                     #magic numbers
        self._LoRaConfig(8, 5, 4, 6, 0, True, False) #spreadingFactor, bandwidth, codingRate, preambleLength, payloadLen, crcOn, invertIrq

    def off(self):
        self.SetLowPower()

    def SetLowPower(self):
        self._SetTxPower(3)
        self.powerTx=3

    def SetMaxPower(self):
        self._SetTxPower(20) 
        self.powerTx=20 


    def SetChannel(self, ch_ : int):
        freq_=435E6 

        if(ch_ == 1):
            freq_=435E6 
        if(ch_ == 2):
            freq_=436E6 
        if(ch_ == 3):
            freq_=437E6 
        if(ch_ == 4):
            freq_=438E6 
        if(ch_ == 5):
            freq_=439E6 
        if(ch_ == 6):
            freq_=440E6 

        self.freq = freq_
        self.on() 

                                #Send
    def SendS(self, msg : str):
        byte_arr = msg.encode('ascii')
        self._Send(byte_arr, len(byte_arr), SX126x_TXMODE_SYNC) 


    def SendTelemetryPack(self, flag_0 : bool, flag_1 : bool,flag_2 : bool, height_ : float, speed_: float,
                                                                Lon_ : float, Lat_: float):
        self.packet+=1
        Len_=23 
        msg_ = bytearray(Len_)

        flag_= (flag_0 | flag_1<<1 | flag_2<<2) 

        Lon = int(Lon_*1000000.0) & 0xFFFFFFFF
        Lat = int(Lat_*1000000.0) & 0xFFFFFFFF

        msg_[0]         = ord('*')
        msg_[1:5]       = struct.pack('<I', self.packet)
        msg_[5:9]       = struct.pack('<f', height_)
        msg_[9:13]      = struct.pack('<f', speed_)  
        msg_[13:17]     = struct.pack('<I', Lon)  
        msg_[17:21]     = struct.pack('<I', Lat)  
        msg_[21]        = flag_
        msg_[22]        = ord('#')

        self._Send(msg_, Len_, SX126x_TXMODE_SYNC) 


                                #Recive
    def AvailablePacket(self):
        return  self._Available() 

    def ReciveS(self):
        int_arr, rxLen =  self._Receive(255) 
        chars = [chr(c) for c in int_arr]
        msg = ''.join(chars)
        return msg

    def ParceTelemetryPack(self):
        len_ = 23
        int_arr, rxLen =  self._Receive(255) 
        msg = ""

        if( rxLen == len_ and chr(int_arr[0]=='*') and chr(int_arr[22])=='#'):
            self.packet+=1
            
            byte_arr = bytearray(struct.pack(f"{len_}B", *int_arr)) #Transform int_array 2 byte_arr
            
            #My packet 
            msg+="R:"+str(struct.unpack('<I', byte_arr[1:5])[0])
            msg+="/"+str(self.packet) 

            #height
            msg+=":"+str(struct.unpack('<f', byte_arr[5:9])[0])

            #speed
            msg+=":"+str(struct.unpack('<f', byte_arr[9:13])[0])

            #Lon
            Buff_d=str((struct.unpack('<I', byte_arr[13:17])[0])/1000000.0 )
            msg+=":"+Buff_d
            
            #Lat
            Buff_d=str((struct.unpack('<I', byte_arr[17:21])[0])/1000000.0 )
            msg+=":"+Buff_d

            #flag
            Buff=byte_arr[21] 
            msg+=":"+str(Buff&0x1) 
            msg+=":"+str((Buff&0x2)>>1) 
            msg+=":"+str((Buff&0x4)>>2) 

            return msg 
        else:
            chars = [chr(c) for c in byte_arr]
            msg = ''.join(chars)
            return msg

############################################################# LO CODE ZONE 
    def _begin(self, frequencyInHz, txPowerInDbm):
        self.txActive          = False
        self.debugPrint        = False

        self.tcxoVoltage     = 0.0
        self.useRegulatorLDO = False

        if ( txPowerInDbm > 22 ):
            txPowerInDbm = 22
        if ( txPowerInDbm < -3 ):
            txPowerInDbm = -3
        
        self._Reset()

        wk = self._ReadRegister(SX126X_REG_LORA_SYNC_WORD_MSB,2)
        syncWord = (wk[0] << 8) + wk[1]

        if (syncWord != SX126X_SYNC_WORD_PUBLIC and syncWord != SX126X_SYNC_WORD_PRIVATE):
            print("SX126x error, maybe no SPI connection")
            return ERR_INVALID_MODE

        self._SetStandby(SX126X_STANDBY_RC)

        self._SetDio2AsRfSwitchCtrl(True)
        #set TCXO control, if requested
        if(self.tcxoVoltage > 0.0):
            self._SetDio3AsTcxoCtrl(self.tcxoVoltage, RADIO_TCXO_SETUP_TIME) #Configure the radio to use a TCXO controlled by DIO3
        
        self._Calibrate(  SX126X_CALIBRATE_IMAGE_ON
                        | SX126X_CALIBRATE_ADC_BULK_P_ON
                        | SX126X_CALIBRATE_ADC_BULK_N_ON
                        | SX126X_CALIBRATE_ADC_PULSE_ON
                        | SX126X_CALIBRATE_PLL_ON
                        | SX126X_CALIBRATE_RC13M_ON
                        | SX126X_CALIBRATE_RC64K_ON
                        )

        if (self.useRegulatorLDO):
            self._SetRegulatorMode(SX126X_REGULATOR_LDO) #set regulator mode: LDO
        else:
            self._SetRegulatorMode(SX126X_REGULATOR_DC_DC) #set regulator mode: DC-DC


        self._SetBufferBaseAddress(0, 0)
        self._SetPaConfig(0x04, 0x07, 0x00, 0x01) #PA Optimal Settings +22 dBm
        self._SetOvercurrentProtection(120.0)  #current max 120mA for the whole device
        self._SetPowerConfig(txPowerInDbm, SX126X_PA_RAMP_200U) #0 fuer Empfaenger
        self._SetRfFrequency(frequencyInHz)
        return ERR_NONE

    def _LoRaConfig(self, spreadingFactor, bandwidth, codingRate, preambleLength, payloadLen, crcOn, invertIrq):
        self._SetStopRxTimerOnPreambleDetect(False)
        self._SetLoRaSymbNumTimeout(0) 
        self._SetPacketType(SX126X_PACKET_TYPE_LORA) #SX126x.ModulationParams.PacketType : MODEM_LORA
        
        ldro = 0 #LowDataRateOptimize OFF
        self._SetModulationParams(spreadingFactor, bandwidth, codingRate, ldro)
        
        self.PacketParams = [0 for i in range(6)]

        self.PacketParams[0] = (preambleLength >> 8) & 0xFF
        self.PacketParams[1] = preambleLength

        if ( payloadLen ):
            self.PacketParams[2] = 0x01 #Fixed length packet (implicit header)
            self.PacketParams[3] = payloadLen
        else:
            self.PacketParams[2] = 0x00 #Variable length packet (explicit header)
            self.PacketParams[3] = 0xFF

        if ( crcOn ):
            self.PacketParams[4] = SX126X_LORA_IQ_INVERTED
        else:
            self.PacketParams[4] = SX126X_LORA_IQ_STANDARD

        if ( invertIrq ):
            self.PacketParams[5] = 0x01 #Inverted LoRa I and Q signals setup
        else:
            self.PacketParams[5] = 0x00 #Standard LoRa I and Q signals setup

        #fixes IQ configuration for inverted IQ
        self._FixInvertedIQ(self.PacketParams[5])

        self._WriteCommand(SX126X_CMD_SET_PACKET_PARAMS, self.PacketParams, 6) #0x8C

        # #if 0
        # SetDioIrqParams(SX126X_IRQ_ALL,  #all interrupts enabled
        #                 (SX126X_IRQ_RX_DONE | SX126X_IRQ_TX_DONE | SX126X_IRQ_TIMEOUT), #interrupts on DIO1
        #                 SX126X_IRQ_NONE,  #interrupts on DIO2
        #                 SX126X_IRQ_NONE) #interrupts on DIO3
        # #endif
        #Do not use DIO interruptst
        self._SetDioIrqParams(SX126X_IRQ_ALL,   #all interrupts enabled
                        SX126X_IRQ_NONE,  #interrupts on DIO1
                        SX126X_IRQ_NONE,  #interrupts on DIO2
                        SX126X_IRQ_NONE) #interrupts on DIO3

        #Receive state no receive timeoout
        self._SetRx(0xFFFFFF)

    def _Receive(self, max_len):
        irqRegs = self._GetIrqStatus()

        if( irqRegs & SX126X_IRQ_RX_DONE ):
            self._ClearIrqStatus(SX126X_IRQ_ALL)
            rxData, rxLen = self._ReadBuffer(max_len)
        return rxData, rxLen

    def _Send(self, Data, len, mode):
        irqStatus = 0
        rv = False
    
        if ( self.txActive == False ):
            self.txActive = True
            self.PacketParams[2] = 0x00 #Variable length packet (explicit header)
            self.PacketParams[3] = len
            self._WriteCommand(SX126X_CMD_SET_PACKET_PARAMS, self.PacketParams, 6) #0x8C

            #ClearIrqStatus(SX126X_IRQ_TX_DONE | SX126X_IRQ_TIMEOUT)
            self._ClearIrqStatus(SX126X_IRQ_ALL)

            self._WriteBuffer(Data, len)
            self._SetTx(500)

            if ( mode & SX126x_TXMODE_SYNC ):
                timeout = time.monotonic() + 0.1
                irqStatus = self._GetIrqStatus()

                while ( ( not(irqStatus & SX126X_IRQ_TX_DONE)) and ( not (irqStatus & SX126X_IRQ_TIMEOUT)) ):
                    time.sleep(0.001)
                    irqStatus = self._GetIrqStatus()
                    if(time.monotonic() > timeout ):
                        print("SetTx Timeout")
                        break

                self.txActive = False

                self._SetRx(0xFFFFFF)

                if ( irqStatus & SX126X_IRQ_TX_DONE):
                    rv = True
            else:
                rv = True
        return rv

    def _Available(self):
        irqRegs = self._GetIrqStatus()
        return ( (irqRegs & SX126X_IRQ_RX_DONE) != 0)

    def _SetTxPower(self, txPowerInDbm):
        self._SetPowerConfig(txPowerInDbm, SX126X_PA_RAMP_200U)

#############################################################LO LO CODE ZONE
    def _SetTx(self, timeoutInMs):
        self._SetStandby(SX126X_STANDBY_RC)
        self._SetTxEnable()

        buf = [0,0,0]
        tout = timeoutInMs

        if (timeoutInMs != 0):
            #Timeout duration = Timeout * 15.625 ��s
            timeoutInUs = int(timeoutInMs * 1000)
            tout = int(timeoutInUs / 15.625)

        buf[0] = (tout >> 16) & 0xFF
        buf[1] = (tout >> 8) & 0xFF
        buf[2] = tout & 0xFF
        self._WriteCommand(SX126X_CMD_SET_TX, buf, 3) #0x83

        for retry in range(0,10):
            status_ = self._GetStatus() 
            if ((status_ & 0x70) == 0x60):
                break
            time.sleep(0.002)

        if ((status_ & 0x70) != 0x60):
            print("SetTx Illegal Status")
            return ERR_INVALID_MODE
        return ERR_NONE

    def _SetRx(self, timeout):
        self._SetStandby(SX126X_STANDBY_RC)
        self._SetRxEnable()

        buf = [0,0,0]
        buf[0] = (timeout >> 16) & 0xFF
        buf[1] = (timeout >> 8) & 0xFF
        buf[2] = timeout & 0xFF
        self._WriteCommand(SX126X_CMD_SET_RX, buf, 3) #0x82

        for retry in range(0,10):
            if ((self._GetStatus() & 0x70) == 0x50):
                break
            time.sleep(0.001)

        if ((self._GetStatus() & 0x70) != 0x50):
            print("SetRx Illegal Status")

    def _SetDioIrqParams( self, irqMask, dio1Mask, dio2Mask, dio3Mask ):
        buf = [0,0,0,0,0,0,0,0]

        buf[0] = (irqMask >> 8) & 0x00FF
        buf[1] = irqMask & 0x00FF
        buf[2] = (dio1Mask >> 8) & 0x00FF
        buf[3] = dio1Mask & 0x00FF
        buf[4] = (dio2Mask >> 8) & 0x00FF
        buf[5] = dio2Mask & 0x00FF
        buf[6] = (dio3Mask >> 8) & 0x00FF
        buf[7] = dio3Mask & 0x00FF
        self._WriteCommand(SX126X_CMD_SET_DIO_IRQ_PARAMS, buf, 8) #0x08

    def _FixInvertedIQ(self, iqConfig):
        #fixes IQ configuration for inverted IQ
        #see SX1262/SX1268 datasheet, chapter 15 Known Limitations, section 15.4 for details
        #When exchanging LoRa packets with inverted IQ polarity, some packet losses may be observed for longer packets.
        #Workaround: Bit 2 at address 0x0736 must be set to:
        #��0�� when using inverted IQ polarity (see the SetPacketParam(...) command)
        #��1�� when using standard IQ polarity

        #read current IQ configuration
        iqConfigCurrent = self._ReadRegister(SX126X_REG_IQ_POLARITY_SETUP, 1)[0] #0x0736

        #set correct IQ configuration
        if(iqConfig == SX126X_LORA_IQ_STANDARD):
            iqConfigCurrent &= 0xFB
        else:
            iqConfigCurrent |= 0x04

        #update with the new value
        self._WriteRegister(SX126X_REG_IQ_POLARITY_SETUP, [iqConfigCurrent], 1) #0x0736
    
    def _CalibrateImage(self, frequency):
        calFreq = [0,0]

        if( frequency> 900000000 ):
            calFreq[0] = 0xE1
            calFreq[1] = 0xE9
        elif( frequency > 850000000 ):
            calFreq[0] = 0xD7
            calFreq[1] = 0xD8
        elif( frequency > 770000000 ):
            calFreq[0] = 0xC1
            calFreq[1] = 0xC5
        elif( frequency > 460000000 ):
            calFreq[0] = 0x75
            calFreq[1] = 0x81
        else :# ( frequency > 425000000 ):
            calFreq[0] = 0x6B
            calFreq[1] = 0x6F
       
        self._WriteCommand(SX126X_CMD_CALIBRATE_IMAGE, calFreq, 2) #0x98

    def _SetDio3AsTcxoCtrl(self, voltage, delay):
        buf = [0,0,0,0]

        #buf[0] = tcxoVoltage & 0x07
        if(math.fabs(voltage - 1.6) <= 0.001):
            buf[0] = SX126X_DIO3_OUTPUT_1_6
        elif(math.fabs(voltage - 1.7) <= 0.001):
            buf[0] = SX126X_DIO3_OUTPUT_1_7
        elif(math.fabs(voltage - 1.8) <= 0.001):
            buf[0] = SX126X_DIO3_OUTPUT_1_8
        elif(math.fabs(voltage - 2.2) <= 0.001):
            buf[0] = SX126X_DIO3_OUTPUT_2_2
        elif(math.fabs(voltage - 2.4) <= 0.001):
            buf[0] = SX126X_DIO3_OUTPUT_2_4
        elif(math.fabs(voltage - 2.7) <= 0.001):
            buf[0] = SX126X_DIO3_OUTPUT_2_7
        elif(math.fabs(voltage - 3.0) <= 0.001):
            buf[0] = SX126X_DIO3_OUTPUT_3_0
        else:
            buf[0] = SX126X_DIO3_OUTPUT_3_3
        
        delayValue = int(delay / 15.625)
        buf[1] = ( delayValue >> 16 ) & 0xFF
        buf[2] = ( delayValue >> 8 ) & 0xFF
        buf[3] = delayValue & 0xFF

        self._WriteCommand(SX126X_CMD_SET_DIO3_AS_TCXO_CTRL, buf, 4) #0x97

    def _SetRfFrequency(self, frequency : float):
        buf = [0,0,0,0]
        freq = 0

        self._CalibrateImage(frequency)

        freq = int(frequency / FREQ_STEP)
        buf[0] = (freq >> 24) & 0xFF
        buf[1] = (freq >> 16) & 0xFF
        buf[2] = (freq >> 8) & 0xFF
        buf[3] = freq & 0xFF
        self._WriteCommand(SX126X_CMD_SET_RF_FREQUENCY, buf, 4) #0x86

    def _ClearIrqStatus(self, irq):
        buf = [0,0]
        buf[0] = (irq >> 8) & 0xFF
        buf[1] = (irq >> 0) & 0xFF
        self._WriteCommand(SX126X_CMD_CLEAR_IRQ_STATUS, buf, 2) #0x02

    def _SetPowerConfig(self, power, rampTime):
        if( power > 22 ):
            power = 22
        elif( power < -3 ):
            power = -3
        
        buf = [power, rampTime & 0xFF]
        self._WriteCommand(SX126X_CMD_SET_TX_PARAMS, buf, 2) #0x8E

    def _SetOvercurrentProtection(self, currentLimit):
        if((currentLimit <= 0.0) or (currentLimit >= 140.0)):
            return
        buf = [int(currentLimit / 2.5)]
        self._WriteRegister(SX126X_REG_OCP_CONFIGURATION, buf, 1) #0x08E7

    def _SetModulationParams(self, spreadingFactor, bandwidth, codingRate, lowDataRateOptimize):
        buf = [spreadingFactor, bandwidth, codingRate, lowDataRateOptimize]
        self._WriteCommand(SX126X_CMD_SET_MODULATION_PARAMS, buf, 4) #0x8B

    def _SetPaConfig(self, paDutyCycle, hpMax, deviceSel, paLut):
        buf = [paDutyCycle, hpMax, deviceSel, paLut]
        self._WriteCommand(SX126X_CMD_SET_PA_CONFIG, buf, 4) #0x95

    def _SetBufferBaseAddress(self, txBaseAddress, rxBaseAddress):
        buf = [txBaseAddress, rxBaseAddress]
        self._WriteCommand(SX126X_CMD_SET_BUFFER_BASE_ADDRESS, buf, 2) #0x8F

    def _GetRxBufferStatus(self):
        buf = self._ReadCommand( SX126X_CMD_GET_RX_BUFFER_STATUS, 3 ); #0x13
        return buf[1], buf[2];

    def _SetPacketType(self, packetType):
        self._WriteCommand(SX126X_CMD_SET_PACKET_TYPE, [packetType], 1) #0x01

    def _SetLoRaSymbNumTimeout(self, SymbNum):
        self._WriteCommand(SX126X_CMD_SET_LORA_SYMB_NUM_TIMEOUT, [SymbNum], 1) #0xA0

    def _SetStopRxTimerOnPreambleDetect(self, enable):
        self._WriteCommand(SX126X_CMD_STOP_TIMER_ON_PREAMBLE, [enable], 1) #0x9F

    def _SetRegulatorMode(self, mode):
        self._WriteCommand(SX126X_CMD_SET_REGULATOR_MODE, [mode], 1) #0x96

    def _Calibrate(self, calibParam):
        self._WriteCommand(SX126X_CMD_CALIBRATE, [calibParam], 1) #0x89

    def _SetDio2AsRfSwitchCtrl(self, enable):
        self._WriteCommand(SX126X_CMD_SET_DIO2_AS_RF_SWITCH_CTRL, [enable], 1) #0x9D

    def _SetStandby(self, mode):
        self._WriteCommand(SX126X_CMD_SET_STANDBY, [mode], 1) #0x80

    def _GetStatus(self):
        return self._ReadCommand(SX126X_CMD_GET_STATUS, 1)[0]  #0xC0

    def _GetIrqStatus(self):
        data = self._ReadCommand(SX126X_CMD_GET_IRQ_STATUS, 3) #0x12
        return (data[1] << 8) | data[2]

    def _SetRxEnable(self):
        pass

    def _SetTxEnable(self):
        pass

    def _Reset(self):
        time.sleep(0.01)
        self.SX126x_RESET.value = False
        time.sleep(0.02)
        self.SX126x_RESET.value = True
        time.sleep(0.01)
        #ensure BUSY is low (state meachine ready)
        self._WaitForIdle()

    def _WaitForIdle(self, timeout = 1000):
        start = int(time.monotonic()*1000)
        time.sleep(0.001)
        while(self.SX126x_BUSY.value):
            time.sleep(0.001)
            if(int(time.monotonic()*1000) - start >= timeout):
                break

############################################################# HI Protocol zone

    def _ReadRegister(self, register, lenght):
        data = [0 for i in range(lenght)]

         #ensure BUSY is low (state meachine ready)
        self._WaitForIdle()

        #start transfer
        self.SX126x_SPI_SELECT.value = False
        self._lock() 

        #send command byte
        self._write_byte(SX126X_CMD_READ_REGISTER) #0x1D
        self._write_byte((register & 0xFF00) >> 8)
        self._write_byte(register & 0xff)
        self._write_byte(SX126X_CMD_NOP)

        for n in range(0,lenght):
            data[n] = self._read_write_byte(SX126X_CMD_NOP)

        #stop transfer
        self.SX126x_SPI_SELECT.value = True
        self._unlock()

        #wait for BUSY to go low
        self._WaitForIdle()

        return data

    def _WriteRegister(self, reg, data, numBytes):
        #ensure BUSY is low (state meachine ready)
        self._WaitForIdle()

        #start transfer
        self.SX126x_SPI_SELECT.value = False
        self._lock() 

        #send command byte
        self._write_byte(SX126X_CMD_WRITE_REGISTER) #0x0D
        self._write_byte((reg & 0xFF00) >> 8)
        self._write_byte(reg & 0xff)
        
        for n in range(0, numBytes):
            self._write_byte(data[n])

        #stop transfer
        self.SX126x_SPI_SELECT.value = True
        self._unlock()

        #wait for BUSY to go low
        self._WaitForIdle()

    def _ReadCommand(self, cmd, numBytes):
        data = [0 for i in range(numBytes)]

        #ensure BUSY is low (state meachine ready)
        self._WaitForIdle()

        #start transfer
        self.SX126x_SPI_SELECT.value = False
        self._lock() 

        self._write_byte(cmd)

        #send/receive all bytes
        for n in range(0,numBytes):
            data[n] = self._read_write_byte(SX126X_CMD_NOP)

        #stop transfer
        self.SX126x_SPI_SELECT.value = True
        self._unlock()

        #wait for BUSY to go low
        self._WaitForIdle()

        return data

    def _WriteCommand(self,cmd, data, numBytes):
        #ensure BUSY is low (state meachine ready)
        self._WaitForIdle()

        #start transfer
        self.SX126x_SPI_SELECT.value = False
        self._lock() 

        #send command byte
        self._write_byte(cmd)

        #variable to save error during SPI transfer
        status  = 0

        #send/receive all bytes
        for n in range(0,numBytes):
            ret = self._read_write_byte(data[n])

            if(ret == 0x00 or ret == 0xFF):
                status = SX126X_STATUS_SPI_FAILED
                break

            #check status
            ret &= 0xE
            if( (ret == SX126X_STATUS_CMD_TIMEOUT) or \
                (ret == SX126X_STATUS_CMD_INVALID) or \
                (ret == SX126X_STATUS_CMD_FAILED)):
                status = ret
                break

        #stop transfer
        self.SX126x_SPI_SELECT.value = True
        self._unlock()

        #wait for BUSY to go low
        self._WaitForIdle()

        if (status != 0):
            print(f"SPI Transaction error: {status}")

    def _ReadBuffer(self, maxLen):
        payloadLength, offset = self._GetRxBufferStatus()

        if( payloadLength > maxLen ):
            print("ReadBuffer maxLen too small")
            return 0

        #ensure BUSY is low (state meachine ready)
        self._WaitForIdle()

        #start transfer
        self.SX126x_SPI_SELECT.value = False
        self._lock() 

        self._write_byte(SX126X_CMD_READ_BUFFER)
        self._write_byte(offset)
        self._write_byte(SX126X_CMD_NOP)

        rxData  = [0 for i in range(payloadLength)]
        for n in range(0,payloadLength):
            rxData[n] = self._read_write_byte(SX126X_CMD_NOP)  

        #stop transfer
        self.SX126x_SPI_SELECT.value = True
        self._unlock()

        #wait for BUSY to go low
        self._WaitForIdle()

        return rxData, payloadLength

    def _WriteBuffer(self, txData, txDataLen):
        #ensure BUSY is low (state meachine ready)
        self._WaitForIdle()

        #start transfer
        self.SX126x_SPI_SELECT.value = False
        self._lock() 

        self._write_byte(SX126X_CMD_WRITE_BUFFER) #0x0E
        self._write_byte(0)

        for n in range(0,txDataLen):
            self._write_byte(txData[n])

        #stop transfer
        self.SX126x_SPI_SELECT.value = True
        self._unlock()

        #wait for BUSY to go low
        self._WaitForIdle()

############################################################# LO Protocol zone

    def _write_byte(self, value :int):
        self._bus_implementation.write_byte(value)

    def _read_write_byte(self, value :int):
        return self._bus_implementation.read_write_byte(value)
    
    def _lock(self):
        return self._bus_implementation.lock()

    def _unlock(self):
        self._bus_implementation.unlock()


class Ra01S_SPI(Ra01S):
    def __init__(self, spi: SPI, cs: DigitalInOut, nRst : DigitalInOut, nInt : DigitalInOut, baudrate: int = 100000) -> None:
        super().__init__(SPI_Impl(spi, None, baudrate), cs, nRst, nInt) #We don't hand over the CS, because we control it ourselves
