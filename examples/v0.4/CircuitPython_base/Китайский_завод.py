import math
import os
import time
import board
import pwmio
import busio
import digitalio
import pwmio
import analogio
import sdcardio
import storage

from lib import BMP280
from lib import BUZZER
from lib import DC_MOTOR
from lib import GNSS
from lib import GPIO_IO
from lib import GPIO_MUX
from lib import LSM6DSL
from lib import QMC5883L
from lib import Ra01S
from lib import SERVO

################################### Setup BUTTON_LED
print("\t\t\t###\t\t\t\n")
print("Проверка кнопки и светодиода")

#Create button object
button           = digitalio.DigitalInOut(board.IO9)
button.direction = digitalio.Direction.INPUT
button.pull      = digitalio.Pull.UP

#Create led object
led = digitalio.DigitalInOut(board.IO8)
led.direction = digitalio.Direction.OUTPUT

################################### Work BUTTON_LED
print("Жмите кнопку в течении 3 секунд, светодиод дожен гореть")
start = False
i = 0
while True:
    if(not button.value):
        if(not start):
            print("Кнопка нажата!")
            start = True
        if(i<30):
            i+=1
        else:
            break
    elif(start):
        print("Кнопка отпущена раньше времени!")
        start = False
        i = 0
    led.value = not button.value
    time.sleep(0.1)
print("Проверка кнопки и светодиода: ЗАВЕРШЕНА")


################################### Setup BUZZER
print("\t\t\t###\t\t\t\n")
print("Проверка бузера")

buzz = BUZZER.BUZZ(pin=board.IO4, freq=1000)

################################### Work BUZZER
for i in range(0,5):
    time.sleep(0.5)
    buzz.BuzzerOn()
    time.sleep(0.5)
    buzz.BuzzerOff()
print("Проверка бузера: ЗАВЕРШЕНА")


################################### Setup   MUX
print("\t\t\t###\t\t\t\n")
print(f"Проверка MUX")
mux = GPIO_MUX.MUX(s0=board.IO46, s1=board.IO0, s2=board.IO39, inout=board.IO1)

################################### Work MUX
#Switch pin mode
mux.mux2Analog()
#Select Line 
mux.muxSelectLine(0)

#Calculate
raw_bit  = mux.mux_inout.value
percent  = raw_bit/65535.0
raw_volt = percent*mux.mux_inout.reference_voltage
volt     = raw_volt*2.0
print(f"Показания аккумулятора с MUX: {raw_bit} lsb, Вход %: {percent}, Чистое напряжение: {raw_volt}, С учетом делителя: {volt}")
print(f"Проверка MUX : ЗАВЕРШЕНА")
time.sleep(1)

################################### Setup IO
print("\t\t\t###\t\t\t\n")
print("Проверка IO")

#Create i2c module object
i2c1_module = busio.I2C(scl=board.IO2, sda=board.IO3)

io_pins = GPIO_IO.IO_I2C(i2c1_module)

#Get pin (0...7)
#The pins can only operate in BASIC mode and do not support things like PWM ...
pin_test_0_out = io_pins.get_pin(0)
pin_test_1_in = io_pins.get_pin(1)

#Set directions
#Short-circuit pins 0 and 1!
pin_test_0_out.direction = digitalio.Direction.OUTPUT
pin_test_1_in.direction = digitalio.Direction.INPUT

################################### Work IO
print("Замкните IO0 и IO1 между собой") 

print("Set pin_0 to HI")
pin_test_0_out.value = 1

for i in range(2):
    val = pin_test_1_in.value
    if(val != 1):
        raise Exception("ошибка IO! состояния пинов не совпадают!")
    print(f"\tRead pin_1 = {val}")
    time.sleep(0.5)

print("Set pin_0 to LO")
pin_test_0_out.value = 0

for i in range(2):
    val = pin_test_1_in.value
    if(val != 0):
        raise Exception("ошибка IO! состояния пинов не совпадают!")
    print(f"\tRead pin_1 = {val}")
    time.sleep(0.5)

print(f"Проверка IO : ЗАВЕРШЕНА")
time.sleep(1)
################################### Setup DC Motor
print("\t\t\t###\t\t\t\n")
print(f"Проверка Мотора")

#MOTOR A
# xIN1_PWM_pin  = board.IO21
# xIN2_GPIO_pin = board.IO47

#MOTOR B - shared with servo 3/4
xIN1_PWM_pin  = board.IO42
xIN2_GPIO_pin = board.IO41

dc_mot = DC_MOTOR.MOTOR(xIN1_PWM_pin, xIN2_GPIO_pin)

################################### Work  DC Motor
#Start point
speed = 0
dir   = 1

print("Вращаем мотором, в обе стороны")
while True:
    time.sleep(0.05)
    #Change direction
    if(dir == 1  and speed == 100):
        dir = -1
    if(dir == -1 and speed == -100):
        dir = 1
        dc_mot.DCMotorSetSpeed(0)
        break

    #Change speed
    speed += dir
    dc_mot.DCMotorSetSpeed(speed)
print(f"Проверка Мотора : ЗАВЕРШЕНА")
time.sleep(1)

################################### Setup Servos
print("\t\t\t###\t\t\t\n")
print(f"Проверка Servos")

# Servo 1
PWM1_pin = board.IO48
# Servo 2
PWM2_pin = board.IO45
# # Servo 3 - shared with MOTOR B
# PWM3_pin = board.IO42
# # Servo 4 - shared with MOTOR B
# PWM4_pin = board.IO41

servo1 = SERVO.SERVO(PWM1_pin)
servo2 = SERVO.SERVO(PWM2_pin)

################################### Work Servos
print("Вращаем сервами, в обе стороны")
for i in range(0,5):
    time.sleep(1)
    servo1.ServoSetAngle(0)
    servo1.ServoSetAngle(180)
    time.sleep(1)
    servo1.ServoSetAngle(180)
    servo1.ServoSetAngle(0)

print(f"Проверка Servos : ЗАВЕРШЕНА")
time.sleep(1)

################################### Setup GNSS
print("\t\t\t###\t\t\t\n")
print(f"Проверка GNSS (без ожидания спутников)")

#Create gnss_uart module object
gnss_uart = busio.UART(board.IO17, board.IO18, baudrate=9600, timeout=10)

#Create gnss module object
gnss = GNSS.GPS(gnss_uart, debug=False)
gnss.send_command(b'PMTK314,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0')
# Set update rate to once a second (default - 1hz)
gnss.send_command(b"PMTK220,1000")

################################### Work GNSS

check = gnss_uart.readline()
if(check is not None and len(check)>6):
    print( f"GNSS, 5 строк : \n{gnss_uart.readline()}\n\
    {gnss_uart.readline()}\n\
    {gnss_uart.readline()}\n\
    {gnss_uart.readline()}\n\
    {gnss_uart.readline()}")
else:
    print("Нет данных от GNSS!")
    time.sleep(1)
print(f"Проверка GNSS : ЗАВЕРШЕНА")
time.sleep(1)

################################### Setup BMP280
print("\t\t\t###\t\t\t\n")
print(f"Проверка BMP280")

#Create i2c module object
# i2c1_module = busio.I2C(scl=board.IO2, sda=board.IO3)
#Create bmp280 object
bmp280      = BMP280.Adafruit_BMP280_I2C(i2c1_module, BMP280._BMP280_ADDRESS)
#Set zero pressure
bmp280.sea_level_pressure = 101.325 #kPa

################################### Work BMP280
#Get data from device
for i in range(0,10):
    temp    = bmp280.temperature()
    press   = bmp280.pressure()
    alt     = bmp280.altitude()
    time.sleep(0.1)

all_ok = "все хорошо"
if(temp<-20 or temp>40 or press<90 or press>130 or alt<-200 or alt>500):
    all_ok = "не работает"

print(f"Показания - температура: {temp} град, Давление: {press} кПа, Высота: {alt} м.; Результат - {all_ok}")
print(f"Проверка BMP280 : ЗАВЕРШЕНА")
time.sleep(1)
################################### Setup LSM6DSL
print("\t\t\t###\t\t\t\n")
print(f"Проверка LSM6DSL")

#Create i2c module object
#i2c1_module = busio.I2C(scl=board.IO2, sda=board.IO3)
#Create lsm6dsl object
lsm6dsl     = LSM6DSL.LSM6DSL_I2C(i2c1_module, LSM6DSL.LSM6DSL_DEFAULT_ADDRESS)

################################### Work LSM6DSL
#Get data from device
for i in range(0,10):
    acc = lsm6dsl.acceleration()
    gyro = lsm6dsl.gyro()
    temp = lsm6dsl.temperature()
    time.sleep(0.5)

acc_m = math.sqrt(acc[0]*acc[0]+ acc[1]*acc[1]+ acc[2]*acc[2])
gyro_m = math.sqrt(gyro[0]*gyro[0]+ gyro[1]*gyro[1]+ gyro[2]*gyro[2])

all_ok = "все хорошо"
if(temp<-20 or temp>40 or acc_m<-15 or acc_m>15 or gyro_m<-50 or gyro_m>50):
    all_ok = "не работает"

print(f"Показания - температура: {temp} град, Магнитоное поле: {acc_m} м/с, Угл. скорость: {gyro_m} град/с; Результат - {all_ok}")
print(f"Проверка LSM6DSL : ЗАВЕРШЕНА")
time.sleep(1)
################################### Setup QMC5883l
print("\t\t\t###\t\t\t\n")
print(f"Проверка QMC5883l")

#Create i2c module object
#i2c1_module = busio.I2C(scl=board.IO2, sda=board.IO3)
#Create qmc5883l object
qmc5883l     = QMC5883L.QMC5883L_I2C(i2c1_module, QMC5883L.QMC5883L_DEFAULT_ADDRESS)

################################### Work QMC5883l
for i in range(0,10):
    #Get data from device
    temp = qmc5883l.temperature()   #need offset
    mag = qmc5883l.magnetometer()
    time.sleep(0.5)

    mag_m = math.sqrt(mag[0]*mag[0]+ mag[1]*mag[1]+ mag[2]*mag[2])

all_ok = "все хорошо"
if(temp<-10 or temp>10):
    all_ok = "не работает"

print(f"Показания - температура(без смещения): {temp} град, Ускорение: {mag_m} мТ; Результат - {all_ok}")
print(f"Проверка QMC5883l : ЗАВЕРШЕНА")
time.sleep(1)

################################### Setup SDCard
print("\t\t\t###\t\t\t\n")
print(f"Проверка SDCard")

my_file = "test.txt"

#Create spi module object
spi0_module     = busio.SPI(clock=board.IO12, MOSI=board.IO11, MISO=board.IO13)
#Create cs object
sd_card_cs_pin  = board.IO15

#Create SDCard object
spi0_speed = 2000000
sd_card = sdcardio.SDCard(spi0_module, sd_card_cs_pin, spi0_speed)
#Create FileSystem object
vfs     = storage.VfsFat(sd_card)
#Mount FileSystem
storage.mount(vfs, '/sd')

################################### Work SDCard
print("Записываем файл")
#Open File to write (append mode)
f = open("/sd/"+my_file, "a")
#Write (next line = "\r\n")
f.write("Hello, world!\r\n")
f.close()

print("Читаем файл")
#Open File to read 
f = open("/sd/"+my_file, "r")
#Read All ((!)Only 5-6 MB RAM available(!))
contents = f.read()
print(contents)
f.close()

storage.umount('/sd')

print(f"Проверка SDCard : ЗАВЕРШЕНА")
time.sleep(1)
################################### Setup Ra01S
print("\t\t\t###\t\t\t\n")
print(f"Проверка Ra01S")

# #Create spi module object
# spi0_speed = 2000000
# spi0_module     = busio.SPI(clock=board.IO12, MOSI=board.IO11, MISO=board.IO13)

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

################################### Work Ra01S
print("Отсылаем 5 пакетов в космос...")
for i in range(0,5):
    Ra01S.SendS("Hello world!\n")
    time.sleep(0.5)
print(f"Проверка Ra01S : ЗАВЕРШЕНА")
time.sleep(1)