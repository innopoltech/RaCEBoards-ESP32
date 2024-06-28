import time
import board
import busio
import sys

from lib import GNSS

################################### Setup
#initial delay
time.sleep(1)
#Create gnss_uart module object
gnss_uart = busio.UART(board.IO17, board.IO18, baudrate=9600, timeout=10)

# !Debug! raw out from gnss module
# while True:
#     print(uart.readline())

# !Debug! raw gnss <-> pc (uart0)
# main_uart = busio.UART(board.TX, board.RX, baudrate=9600, timeout=10)
# while True:
#     if(gnss_uart.in_waiting):
#         main_uart.write(gnss_uart.read(1))
#     if(main_uart.in_waiting):
#         gnss_uart.write(main_uart.read(1))

#Create gnss module object
gnss = GNSS.GPS(gnss_uart, debug=False)

# Turn on the basic GGA and RMC info (default)
gnss.send_command(b"PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0")
# Turn on just minimum info (RMC only, location):
#gnss.send_command(b'PMTK314,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0')
# Turn off everything:
# gnss.send_command(b'PMTK314,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0')
# Turn on everything (not all of it is parsed!)
# gnss.send_command(b'PMTK314,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0')

# Set update rate to once a second (default - 1hz)
gnss.send_command(b"PMTK220,1000")

last_print = time.monotonic()

################################### Work
while True:
    # Make sure to call gnss.update() every loop iteration and at least twice
    # as fast as data comes from the gnss unit (usually every second).
    # This returns a bool that's true if it parsed new data (you can ignore it
    # though if you don't care and instead look at the has_fix property).
    gnss.update()

    current = time.monotonic()
    if current - last_print >= 1.0:
        last_print = current
        #no valid data
        if not gnss.has_fix:
            print("Waiting for fix...")
            continue
        
        #valid data, print everything
        print("=" * 40)
        print(
            "Fix timestamp: {}/{}/{} {:02}:{:02}:{:02}".format(
                gnss.timestamp_utc.tm_mon, 
                gnss.timestamp_utc.tm_mday, 
                gnss.timestamp_utc.tm_year,
                gnss.timestamp_utc.tm_hour,
                gnss.timestamp_utc.tm_min,
                gnss.timestamp_utc.tm_sec,
            )
        )
        print("Latitude: {0:.6f} degrees".format(gnss.latitude))
        print("Longitude: {0:.6f} degrees".format(gnss.longitude))
        print(
            "Precise Latitude: {:2.}{:2.4f} degrees".format(
                gnss.latitude_degrees, gnss.latitude_minutes
            )
        )
        print(
            "Precise Longitude: {:2.}{:2.4f} degrees".format(
                gnss.longitude_degrees, gnss.longitude_minutes
            )
        )
        print("Fix quality: {}".format(gnss.fix_quality))

        if gnss.satellites is not None:
            print("# satellites: {}".format(gnss.satellites))
        if gnss.altitude_m is not None:
            print("Altitude: {} meters".format(gnss.altitude_m))
        if gnss.speed_knots is not None:
            print("Speed: {} knots".format(gnss.speed_knots))
        if gnss.track_angle_deg is not None:
            print("Track angle: {} degrees".format(gnss.track_angle_deg))
        if gnss.horizontal_dilution is not None:
            print("Horizontal dilution: {}".format(gnss.horizontal_dilution))
        if gnss.height_geoid is not None:
            print("Height geoid: {} meters".format(gnss.height_geoid))
