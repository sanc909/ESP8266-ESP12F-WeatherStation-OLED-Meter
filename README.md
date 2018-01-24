# ESP8266-ESP12F-WeatherStation-OLED-Meter
IOT enabled analogue Meter shows weather details on OLED and moves meter needle for emphasis. 


Click the image to play a video on Youtube. 

[![WeatherStation](http://img.youtube.com/vi/n-S7tnWHkP0/0.jpg)](http://www.youtube.com/watch?v=n-S7tnWHkP0 "Click to play on Youtube")

Analogue meter connects to GPIO5 on ESP12F, creating an Internet enabled weather meter. </br>
* Old meter is connected to one ESP12F GPIO pin.</br>
* ESP12F serves a webpage to allow configuration: </br>
* SSID, Password, CountryCode, CityCode and whether needle moves </br>
* Weather information  is displayed on OLED Screen</br>
* Meter needle moves to reflect values, if enabled. </br>

See https://www.youtube.com/watch?v=n-S7tnWHkP0 </br>

Extends Squix's Weatherstation example:
1. Doesn't use ThingSpeak
2. Store settings in EEPROM so settings are retrieved and restored on power up
3. Uses displayed value to move a meter needle. </br>
Potentiometer placed between GPIO and meter to allow manual adjustment of scaling. </br>
4. Uses Squix's library to display a number of screens </br>
4.1 <b>Summary Time and Date</b> with temp shown in bottom left of footer</br>
4.2 <b>Current Weather></b> conditions: Temp with weather icon</br>
4.3 <b>Feels like:</b> indication of what current condition feel like</br>
4.4 <b>Wind Speed and Direction</b></br>
4.5 <b>Chance of Rain</b> as a Percentage</br>
4.6 <b>Humidity</b></br>
4.7 <b>Three day forecast</b> with highs and lows</br>
4.8 <b>Battery meter scale and Wifi Strength</b> (percentage) indicator</br>
5. On startup retrieves values from eeprom</br>
6. If no values retrieved</br>
6.1 ESP12F opens a SoftAP and serves web configuration page from 192.168.4.1 </br>
6.2 If valid, store values in EEPROM </br>
7. If values retrieved </br>
7.1 try to connect to wifi, if unable to connect perform stage 6</br>
7.2 If able to connect to wifi, get data from Weather Underground every 20 mins. </br>
8. Wifi OTA firmware update available</br> 


Developed in Arduino IDE. 
