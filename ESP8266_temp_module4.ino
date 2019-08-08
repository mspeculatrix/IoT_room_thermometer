// ESP8266-based IoT room thermometer - HTTP version
// For more information, go to: https://mansfield-devine.com/speculatrix/category/projects/iot-thermometer/

#ifdef __AVR__
  #include <avr/power.h>
#endif

// *** SET FOR EACH DEVICE ***
// I designed a PCB for this project. There are two versions, rev 1.1 having
// an extra reset switch.
#define PCB_REVISION 1.1
// This string is used to identify each thermometer when communicating
// with the REST server.
#define SENSOR_NAME "DHT22_3"

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include "SPI.h"
#include "TFT_eSPI.h"

// You need a header file that is in the same directory, or within the compiler's
// search path, containing the credentials for your wifi network. As I have
// two APs, I've set up primary and secondary creds (using the same password for
// both). The file should contain:
// #define HOME_WIFI_AP_MAIN "YourMainSSID"
// #define HOME_WIFI_AP_ALT "Your2ndSSID"
// #define HOME_WIFI_PW "YourWifiPassword"
#include <HomeWifi.h>

#include "Adafruit_Sensor.h"      // both of these libraries are from Adafruit
#include "DHT.h"

#define LOOP_INTERVAL 600         // milliseconds between each cycle through main loop
#define MSG_PERSIST_CYCLES 100    // for how many cycles through the loop should messages persist?
#define ERR_PERSIST_CYCLES 100    // same for error messages
#define SENSOR_INTERVAL 100       // how many cycles between sensor readings & temp updating
#define REPORT_INTERVAL 500       // how often data is sent to server
#define TIME_INTERVAL 20          // how often is time display updated
#define DATE_INTERVAL 1000        // how often is date display updated
#define WEATHER_INTERVAL 500
#define SERVER_ERR_LIMIT 5        // how many server errors in a row before we reset wifi

// --- SENSOR INFO ------------------------------------------------------------------------
typedef enum {DATA_FMT_SCREEN, DATA_FMT_REPORT} DataFormat;

// --- TFT DISPLAY ------------------------------------------------------------------------
#define GFXFF 1
#define CF_LL_48 &Lato_Light_48
#define CF_LL_72 &Lato_Light_72
#define CF_LL_96 &Lato_Light_96
#define CF_URWG_12 &URW_Gothic_L_Book_12
#define CF_URWG_16 &URW_Gothic_L_Book_16
#define CF_URWG_24 &URW_Gothic_L_Book_24
#define CF_URWG_36 &URW_Gothic_L_Book_36
#define CF_URWG_48 &URW_Gothic_L_Book_48
#define CF_URWG_72 &URW_Gothic_L_Book_72
#define CF_URWG_96 &URW_Gothic_L_Book_96

#define SENSOR_DATA_YPOS  95      // screen positions, in pixels
#define TIME_YPOS         125
#define DATE_YPOS         200
#define ERR_YPOS          280
#define MSG_YPOS          280
#define X_CENTRE          118
#define WEATHER_YPOS      240
#define MSG_HEIGHT         14

// APP COLOURS
// Used this to define colours: http://www.barth-dev.de/online/rgb565-color-picker/
// see here for more info: http://henrysbench.capnfatz.com/henrys-bench/arduino-adafruit-gfx-library-user-guide/arduino-16-bit-tft-rgb565-color-basics-and-selection/
#define BG_COLOUR 0x0004
#define FG_DATA_TEXT 0xFFE0
#define FG_INFO_TEXT 0xCF18


// --- NETWORK ----------------------------------------------------------------------------
#define WIFI_MAX_TRIES 12
const char* ssid [] = { HOME_WIFI_AP_MAIN, HOME_WIFI_AP_ALT };
int wifi_status = WL_IDLE_STATUS;
String network = "--";
IPAddress ip;

// --- HTTP CLIENT ------------------------------------------------------------------------
// This is my intranet server, which is running a REST API. You'll need one of these.
#define HTTP_SERVER "http://10.0.0.159/iotServer.php"
typedef enum {TIME_INFO, DATE_INFO} DatetimeInfo;
HTTPClient http;

// --- TEMP SENSOR -----------------------------------------------------------------------
#define DHTPIN 2        // pin used for temp sensor
#define REFRESH_PIN 16  // pin used for resetting the min/max values and maybe other things.
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
float humidity = 50.0;              // arbitrary initial settings. To be overwritten
float temp_float = 20.0;            // once we get going.
int8_t temp_int = int(temp_float);
int8_t temp_max = -99;
int8_t temp_min = 99;

// --- INFO ------------------------------------------------------------------------------
// These strings will be set by info coming from the REST API server.
String timeStr = "--:--";
String dateStr = "-";
String weatherStr = "";

// ---------------------------------------------------------------------------------------
// --- GLOBALS                                                                         ---
// ---------------------------------------------------------------------------------------
TFT_eSPI tft = TFT_eSPI();
uint8_t server_errors = 0;
int msg_counter, err_counter = 0;
int sensor_counter = SENSOR_INTERVAL; // to force an immediate read
int time_counter = TIME_INTERVAL;
int date_counter = DATE_INTERVAL;
int weather_counter = WEATHER_INTERVAL;
int report_counter = 0;

// ---------------------------------------------------------------------------------------
// --- DISPLAY FUNCTIONS                                                               ---
// ---------------------------------------------------------------------------------------
String dataString(DataFormat format) {
  char temp_buf[5];
  char hum_buf[5];
  char minmax_buf[6];
  sprintf(minmax_buf, "%i/%i", temp_min, temp_max);
  dtostrf(temp_float, 4, 1, temp_buf);
  dtostrf(humidity, 4, 1, hum_buf);
  String sensor_vals = "";
  if (format == DATA_FMT_SCREEN) {
    sensor_vals = String(temp_buf) + "  " + String(minmax_buf) + "  " + String(hum_buf) + "%";
  } else if(format == DATA_FMT_REPORT) {
    sensor_vals = String(temp_buf) + "_" + String(hum_buf) + "_" + String(minmax_buf);
  }
  return sensor_vals;
}

void clearError() {
  tft.fillRect(0, ERR_YPOS, tft.width(), MSG_HEIGHT, BG_COLOUR);
}

void clearMsg() {
  tft.fillRect(0, MSG_YPOS, tft.width(), MSG_HEIGHT, BG_COLOUR);
}

void printDate() {
  tft.setTextColor(TFT_BLUE, BG_COLOUR);
  tft.setTextDatum(TC_DATUM);
  tft.setFreeFont(CF_URWG_36);
  tft.setTextPadding(240);
  tft.drawString(dateStr, X_CENTRE, DATE_YPOS, GFXFF);
}

void printTime() {
  tft.setTextColor(TFT_BLUE, BG_COLOUR);
  tft.setTextDatum(TC_DATUM);
  tft.setFreeFont(CF_URWG_72); 
  tft.setTextPadding(240);
  tft.drawString(timeStr, 118, TIME_YPOS, GFXFF);
}

void printError(String errMsg) {
  clearError();
  tft.setTextColor(TFT_RED, BG_COLOUR);
  tft.setTextDatum(TL_DATUM);
  tft.setFreeFont(CF_URWG_12); 
  tft.drawString(errMsg, 0, ERR_YPOS, GFXFF);
  err_counter = 1;    // start counting
}

void printMsg(String msg) {
  clearMsg();
  tft.setTextColor(TFT_GREEN, BG_COLOUR);
  tft.setTextDatum(TL_DATUM);
  tft.setFreeFont(CF_URWG_12); 
  tft.drawString(msg, 0, MSG_YPOS, GFXFF);
  msg_counter = 1;    // start counting
}

void printDegC() {
  tft.setTextColor(FG_INFO_TEXT, BG_COLOUR);
  tft.setTextDatum(TL_DATUM);
  tft.setFreeFont(CF_LL_48); 
  tft.drawString("o", 128, 0, GFXFF);
  tft.setFreeFont(CF_LL_96);
  tft.drawString("C", 153, 5, GFXFF);  
}

void printTemperature() {
  tft.setTextColor(FG_DATA_TEXT, BG_COLOUR);
  tft.setTextDatum(TR_DATUM);
  tft.setFreeFont(CF_URWG_96);
  tft.drawString(String(temp_int), 123, 5, GFXFF);
  // print sensor info
  tft.setFreeFont(CF_URWG_24);
  tft.setTextDatum(TC_DATUM);
  //tft.fillRect(0, SENSOR_DATA_YPOS, tft.width(), 24, BG_COLOUR);
  tft.setTextPadding(240); // was 240
  tft.drawString(dataString(DATA_FMT_SCREEN), 120, SENSOR_DATA_YPOS, GFXFF);
}

void printWeather() {
  tft.setTextColor(TFT_BLUE, BG_COLOUR);
  tft.setTextDatum(TC_DATUM);
  tft.setFreeFont(CF_URWG_24); 
  tft.drawString(weatherStr, X_CENTRE, WEATHER_YPOS, GFXFF);
  tft.setTextPadding(240);
}

void printNetworkInfo() {
  tft.setFreeFont(CF_URWG_16);
  tft.setTextColor(FG_INFO_TEXT, BG_COLOUR);
  tft.setTextDatum(BL_DATUM);
  tft.drawString(network + "           ", 0, 320, GFXFF);
  tft.setTextDatum(BR_DATUM);
  tft.drawString(" " + ip.toString(), 240, 320, GFXFF);
}

void refreshScreen() {
  tft.fillScreen(BG_COLOUR);
  printDegC();
  printNetworkInfo();
}

// ---------------------------------------------------------------------------------------
// --- SENSOR FUNCTIONS                                                                ---
// ---------------------------------------------------------------------------------------
void getReadings() {
    humidity = dht.readHumidity();
    temp_float = dht.readTemperature() - 0.5;
    temp_int = round(temp_float);
    if(temp_int > temp_max) temp_max = temp_int;
    if(temp_int < temp_min) temp_min = temp_int;
}

// ---------------------------------------------------------------------------------------
// --- WIFI FUNCTIONS                                                                  ---
// ---------------------------------------------------------------------------------------
void wifiConnect() {
  uint8_t ssid_idx = 0;
  uint8_t connect_counter = 0;
  network = "--";                                 // reset
  tft.setTextColor(TFT_CYAN);
  tft.fillScreen(BG_COLOUR);
  tft.setCursor(0,20);
  tft.setFreeFont(CF_URWG_16);
  tft.println("Montcocher IoT");
  tft.println("Connecting to wifi...");
  while (connect_counter < WIFI_MAX_TRIES) {
    WiFi.begin(ssid[ssid_idx], HOME_WIFI_PW);     // try to connect
    tft.print("- trying: "); tft.println(ssid[ssid_idx]);
    // delay to allow time for connection
    delay(5000);
    wifi_status = WiFi.status();
    connect_counter++;
    if (wifi_status != WL_CONNECTED) {
      ssid_idx = 1 - ssid_idx;    // swap APs
    } else {
      tft.println("Connected!");
      tft.println(WiFi.localIP());
      connect_counter = WIFI_MAX_TRIES;
      network = (String)ssid[ssid_idx];
      ip = WiFi.localIP();
      server_errors = 0;
    }
  }
  refreshScreen();
  if (wifi_status != WL_CONNECTED) {  // wifi connection failed
    printError("ERROR: Failed to connect to wifi.");
    server_errors = SERVER_ERR_LIMIT;
  }
}

// ---------------------------------------------------------------------------------------
// --- HTTP FUNCTIONS                                                                  ---
// ---------------------------------------------------------------------------------------
void getDateTime(DatetimeInfo option) {
  // make a request to the REST API server for the current time or date
  String getRequest = String(HTTP_SERVER) + "?func=";
  if (option == TIME_INFO) {
    getRequest += "timereq";
  } else {
    getRequest += "datereq";    
  }
  http.begin(getRequest);
  int httpResponseCode = http.GET(); // See here for list of possible responses: https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266HTTPClient/src/ESP8266HTTPClient.h#L45
  String httpData = http.getString();
  http.end();
  if (httpResponseCode == 200) {
    server_errors = 0;
    if (option == TIME_INFO) {
      timeStr = httpData;
      printTime();
    } else {
      dateStr = httpData;
      printDate();
    }
  } else {
    printError("Error contacting server");
    timeStr = "--:--";
    dateStr = "-";
    server_errors++;
  }
}

void getWeather() {
  // make a request to the REST API server for the current weather in my local town
  String getRequest = String(HTTP_SERVER) + "?func=weather";
  http.begin(getRequest);
  int httpResponseCode = http.GET(); // See here for list of possible responses: https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266HTTPClient/src/ESP8266HTTPClient.h#L45
  String httpData = http.getString();
  http.end();
  if (httpResponseCode == 200) {
    weatherStr = httpData;
    printWeather();
  }
}

void sendDataReport() {
  // send info to the REST API server
  String getRequest = String(HTTP_SERVER) + "?func=report&sensor=" + String(SENSOR_NAME);
  getRequest += "&data=" + dataString(DATA_FMT_REPORT);
  http.begin(getRequest);
  int httpResponseCode = http.GET();
  String httpData = http.getString();
  http.end();
  if (httpResponseCode == 200) {
    server_errors = 0;
    printMsg(httpData);
  } else {
    printError(String(httpResponseCode) + ": " + httpData);
    server_errors++;
  }
}

// ***************************************************************************************
// ***  SETUP                                                                          ***
// ***************************************************************************************
void setup() {
  tft.begin();
  // set orientation of screen.
  // 0  - vertical, with wide area/pins at bottom
  // 2  - vertical, with wide area/pins at top
  tft.setRotation(0);
  dht.begin();
  pinMode(REFRESH_PIN, INPUT);
  wifiConnect();
  printMsg("Hello world");    // why not?
}

// ***************************************************************************************
// ***  LOOP                                                                           ***
// ***************************************************************************************

uint8_t refreshPinState = HIGH;

void loop() {
  //uint32_t now = millis();  // would be another way to go
  
  if (sensor_counter == SENSOR_INTERVAL) {
    // if(now - lastSensorRead >= SENSOR_INTERVAL) { .. }
    sensor_counter = 0;
    getReadings();
    printTemperature();
  } 
  if (report_counter == REPORT_INTERVAL) {
    report_counter = 0;
    getReadings();
    sendDataReport();
  }
  if (time_counter == TIME_INTERVAL) {
    time_counter = 0;
    getDateTime(TIME_INFO);
    printTime();
  }
  if (date_counter == DATE_INTERVAL) {
    date_counter = 0;
    getDateTime(DATE_INFO);
    printDate();
  }
  if (weather_counter == WEATHER_INTERVAL) {
    weather_counter = 0;
    getWeather();
    printWeather();
  }
  
  if (msg_counter >= MSG_PERSIST_CYCLES) {
    msg_counter = 0;
    clearMsg();
  }
  if (err_counter >= ERR_PERSIST_CYCLES) {
    err_counter = 0;
    clearError();
  }
  if (msg_counter > 0) msg_counter++;
  if (err_counter > 0) err_counter++;
  
  sensor_counter++; time_counter++; date_counter++; report_counter++; weather_counter++;
 
  if (server_errors >= SERVER_ERR_LIMIT) {
    timeStr = "--:--";
    dateStr = ".";
    wifiConnect();
  }

  if(PCB_REVISION > 1.0) {
    // the later boards have a reset button that clears the min/max settings
    refreshPinState = digitalRead(REFRESH_PIN);
    if(refreshPinState == LOW) {
      temp_min = 99;
      temp_max = -99;
      getReadings();
      printTemperature();
      printMsg("Min/Max reset");
      while(refreshPinState == LOW) {
        delay(100);
        refreshPinState = digitalRead(REFRESH_PIN);
      }
    }
  }
  
  delay(LOOP_INTERVAL);

}
