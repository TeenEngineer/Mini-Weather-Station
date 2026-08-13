A mini weather and temperature station displaying outdoors and indoors temperature, and barometric pressure, along with an estimated altitude above the sea level. All working on a Tenstar Robot TS-ESP32-S3

### Disclaimer: Used Gemini for code

Here is a video showcase of the thing:

https://github.com/user-attachments/assets/98cfebd6-e004-4a77-a924-2f23e415244a

# Instructions on how to use
There is a touch sensor, which when tapped on, switches between three menus: home screen, AP setup and Flashlight.

## Home screen
In this menu, you will see "OUT:..." and "IN:..." at the top. They display outdoors and indoors temperature(Indoors temperature may vary from the actual temp because the sensor is stupidly located near the MCU itself, which gets really hot and alters the readings. So there is an offset you can change in the code, the default is 26.5C). The outdoors temperature is only displayed if you put your Open Weather Map API key in the code.
(FYI the temp readings are from the onboard BMP280)
There is also a clock in the center. You have to set the timezone offsets for your specific region in the code(you can talk to your AI assistant if you don't know what to do)
There is a battery indicator on top of the clock, displays the battery charge percentage(may vary)
And there are barometric pressure readings at the bottom left, displayed in hectopascals(readings are from BMP280). There is an estimated altitude too, at the bottom right(based on pressure readings)

## AP setup
In this menu, you will see the connection status(Connected or Disconnected). If connected, you will see the SSID of the AP, the IP and Signal(formally RSSI) strength in dBm.
If you want to connect to your Wi-Fi AP, you touch and hold the touch sensor, until the RGB light turns blue, and the screen displays instructions(Connect to "WeatherStation-AP", open "192.168.4.1" in your browser, and set things up there. Used "WiFiManager" library by tzapu). If you wish to exit the AP setup mode, you can either tap on the touch sensor again, or exit from the web portal itself.

## Flashlight mode
Not much here. In this menu, touch and hold the touch sensor to turn on, and vice versa.

The station will automatically go into deep sleep after 10 minutes(can be changed in the code), and it can also manually be turned off(goes to deep sleep, technically) by touching and holding the sensor(IN HOME SCREEN!!!) until the RGB turns red and the device goes to deep sleep. Can be woken up by tapping on the sensor.


# Circuit diagram
<img width="750" height="492" alt="circuit diagram" src="https://github.com/user-attachments/assets/63e5edaf-abf1-4f25-bc62-36640391eb91" />

# Pinouts
| Touch sensor | ESP32-S3 |
| --- | --- |
| VCC | 3V |
| I/O | GPIO 5 |
| GND | GND |

Used a 1000mAh li-po battery with dimensions: 35x30x10mm, connected via PH2.0 compatible connector

# BOM
| Item | Link(aliexpress) |
| --- | --- |
| Touch sensor(TTP223) | https://ali.click/4cv6j1i |
| Tenstar Robot TS-ESP32-S3| https://ali.click/gcv6j17 |
| Battery 1000mAh | https://ali.click/ebv6j1w |
