/**The MIT License (MIT)

SanC, Jan 2018  -  Internet enable weather meter. 
                   Old meter is connected to one ESP12F GPIO pin.
                   ESP12F serves a webpage to allow configuration: 
                   SSID, Password, CountryCode, CityCode and whether needle moves 
                   Weather information  is displayed on OLED Screen
                   Meter needles is moved to reflect values, if enabled. 
                   
                   See https://www.youtube.com/watch?v=n-S7tnWHkP0 

Extends Squix's Weatherstation example to 
                  1. Doesn't use ThingSpeak
                  2. Store settings in EEPROM so settings are retrieved and restored on power up
                  3. Uses displayed value to move a meter needle. 
                     Potentiometer placed between GPIO and meter to allow manual adjustment of scaling. 
                  4. Uses Squix's library to display a number of screens 
                     1 Summary Time and Date, temp shown in bottom left of footer
                     2 Current Weather conditions: Temp with weather icon
                     3 Feels like: indication of what current condition feel like
                     4 Wind Speed and Direction
                     5 Chance of Rain as a Percentage
                     6 Humidity
                     7 Three day forecast with highs and lows
                     8 Battery meter scale and Wifi Strength (percentage) indicator
                   5. On startup retrieves values from eeprom
                   6. If no values retrieved
                      6.1 ESP12F opens a SoftAP and serves web configuration page from 192.168.4.1 
                      6.2 If valid, store values in EEPROM 
                   7. If values retrieved 
                      7.1 try to connect to wifi, if unable to connect perform stage 6
                      7.2 If able to connect to wifi, get data from Weather Underground every 20 mins.                       
                   8. Wifi OTA firmware update available 
                      
Squix WeatherStation notice:
Copyright (c) 2016 by Daniel Eichhorn

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS ORx
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

See more at http://blog.squix.ch

*/

#include <EEPROM.h>                     // Parameter storage
#include <ESP8266WiFi.h>           
#include <Ticker.h>
#include <JsonListener.h>               // JSON parsing of WunderGround data
#include "SSD1306Wire.h"                // Example uses SPI connected OLED         
#include <SPI.h> 
#include "SSD1306Spi.h"
#include "OLEDDisplayUi.h"
#include "Wire.h"
#include "WundergroundClient.h"
#include "WeatherStationFonts.h"
#include "WeatherStationImages.h"
#include "TimeClient.h"
#include "indexpage.h"                  // HTML for indexpage

#include <ESP8266WebServer.h>           // ESP12 webserver...
#include <ESP8266HTTPUpdateServer.h>

extern "C" {
#include "user_interface.h"
}

String webSite,javaScript,JSONtxt;

/***************************
 * Begin Settings
 **************************/
// Please read http://blog.squix.org/weatherstation-getting-code-adapting-it
// for setup instructions

// EEPROM MAP 
size_t EPROM_MEMORY_SIZE= 512;

// EEPROM holds 4 items, includes NULL 
typedef struct  {
  char wifi_ssid[33];                  // Internet connected SSID details 
  char wifi_pwd[33];
  char cityCode[181];                  // Weather Underground codes...
  char countryCode[101]; 
  int  moveNeedle;                     // Does needle move or not (Quiet mode)     
} EEPROM_Storage;

EEPROM_Storage configuration;

ADC_MODE(ADC_VCC);                     //  Measures Battery meter scale from VCC

// For Wifi OTA update

const char* host = "ESP-Weather";

const char* update_path = "/firmware";
const char* update_username = "admin";
const char* update_password = "admin";
//ssid and password to connect to local hotspot of ESP8266
//const char *esp_ssid = "ESP-Weather";
//const char *esp_password = "#########";


ESP8266WebServer httpServer(80);          
ESP8266HTTPUpdateServer httpUpdater;      

// Setup
const int UPDATE_INTERVAL_SECS = 20 * 60; // Update every 20 minutes

// TimeClient settings
const float UTC_OFFSET = 0;

// Wunderground Settings
const boolean IS_METRIC = true;           
const String WUNDERGRROUND_API_KEY = "INSERT-YOUR-WUNDERGROUND_API_KEY";
const String WUNDERGRROUND_LANGUAGE = "EN";
const String WUNDERGROUND_COUNTRY = "UK";       // Example
const String WUNDERGROUND_CITY = "LONDON";      // Example

SSD1306Spi        display(16, 2, 15);           // Res, DC, CS pins on OLED display
OLEDDisplayUi   ui( &display ); 

/***************************
 * End Settings
 **************************/

// Set to false, if you prefere imperial/inches, Fahrenheit
WundergroundClient wunderground(IS_METRIC);     

TimeClient timeClient(UTC_OFFSET);


// flag changed in the ticker function every 10 minutes
bool readyForWeatherUpdate = false;

String lastUpdate = "--";

Ticker ticker;

//declaring prototypes
void drawProgress(OLEDDisplay *display, int percentage, String label,String cityCode, String countryCode); 
void updateData(OLEDDisplay *display);
void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawFeelsLike(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
// void drawThingspeak(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex);
void drawCurrentWindSpeed(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawHumidity(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) ; 
void drawForecastRainToday(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state);
void drawNetworkStatus(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void clearSettings(int16_t eepromSize); 
boolean connectToWiFi(EEPROM_Storage settings ); 

String encodeURL(String param); 

void setReadyForWeatherUpdate();


// Add frames
// this array keeps function pointers to all frames
// frames are the single views that slide from right to left

FrameCallback frames[] = { drawDateTime, drawCurrentWeather, drawFeelsLike,  drawCurrentWindSpeed,drawForecastRainToday, drawHumidity, drawForecast, drawNetworkStatus};
int numberOfFrames = 8;

// Note: SQUIX library required amended to correctly display indicator for 8 screens
OverlayCallback overlays[] = { drawHeaderOverlay };
int numberOfOverlays = 1;


void setup() {
  // clearSettings(EPROM_MEMORY_SIZE);  DEBUG USE ONLY             
  Serial.begin(74880);          
  Serial.println();
  Serial.println();
  httpServer.on("/", handleRoot);                                   // handler for http://192.168.4.1                                           
  httpServer.on("/reset",  [](){                                    // Wipe EEPROM if http://192.168.4.1/reset 
      clearSettings(EPROM_MEMORY_SIZE);
      httpServer.send(200, "text/plain", "Factory reset completed. Connect to Wifi via SSID: ESP-Weather");
  });
  httpServer.on("/inline", [](){
  httpServer.send(200, "text/plain", "this works as well");
  });
  httpServer.onNotFound(handleNotFound);                            // handler for an invalid URL
  httpUpdater.setup(&httpServer, update_path, update_username, update_password);   // Configure update HTTP page etc
  httpServer.begin();                                               // Start Webserver
  Serial.println("HTTP server started");                            
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local%s in your browser and login with username '%s' and password '%s'\n", host, update_path, update_username, update_password);

  // initialize display
  display.init();                                               
  display.clear();
  display.display();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setContrast(255);

  pinMode(5, OUTPUT);                                              // This pin drives the meter needle

  // Get saved configuration from EEPROM 
  EEPROM.begin(EPROM_MEMORY_SIZE);
  EEPROM.get(0,configuration); 
  EEPROM.end();
   
   // check lengths of stored values and try and connect to WIFI...
  boolean  readAllEEPROMValues = ( (strlen(configuration.wifi_ssid) > 0) && (strlen(configuration.wifi_pwd) > 0) 
                                 && (strlen(configuration.cityCode) > 0) && (strlen(configuration.countryCode) > 0)
                                 && connectToWiFi(configuration)
                                 ); 
  //diagnostics                               
  Serial.printf("Retrieved wifi_ssid    = \"%s\"\n",configuration.wifi_ssid);
  Serial.printf("Retrieved wifi_pwd     = \"%s\"\n",configuration.wifi_pwd);
  Serial.printf("Retrieved cityCode = \"%s\"\n",configuration.cityCode);
  Serial.printf("Retrieved CountryCode  = \"%s\"\n",configuration.countryCode);  
  Serial.printf("Retrieved moveNeedle   = %d\n",configuration.moveNeedle);  
   
             
  const char *esp_ssid     = "ESP-Weather";
  const char *esp_password = "#########"; 
  boolean turnOnSoftAP = true;                                   // Do we need to turn the softAP for configuration setting
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setContrast(255);

                                                                 // Either saved settings failed or 1st time through
  while ( readAllEEPROMValues == false ) {                       // No saved settings so turn on SOFTAP
       if (turnOnSoftAP){                                        
           WiFi.hostname("ESP-Weather");
           WiFi.mode(WIFI_AP);
           WiFi.softAP(esp_ssid, esp_password);
           Serial.print("Soft-AP IP address = ");
           Serial.println(WiFi.softAPIP());
           turnOnSoftAP = false; 
       }
       display.setTextAlignment(TEXT_ALIGN_CENTER);              // Dsplay advice on OLED...
       display.clear(); 
       display.drawString(20,5,"Configuration");
       display.drawString(20,20,"Please connect to ");
       display.drawString(20,30,esp_ssid);
       display.drawString(20,40, String(WiFi.softAPIP()[0]) + "." + String(WiFi.softAPIP()[1]) + "." + String(WiFi.softAPIP()[2]) + "." + String(WiFi.softAPIP()[3]));

       httpServer.handleClient();                              // Accept   configuration via Webpage
                                                               // display status of 4 configuration settings, one symbol per characteristic
       display.drawXbm(43, 55, 8, 8, strlen(configuration.wifi_ssid) > 0     ? activeSymbole : inactiveSymbole);
       display.drawXbm(55, 55, 8, 8, strlen(configuration.wifi_pwd)  > 0     ? activeSymbole : inactiveSymbole);
       display.drawXbm(66, 55, 8, 8, strlen(configuration.cityCode) > 0      ? activeSymbole : inactiveSymbole);
       display.drawXbm(77, 55, 8, 8, strlen(configuration.countryCode) > 0   ? activeSymbole : inactiveSymbole);
       display.display();                                       // Update display every 0.5 secs
       delay(500);

       // have all values, so store to EEPROM
       if ( ( strlen(configuration.wifi_ssid) > 0) && (strlen(configuration.wifi_pwd)  > 0)
           && (strlen(configuration.cityCode) > 0) && ( strlen(configuration.countryCode) > 0) 
           ){
           readAllEEPROMValues = connectToWiFi(configuration) ;    // try and connect when all settings received
           }
  }

  if (WiFi.status() == WL_CONNECTED)                          // Able to connect to Wifi
  {                                                 
   WiFi.softAPdisconnect();                                   // turn off SoftAP
    
  }
  delay(100);           

  ui.setTargetFPS(30);

  // added by San
  ui.setTimePerFrame(10000);                                  // Display each frame for 10secs
  ui.setTimePerTransition(500);                               // 0.5 secs transition
  
  ui.setActiveSymbol(activeSymbole);
  ui.setInactiveSymbol(inactiveSymbole);

  // You can change this to
  // TOP, LEFT, BOTTOM, RIGHT
  ui.setIndicatorPosition(BOTTOM);

  // Defines where the first frame is located in the bar.
  ui.setIndicatorDirection(LEFT_RIGHT);

  //ui.disableAllIndicators();

  // You can change the transition that is used
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_TOP, SLIDE_DOWN
  ui.setFrameAnimation(SLIDE_LEFT);

  ui.setFrames(frames, numberOfFrames);
  ui.setOverlays(overlays, numberOfOverlays);

  // Inital UI takes care of initalising the display too.
  ui.init();

  updateData(&display);                                     // Get data

  ticker.attach(UPDATE_INTERVAL_SECS, setReadyForWeatherUpdate); // Set update boolean flag  every UPDATE_INTERVAL_SECS 

}

void loop()  {
  httpServer.handleClient();                                 

  if (readyForWeatherUpdate && ui.getUiState()->frameState == FIXED) {  // Frames not moving and time for a data update 
    updateData(&display);
  }

  int remainingTimeBudget = ui.update();

  if (remainingTimeBudget > 0) {
    // You can do some work here
    // Don't do stuff if you are below your
    // time budget.
    delay(remainingTimeBudget);
  }

}
// Utility function - displays progress as data retrieved and configured 
void drawProgress(OLEDDisplay *display, int percentage, String label,String cityCode, String countryCode) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 10, label);
  display->drawProgressBar(2, 28, 124, 10, percentage);
  display->drawString(64, 42, cityCode+","+countryCode);
    
  display->display();
}

String encodeURL(String param) {
param.replace(" ","%20");
param.replace("!","%21");
param.replace("#","%23");
param.replace("$","%24");
param.replace("&","%26");
param.replace("'","%27");
param.replace("(","%28");
param.replace(")","%29");
param.replace("*","%2A");
param.replace(",","%2C");
param.replace("/","%2F");
param.replace(":","%3A");
param.replace(";","%3B");
param.replace("=","%3D");
param.replace("?","%3F");
param.replace("@","%40");
param.replace("[","%5B");
param.replace("]","%5D");
return param;
}
// Called when data is updated, every 20 mins
void updateData(OLEDDisplay *display) {                         
  String encodedCityCode; 
  String encodedCountryCode; 
  encodedCityCode = encodeURL( configuration.cityCode);               // Format paramaters
  encodedCountryCode = encodeURL( configuration.countryCode);
  drawProgress(display, 10, "Updating time...",&configuration.cityCode[0],&configuration.countryCode[0]);
  timeClient.updateTime();
  // Retrieve data from Wunderground
  wunderground.updateConditions(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE,  encodedCountryCode, encodedCityCode);  
  // Advise on display
  drawProgress(display, 50, "Updating forecasts...",&configuration.cityCode[0],&configuration.countryCode[0] );
  // Retrieve data from Wunderground
  wunderground.updateForecast(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE,  encodedCountryCode, encodedCityCode);
  lastUpdate = timeClient.getFormattedTime();
  readyForWeatherUpdate = false;                    // All data retrieved from Wunderground...
  drawProgress(display, 100, "Done...",&configuration.cityCode[0],&configuration.countryCode[0]);
  delay(1000);
}

// Functions for display of each frame...
// Callback function to display Date and time Frame 
void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  analogWrite(5,0);     
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  String date = wunderground.getDate();
  int textWidth = display->getStringWidth(date);
  display->drawString(64 + x, 5 + y, date);
  
  display->setFont(ArialMT_Plain_24);
  String time = timeClient.getFormattedTime();
  
  textWidth = display->getStringWidth(time);
  display->drawString(64 + x, 15 + y, time);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64 + x, 39 + y, String(configuration.cityCode));
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}
// Callback function to display CurrentWeather Frame with weather and and Icon..
void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  analogWrite(5,0);                      // no need to move needle
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(60 + x, 5 + y, wunderground.getWeatherText());

  display->setFont(ArialMT_Plain_24);
  String temp = wunderground.getCurrentTemp() + "°C";
  
  display->drawString(60 + x, 15 + y, temp);
  int tempWidth = display->getStringWidth(temp);

  display->setFont(Meteocons_Plain_42);
  String weatherIcon = wunderground.getTodayIcon();
  int weatherIconWidth = display->getStringWidth(weatherIcon);
  display->drawString(32 + x - weatherIconWidth / 2, 05 + y, weatherIcon);
  if (configuration.moveNeedle == 1) {analogWrite(5,temp.toInt()); }              // scale temp to meter scale if required.
 }

// Callback function to display FeelsLike Frame 
void drawFeelsLike(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(60 + x, 5 + y, "Feels like");

  display->setFont(ArialMT_Plain_24);
  String temp = wunderground.getFeelsLike() + "°C";
  
  display->drawString(60 + x, 15 + y, temp);
  int tempWidth = display->getStringWidth(temp);

  display->setFont(Meteocons_Plain_42);
  String weatherIcon = wunderground.getTodayIcon();
  int weatherIconWidth = display->getStringWidth(weatherIcon);
  display->drawString(32 + x - weatherIconWidth / 2, 05 + y, weatherIcon);
  if (configuration.moveNeedle == 1) {analogWrite(5,temp.toInt()); }             // scale temp to meter scale if required.
}



// Callback function to display Current WindSpeed Frame 
void drawCurrentWindSpeed(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(60 + x, 5 + y, "Wind: "+wunderground.getWindDir());

  display->setFont(ArialMT_Plain_24);
  String windSpeed = wunderground.getWindSpeed() ;
  
  display->drawString(60 + x, 15 + y, windSpeed);
  int windSpeedWidth = display->getStringWidth(windSpeed);

  display->setFont(Meteocons_Plain_42);
  String weatherIcon = wunderground.getTodayIcon();
  int weatherIconWidth = display->getStringWidth(weatherIcon);
  display->drawString(32 + x - weatherIconWidth / 2, 05 + y, weatherIcon);
  if (configuration.moveNeedle == 1) {analogWrite(5,windSpeed.toInt()); }        // scale Windspeed to meter scale if required
}
// Callback function to display PercentageChanceOfRain  Frame 
void drawForecastRainToday(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(60 + x, 5 + y, "Chance of rain");

  display->setFont(ArialMT_Plain_24);
  String forecastRainToday = wunderground.getPoP(0)+"%";
  
  display->drawString(60 + x, 15 + y, forecastRainToday);
  int forecastRainWidth = display->getStringWidth(forecastRainToday);

  display->setFont(Meteocons_Plain_42);
  String weatherIcon = wunderground.getTodayIcon();
  int weatherIconWidth = display->getStringWidth(weatherIcon);
  display->drawString(32 + x - weatherIconWidth / 2, 05 + y, weatherIcon);
  if (configuration.moveNeedle == 1) {analogWrite(5,forecastRainToday.toInt());}    // scale Forecast Rain %age to meter scale if required
}

// Callback function to display Humidity  Frame 
void drawHumidity(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(60 + x, 5 + y, "Humidity");

  display->setFont(ArialMT_Plain_24);
  String humidity = wunderground.getHumidity() ;
  
  display->drawString(60 + x, 15 + y, humidity);
  int humidityWidth = display->getStringWidth(humidity);

  display->setFont(Meteocons_Plain_42);
  String weatherIcon = wunderground.getTodayIcon();
  int weatherIconWidth = display->getStringWidth(weatherIcon);
  display->drawString(32 + x - weatherIconWidth / 2, 05 + y, weatherIcon);
  if (configuration.moveNeedle == 1) {analogWrite(5,humidity.toInt()); }           // scale %humidity to meter scale if required 
}

// Callback function to display 3dayForecast  Frame 
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  drawForecastDetails(display, x, y, 0);
  drawForecastDetails(display, x + 44, y, 2);
  drawForecastDetails(display, x + 88, y, 4);
}

// Support  function to display 3dayForecast  Frame 
void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex) {
  analogWrite(5,LOW);                 
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  String day = wunderground.getForecastTitle(dayIndex).substring(0, 3);
  day.toUpperCase();
  display->drawString(x + 20, y, day);

  display->setFont(Meteocons_Plain_21);
  display->drawString(x + 20, y + 12, wunderground.getForecastIcon(dayIndex));

  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y + 34, wunderground.getForecastLowTemp(dayIndex) + "|" + wunderground.getForecastHighTemp(dayIndex));
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

// Top Line 
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  display->setColor(WHITE);
  display->setFont(ArialMT_Plain_10);
  String time = timeClient.getFormattedTime().substring(0, 5);

  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0, 54, time);

  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  String temp = wunderground.getCurrentTemp() + "°C";
  display->drawString(128, 54, temp);
  display->drawHorizontalLine(0, 52, 128);
}

// Callback function to display Battery Status and Wifi Strength
void drawNetworkStatus(OLEDDisplay *display, OLEDDisplayUiState* state ,int16_t x, int16_t y ) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_16);
  char ipaddr[16]; 
  sprintf(ipaddr,"IP:%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  int textWidth = display->getStringWidth(ipaddr);
  display->drawString(64 + x, 8 + y, ipaddr);
    
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  float vdd = ESP.getVcc()/1000.0;                                         // Read voltage
  display->drawString(10+x, 30 + y, String(vdd)+"V");                      // display supply VCC  on OLED 
  
  // calculate and display WIFI quality

  int32_t dbm = WiFi.RSSI();
  if (configuration.moveNeedle == 1) 
    {
    analogWrite(5,dbm);                                                    // scale WIFI %age strength to WiFi Strength
    }
  if(dbm <= -100) {
      dbm=0; 
  } else if(dbm >= -50) {
      dbm=100;
  } else {
      dbm=  2 * (dbm + 100);
  }
  for (int8_t i = 0; i < 4; i++) {                                         // draw wifi strength graphic
    for (int8_t j = 0; j < 3 * (i + 1); j++) {
      if (dbm > i * 25 || j == 0) {
        display->setPixel(64 + 2 * i, 45 - j);
      }
    }
  }
  display->setTextAlignment(TEXT_ALIGN_LEFT);  
  display->drawString(75+x, 30 + y, String(dbm)+"%");
}

// is it time for an update from WunderGround
void setReadyForWeatherUpdate() {
  Serial.println("Setting readyForUpdate to true");
  readyForWeatherUpdate = true;
}

// Handler for processing data sent back from webpage: SSID, PassWord, CityCode, CountryCode and moveNeedle
void handleArgs(){
  for (int i = 0; i < httpServer.args(); i++) {
        Serial.print(String(i) + " ");  //print id
        Serial.print("\"" + String(httpServer.argName(i)) + "\"");  //print name
        Serial.print("\"" + String(httpServer.arg(i)) + "\"");  //print value
        Serial.println("\"" + String(String(httpServer.arg(i)).length())  + "\"");  //print length
  }  
  // copy to config structure, need to take care of overwriting shorter strings etc
  boolean wifiChanged = false; 
  if (strcmp(httpServer.arg(0).c_str(),configuration.wifi_ssid ) != 0) {
    Serial.print("WIFI CHANGED " );  //print name
    *(configuration.wifi_ssid) = '\0';
    strncat(configuration.wifi_ssid, httpServer.arg(0).c_str(),strlen(httpServer.arg(0).c_str()));
    wifiChanged = true;
  }
  if (strcmp(httpServer.arg(1).c_str(),configuration.wifi_pwd ) != 0) {
    Serial.print("WIFI PWD  CHANGED " );  //print name
    *(configuration.wifi_pwd) = '\0';
    strncat(configuration.wifi_pwd,     httpServer.arg(1).c_str(),strlen(httpServer.arg(1).c_str()));
    wifiChanged = true;
  }
  *(configuration.cityCode) = '\0';
  strncat(configuration.cityCode,     httpServer.arg(2).c_str(),strlen(httpServer.arg(2).c_str()));

  *(configuration.countryCode) = '\0';
  strncat(configuration.countryCode,  httpServer.arg(3).c_str(),strlen(httpServer.arg(3).c_str()));

// toggle only sent if set to on ie 5 arguments passed.
  configuration.moveNeedle =((httpServer.args() == 5) && (String(httpServer.arg(4)) == "on")) ? 1 : 0; 

  Serial.printf("\nReceived\nssid:\t\"%s\"%d\npwd:\t\"%s\"%d\ncityCode:\t\"%s\"%d\ncountryCode:\t\"%s\"%d\nmoveNeedle:\t\%d\n",configuration.wifi_ssid,strlen(configuration.wifi_ssid), 
                                                                                                          configuration.wifi_pwd, strlen(configuration.wifi_pwd),
                                                                                                          configuration.cityCode,strlen(configuration.cityCode),
                                                                                                          configuration.countryCode, strlen(configuration.countryCode),
                                                                                                          configuration.moveNeedle);
  if (wifiChanged == true)            //Do new credentials work?
      {  
      connectToWiFi(configuration); 
      }

  readyForWeatherUpdate = true;                       // force an an update from wunderground
      
}
// handler for processing indexpage
void handleRoot() {
  if(httpServer.args() > 0){
      handleArgs(); 
  }else{
    httpServer.send(200,"text/html",MAIN_page);
  }
}
// Page Not found handler 
void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += httpServer.uri();
  message += "\nMethod: ";
  message += (httpServer.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += httpServer.args();
  message += "\n";
  for (uint8_t i=0; i<httpServer.args(); i++){
    message += " " + httpServer.argName(i) + ": " + httpServer.arg(i) + "\n";
  }
  httpServer.send(404, "text/plain", message);
}

// Clear EEPROM (unit reset) 
void clearSettings(int16_t eepromSize){
  EEPROM.begin(eepromSize);
 // write a 0 to all 512 bytes of the EEPROM
 for (int i = 0; i < 512; i++)
   {
    EEPROM.write(i, 0);
   }
  EEPROM.commit();
  EEPROM.end();
  configuration.wifi_ssid[0]  = 0;
  configuration.wifi_pwd[0]   = 0;
  configuration.cityCode[0]   = 0;
  configuration.countryCode[0]= 0;
  configuration.moveNeedle    = 0; 

 
}
// try and connect to WiFi, show progress every 500 milliseconds 
boolean connectToWiFi( EEPROM_Storage settings){
  display.setFont(ArialMT_Plain_10);
   
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  
  
  WiFi.begin(settings.wifi_ssid,settings.wifi_pwd);

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED and counter < 40) {   // Show progress, try for 20 secs (40 * 0.5 secs)                 
    delay(500);
    Serial.print(".");
    display.clear();
    display.drawString(64, 10, "Connecting to WiFi");
    display.drawXbm(46, 30, 8, 8, counter % 3 == 0 ? activeSymbole : inactiveSymbole);
    display.drawXbm(60, 30, 8, 8, counter % 3 == 1 ? activeSymbole : inactiveSymbole);
    display.drawXbm(74, 30, 8, 8, counter % 3 == 2 ? activeSymbole : inactiveSymbole);
    display.display();
    counter++;
  }

  if (WiFi.status() == WL_CONNECTED){                        //Settings work so store to EEPROM
      EEPROM.begin(EPROM_MEMORY_SIZE);
      EEPROM.put(0,configuration); 
      EEPROM.end();    
      Serial.printf("WORKS: \nssid:\t\"%s\"%d\npwd:\t\"%s\"%d\ncityCode:\t\"%s\"%d\ncountryCode:\t\"%s\"%d\nmoveNeedle:\t\%d\n",configuration.wifi_ssid,strlen(configuration.wifi_ssid), 
                                                                                                          configuration.wifi_pwd, strlen(configuration.wifi_pwd),
                                                                                                          configuration.cityCode,strlen(configuration.cityCode),
                                                                                                          configuration.countryCode, strlen(configuration.countryCode),
                                                                                                         configuration.moveNeedle);
     WiFi.mode(WIFI_STA);                                    // Set Mode
     return true;
  }
  if ( counter == 40 && (WiFi.status() != WL_CONNECTED)) {   // Didn't connect to wifi, assume bad credentials, wipe eeprom and start over. 
     Serial.println("WIFI CONNECT timed out");
   }  
 return false;                                               // catches fail to connectToWifi, go back and get user to resubmit. 
}



