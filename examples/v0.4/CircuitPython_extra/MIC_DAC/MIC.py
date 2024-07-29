import time
import board
import array
import audiocore
import analogbufio
import audiopwmio   #Modified python kernel is required
import digitalio

#Shared with aV_Bat in Copter board
mic_pin = board.IO10

#Setup buffer
len_sec = 5
rate = 48000
length = len_sec*rate
adc_buff = array.array("H", [0x00] * length)

#Create buffered_in
adc_in = analogbufio.BufferedIn(mic_pin, sample_rate=rate)

#Create button
button           = digitalio.DigitalInOut(board.IO9)
button.direction = digitalio.Direction.INPUT
button.pull      = digitalio.Pull.UP

#Create PseudoDAC, shared with servo 3/4 and motor B
#Speaker and right channel - board.IO41 
#Left channel - board.IO42
sound_out = audiopwmio.PWMAudioOut(board.IO42, right_channel=board.IO41)
sound = audiocore.RawSample(adc_buff, sample_rate=rate)

#Record-Play loop
try:
    print("Please press the button...")
    while(True):
        if(not button.value):
            print(f"Start record {len_sec} sec!")
            adc_in.readinto(adc_buff)
            print(f"Play recorder {len_sec} sec!")
            sound_out.play(sound)
            time.sleep(len_sec)
            sound_out.stop()
            print(f"All done!\n#####")
except Exception as e:
    raise e
finally:
    adc_in.deinit()
    sound_out.stop()