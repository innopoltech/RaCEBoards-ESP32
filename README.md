# RaCEBoards-ESP32
RaCEboards - The Electronics for Your Project

**RaCEboards** is the *electronic brain* of your device, whether it's a rover, robotic arm, rocket, or airplane model. It's a feature-rich board with multiple sensors that monitors the device's status and runs the algorithm you programmed. A universal controller for hands-on learning of electronics, electrical engineering, and programming within project-based activities.

**RaCEboards** consists of 4 core boards stacked like a "sandwich." You can easily rearrange the boards or exclude one or more from the stack—it will remain fully functional!

The current version of **RaCEboards** is **v0.4**.

**RaCEboards v0.4 Stack:**
+   **MCU v0.4** - Microcontroller Board
+   **PWR v0.4** - Power Management & DC Motor/Servo Driver Board
+   **MEMS v0.4** - Sensors and Expansion Board
+   **GNSS v0.4** - Navigation and Wireless Communication Board
+   **CPM v0.4** - BLDC Power Distribution Board *(test version)*
+   **AUX v0.4** - Audio Board *(test version)*
+   **TP v0.4** - Breadboard *(test version)*

## Technical Specifications
*   **MCU:** ESP32-S3
*   **Programming:** [CircuitPython](https://github.com/adafruit/circuitpython), [Arduino](https://github.com/espressif/arduino-esp32), [ESP-IDF](https://github.com/espressif/esp-idf), [MicroBlocks](https://microblocks.fun/)
*   **Barometer:** BMP280
*   **Accelerometer & Gyroscope:** LSM6DSL
*   **Magnetometer:** QMC5883L
*   **GNSS:** L86-M33
*   **Wireless Transceiver:** Ra-01S (LoRa)
*   **Storage:** Supports microSD cards up to 16GB
*   **Available Interfaces:** I2C, SPI, UART, I2S, GPIO

**Physical Specs:**
*   Weight: 80 g
*   Dimensions: 49 x 49 x 40 mm
*   Power: Compatible with 1S Li-Po/Li-Ion batteries
*   Charging: Supported via USB Type-C

## Code Examples
Usage examples for various **RaCEboards** peripherals can be found in:
*   `examples/v0.x/CircuitPython/` for CircuitPython
*   `examples/v0.x/ESP_IDF/` for ESP-IDF
*   `examples/v0.x/MicroBlock/` for MicroBlocks
*   *Arduino - In development*

## Dedicated Libraries for RaCEboards
Hardware-specific libraries are located in:
*   `lib/v0.x/CircuitPython/` for CircuitPython
*   `lib/v0.x/ESP_IDF/` for ESP-IDF
*   `lib/v0.x/MicroBlock/` for MicroBlocks
*   *Arduino - In development*

## Tutorials & Documentation
For tutorials specifically designed for **RaCEboards**, please visit our wiki:
[RaCEboard Wiki](https://raceboards.notion.site/b904a8853876489b9a182e0184369974?v=f337b6ecd4494e199e3b007a1e0e9fd5)
