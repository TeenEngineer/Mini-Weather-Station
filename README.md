A mini weather and temperature station displaying outdoors and indoors temperature, and barometric pressure, along with an estimated altitude above the sea level. All working on a Tenstar Robot TS-[...]

### Disclaimer: Used Gemini for code

Here is a video showcase of the thing:

https://github.com/user-attachments/assets/98cfebd6-e004-4a77-a924-2f23e415244a

# Instructions on how to use
There is a touch sensor, which when tapped on, switches between three menus: home screen, AP setup and Flashlight.

## Home screen
In this menu, you will see "OUT:..." and "IN:..." at the top. They display outdoors and indoors temperature(Indoors temperature may vary from the actual temp because the sensor is stupidly located[...]
There is also a clock in the center. You have to set the timezone offsets for your specific region in the code(you can talk to your AI assistant if you don't know what to do)
There is a battery indicator on top of the clock, displays the battery charge percentage(may vary)
And there are barometric pressure readings at the bottom left, displayed in hectopascals. There is an estimated altitude too, at the bottom right(based on pressure readings)

## AP setup
In this menu, you will see the connection status(Connected or Disconnected). If connected, you will see the SSID of the AP, the IP and Signal(formally RSSI) strength in dBm.
If you want to connect to your Wi-Fi AP, you touch and hold the touch sensor, until the RGB light turns blue, and the screen displays instructions(Connect to "WeatherStation-AP", open "192.168.4.1[...]

## Flashlight mode
Not much here. In this menu, touch and hold the touch sensor to turn on, and vice versa.

The station will automatically go into deep sleep after 10 minutes(can be changed in the code), and it can also manually be turned off(goes to deep sleep, technically) by touching and holding the [...]

## Pinouts
| Touch sensor | ESP32-S3 |
| --- | --- |
| VCC | 3V |
| I/O | GPIO 5 |
| GND | GND |
