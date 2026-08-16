## Getting Started
- In VSCode, ensure the PlatformIO extension is installed
- Open the project you wish to work with, e.g. `esp32_hellworld`
- Write code
- Use the platform io extension or pio command to build and upload to the board

More info:
https://docs.platformio.org/en/latest/core/quickstart.html


## TODO
GOAL: thermostat with touchscreen
- Touch proof of concept:
    - show temperature and pressure
    - show 'set temp'.  If you touch top of screen, set temp goes up by 1 degree.  If touch bottom, goes down 1 degree.


## Parts
https://www.amazon.com/your-orders/orders?page=1&ref_=ppx_yo2ov_dt_b_pagination_1_2

## ESP32
![](https://docs.platformio.org/en/latest/_images/espressif32_debug_pinout.jpg)

### BME680 Thermometer
https://learn.adafruit.com/adafruit-bme680-humidity-temperature-barometic-pressure-voc-gas/bsec-air-quality-library


## log

Installed pio
```powershell
The full path to `platformio.exe` is `C:\Users\New User\.platformio\penv\Scripts\platformio.exe`

If you need an access to `platformio.exe` from other applications, please install Shell Commands
(add PlatformIO Core binary directory `C:\Users\New User\.platformio\penv\Scripts` to the system environment PATH variable):

See https://docs.platformio.org/page/installation.html#install-shell-commands
```

