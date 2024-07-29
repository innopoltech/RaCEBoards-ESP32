import time
import board
import adafruit_wave
import espidf
import audiocore
import audiomixer
import audiobusio

def print_progress_bar(name, progress, total, bar_length=50):
    percent = int((progress / total) * 100)
    bar = '#' * int(bar_length * percent / 100) + '-' * (bar_length - int(bar_length * percent / 100))
    print(f'\x1b[A\r {name}: [{bar}] {percent}%', f"Free heap = {espidf.heap_caps_get_free_size()/1000} kB")
    #print(f'{name}: [{bar}] {percent}%')

################################### Setup

IS_MP3 = True
IS_I2S = False


if(IS_MP3 == False):
    #ONLY WAV FILE = 16bit and 16000 framerate

    #Open file
    my_file = adafruit_wave.open("wav_example.wav")

    #Get params
    samples_fr = my_file.getframerate()
    channels = my_file.getnchannels()
    samples_len = int(my_file.getnframes() / channels)
    samples_delay = 1.0/samples_fr
    samples_delay_fix_us = samples_delay*1_000_000
    sample_size = my_file.getsampwidth()*8
    duration = (samples_delay_fix_us * samples_len)/1_000_000

    print(f"Total len - {my_file.getnframes()}, sample rate - {samples_fr}, channels - {channels}, sample size - {my_file.getsampwidth()}")
    print(f"Total samples - {samples_len}, frame_timestep - {samples_delay_fix_us}")

    #Load music
    print(f"Free heap = {espidf.heap_caps_get_free_size()/1000} kB")
    sound_reads = my_file.readframes(samples_len)
    sound_m = memoryview(sound_reads).cast("h")
    print(f"Free heap = {espidf.heap_caps_get_free_size()/1000} kB")

    #Create rawsample
    sound = audiocore.RawSample(sound_m, sample_rate=samples_fr)
else:
    #A custom CircuitPython kernel is required!!!
    import audiomp3

    #We can't know the duration of mp3 file
    duration = 120

    #MP3 
    print(f"Free heap = {espidf.heap_caps_get_free_size()/1000} kB")
    sound = audiomp3.MP3Decoder("mp3_example.mp3")
    print(f"Free heap = {espidf.heap_caps_get_free_size()/1000} kB")

    #Get params
    samples_fr = sound.sample_rate
    channels = sound.channel_count
    sample_size = sound.bits_per_sample
    print(f"Sample rate - {samples_fr}, channels - {channels}, sample size - {sample_size}")
    

#Setup mixer to adjust the playback volume
volume = 1.0
print(f"Set volume = {volume}")
mixer = audiomixer.Mixer(voice_count=1, sample_rate=samples_fr, channel_count=channels,
                         bits_per_sample=sample_size, samples_signed=True)
mixer.voice[0].level = volume

if(IS_I2S == False):
    #A custom CircuitPython kernel is required!!!
    import audiopwmio
    #Create PseudoDAC, shared with servo 3/4 and motor B
    #Speaker and right channel - board.IO41 
    #Left channel - board.IO42
    sound_out = audiopwmio.PWMAudioOut(board.IO42, right_channel=board.IO41)
else:
    #Create I2S, shared with servo 1/2 and motor A (pin2)
    sound_out = audiobusio.I2SOut(bit_clock=board.IO47, word_select=board.IO48, data=board.IO45)

################################### Work
#Start play
print("Play")

sound_out.play(mixer)
mixer.voice[0].play(sound)

#Calculate play time
start_play = time.time()
stop_play = duration

#Status bar
try:
    while( time.time()-start_play < stop_play):
        print_progress_bar("Playing",time.time()-start_play, stop_play)
        time.sleep(1)
except KeyboardInterrupt:
    print("Stop from keyboard")

#Stop playing
mixer.voice[0].stop()
sound_out.stop()

print("Stop playing! Stop programm...")
time.sleep(1)
