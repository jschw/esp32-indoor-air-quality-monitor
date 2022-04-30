#include <Arduino.h>
#include "RTClib.h"
#include "time.h"
#include "Preferences.h"
#include "WiFi.h"
#include "ESPmDNS.h"

#include "LEDMatrix.h"
#include "EventLog.h"

//FastLED settings
#define FASTLED_ESP32
#include "FastLED.h"

//Number of LEDs in Array for front/back
//Grosse Uhr
#define NUM_LEDS_BL 28
//Kleine Uhr
//Achtung: Hier passt irgendwas mit der Anzahl nicht!
//Mit BL
//#define NUM_LEDS_BL 17
//Kein BL
//#define NUM_LEDS_BL 0

#define NUM_LEDS (110+NUM_LEDS_BL)

//Data pins for front/back
#define DATA_PIN 18

CRGB leds[NUM_LEDS];

//Set current firmware version
extern const char SW_VERSION[] = {"4.1.2"};
//1.0.0: (Jan 2018) Initial proof of concept on AVR platform
//1.1.0: Fade in/out added
//2.0.0: Ported to ESP32 platform
//2.1.0: Standbymode added, support for lightsensor added
//2.2.0: Support for BL added
//2.3.0: Animated standbymode "Twinkle" added
//2.4.0: Support for touch sensor added
//3.0.0: (Jul 2019) WiFi support with web based config app added
//3.2.0: Minor bugfixes -> Initial release
//4.0.0: (Dez 2019) Update to SDK version 1.4.0, reduced clock speed to 160MHz
//4.1.0: Timesync with NTP server added, graphic show mode added, minute display with BL added
//4.1.1: EventLog added
//4.1.2: Redirection to startpage after config change added in web app

String netDevName = "wordclock";
String deviceIP = "";

int commErrorCounter = 0;

RTC_DS3231 rtc;
//RTC_DS1307 rtc;


Preferences settings;

const int ldrPin = 32;
const int touchPin = 14;

String mode = "";
String standbyMode = "TWINKLE";
String standbyModeSaved = standbyMode;

bool standby = false;
bool enableAutoStandby = true;
bool forceStandby = false;
bool isInStandby = false;
bool isInPresentationMode = false;
//debug settings
bool debugmode = false;
//send matrix as characters to serial
bool debugmatrix = false;

unsigned long refresh_interval=500;
unsigned long previousMillis=0;
bool firstRun = true;

//Time
int current_min = 0;
int current_hour = 0;
int current_min_rtc = 0;
int hour_unconverted = 0;
int previousDaytimeSection = -1;
bool refreshed = false;
bool tmpFadeSetting = false;
bool firstCycle = true;
bool isSummertime = true;
bool enableTimeSync = true;
//Time sync NTP settings
const char* ntpServer = "europe.pool.ntp.org";
const long  gmtOffset_sec = 3600;
int   daylightOffset_sec = 3600;
bool timeSynced = false;
bool timeSyncFail = false;


//LED settings
int rgbcolor[3] = {201,226,255};
int rgbcolorSec1[3] = {201,226,255};
int rgbcolorSec3[3] = {201,226,255};
int rgbcolorBL[3] = {201,226,255};
int rgbcolorMin[3] = {255,0,0};
uint8_t brightness = 40;
//automatic brightness adjustment
bool autoBrightness = true;
uint8_t lastBrightnessChange = 0;
//set fade in/out effect
bool fadeInOut = true;
//is it a fade to/from standby?
bool fadeInBL = true;
bool fadeOutBL = false;
//use different color for section 1
bool colorSec1 = false;
//use different color for section 3
bool colorSec3 = false;
//if both false -> all leds use rgbcolor
int colorMode = 1;
//0 = manual
//1 = auto daylight default
//2 = auto daylight wifi
//3 = auto weather


//====== Backlight functionality ======
bool enableBacklight = false;
bool blockBacklight = false;
bool blNeedRefresh = true;
//Modes:
//daylight, rainbow, manual
String backlightMode = "rainbow";
static uint8_t hue_actual = 0;
static uint8_t sat = 130;
static uint8_t val = 255;

unsigned long currentMillisBLTimer = 0;
unsigned long previousMillisBLTimer = 0;
int delay_backlight_rainbow = 400;

//display minutes with backlight
//Modes:
//0: Off , 1: Middle of each side, 2: Edges
//3: Middle only one, 4: Edges only one
int displayMinutes = 0;

//Set colormode for minutes display with BL
//0 -> Manual color set with rgbcolorMin
//1 -> Automatic changing color with daylight
int colormodeMin = 0;


//======= other settings ========
//language
//0 -> Hochdeutsch
//1 -> Schwaebisch
//2 -> Englisch
int clockLang = 0;
//turn sections on or off
bool activateSec1 = true;
bool activateSec3 = true;
//activate touch functionality on frame
bool enableTouch = false;

//turn on standby mode at sensor value below lightlevel
int lightlevel = 10;
int lastStandbyValue = 0;

LEDMatrix ledMatrixObj;
//set pointer to public array in LEDMatrix object
//int (*ledPanelMatrix)[10][11] = &ledMatrixObj.ledMatrix;
//- Obsolete: matrix is directly accessible now -
int ledMatrixSaved[10][11];

////=====Logging functionality======
EventLog logging;
bool logToSerial;


////=====WiFi functionality======
bool wifiActive = false;
static volatile bool wifiConnected = false;
//#define AP_SSID "Wordclock_WiFi_Config";       //can set ap hostname here
String wifi_config_SSID = "Wordclock_WiFi_Config";
WiFiServer server(80);
WiFiClient client;
String wifiSSID="", wifiPassword="";
bool wifiConfigMode = false;
String header="";
unsigned long currentMillisWifiTimer = 0;
unsigned long previousMillisWifiTimer = 0;
unsigned long currentMillisWifiTimer2 = 0;
unsigned long previousMillisWifiTimer2 = 0;

void wifiStartAPmode();
void wifiAPClientHandle();
void wifiConnectedHandle(WiFiClient client);
void wifiConnect();
double getWifiSignalStrength();

int cycleCounter = 0;


//Functions
void controlSwitch();
void controlBacklight();
int getLightSensorValue();
int readTouchSens();
void setPixel(int Pixel, int red, int green, int blue);
void testLedMatrix(int red, int blue, int green);
void testBLMatrix(int red, int blue, int green);
void testPower();
void setLedMatrix(int hour, int min);
void showStrip();
void setMinutesBL(int min);
void saveMatrix(bool reset);
bool matrixChanges();
void fadeInChanges(int speed);
void fadeOutChanges(int speed);
void softRefresh();
bool inSec1(int led);
bool inSec3(int led);
void setDefaults();
void setDaylightColor(int hour);
void setDaylightColorMin(int hour);
void setAutoBrightness();
String getWiFiKey(bool keyTypeShort); //keyTypeShort=true -> first+last two digits, otherwise -> complete
String getConfig();
String getDebugData();
void importConfig(String confStr);
bool summertime_EU(int year, byte month, byte day, byte hour, byte tzHours);
void getTimeNTP(int &hour, int &min, int &sec, int &day, int &month, int &year);
int convertMin(int min);
void Log_println(String msg, int loglevel=0); //<-- Wrapper function for logging class

//Config
bool readConfigFromFlash();
void writeConfigToFlash();
bool configStored = false;


//Standby sequences
void standby_Twinkle(bool forced);

//void setLedRow(int row, int array[]);

void refreshLedPanel(bool serialOutput);
void clearLedPanel();
void clearBacklight();

void setup()
{
	//Set minutes display off for small clock or clock w/o BL
	if(NUM_LEDS_BL != 28) displayMinutes = 0;

	//Open serial interface
	Serial.begin(9600);

	//Configure logging, turn on or off
	logToSerial = true;
	logging.setLineBreak("<br>\n");

	//Turn off Bluetooth
	btStop();

	if (!rtc.begin()) {
		Log_println("Couldn't find RTC",2);
		while (1);
	}

	//init FastLED
	pinMode(DATA_PIN, OUTPUT);
	FastLED.addLeds<WS2812, DATA_PIN, RGB>(leds, NUM_LEDS);

	//init random number generator
	randomSeed(analogRead(35));

	//Read config if stored in Flash or initialize
	//Reset EEPROM if corrupted
	//setDefaults();
	if(readConfigFromFlash()) configStored=true;
	else writeConfigToFlash();

	//light blue wifi sign and wait to turn wifi on or not
	//LED numbers: W 52 I 35 F 55 I 54
	//or: W 93 I 82 F 55 i 54
	setPixel(52,40,115,255);
	setPixel(35,40,115,255);
	setPixel(55,40,115,255);
	setPixel(54,40,115,255);
	showStrip();
	delay(200);
	Log_println("Ready for Wifi!");

	//Check if ssid / key are already stored
	if(!wifiSSID.equals("") && !wifiPassword.equals("")) wifiActive = true;


	//wait 4 seconds for interaction
	currentMillisWifiTimer = millis();
	previousMillisWifiTimer = currentMillisWifiTimer;
	bool wifiActivatedPrevLoop = false;

	while((unsigned long)(currentMillisWifiTimer - previousMillisWifiTimer) <= 4000){
		//Check touch sensor
		if(wifiActivatedPrevLoop) break;
		if(readTouchSens()>=30){
			//if sensor touched
			//wait for second touch
			currentMillisWifiTimer2 = millis();
			previousMillisWifiTimer2 = currentMillisWifiTimer2;
			delay(100);
			while((unsigned long)(currentMillisWifiTimer2 - previousMillisWifiTimer2) <= 500){
				if(readTouchSens()>=30){
					clearLedPanel();
					delay(400);
					setPixel(52,40,115,255);
					setPixel(35,40,115,255);
					setPixel(55,40,115,255);
					setPixel(54,40,115,255);
					showStrip();
					delay(400);
					clearLedPanel();
					delay(400);
					setPixel(52,40,115,255);
					setPixel(35,40,115,255);
					setPixel(55,40,115,255);
					setPixel(54,40,115,255);
					showStrip();
					delay(400);
					clearLedPanel();
					delay(400);
					setPixel(52,250,240,30);
					setPixel(35,250,240,30);
					setPixel(55,250,240,30);
					setPixel(54,250,240,30);
					showStrip();

					Log_println("WiFi is Activated!");

					wifiActive = true;
					wifiConfigMode = true;
					wifiActivatedPrevLoop = true;

					currentMillisWifiTimer2=0;
					previousMillisWifiTimer2=0;
					break;
				}
				currentMillisWifiTimer2 = millis();
			}
		}else{
			//clearLedPanel();
		}
		currentMillisWifiTimer = millis();
	}


	if(!wifiActive){
		Log_println("WiFi not active, continue booting...");
		WiFi.mode(WIFI_OFF);
		clearLedPanel();
	}else{
		//try to connect to wifi if turned on
		Log_println("WiFi active, try to connect...");
		wifiConnect();
	}

	//get time from NTP if wifi is connected
	if(wifiConnected && enableTimeSync){
		//set daytime offset correct
		if(isSummertime) daylightOffset_sec = 3600;
		else daylightOffset_sec = 0;

		//init and get the time
		configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

		Log_println("Synchronizing time with server '" + String(ntpServer) + "'...");
		int timeSyncTrials = 0;

		while(!time(nullptr)){
			//Log_println(".");
			delay(200);
			if(timeSyncTrials > 10)
			{
				timeSyncFail = true;
				break;
			}
			else timeSyncTrials++;
		}

	}

	//init matrix
	saveMatrix(true);

	//set language
	ledMatrixObj.clockLanguage = clockLang;

	//set sections on or off
	ledMatrixObj.sec1Active = activateSec1;
	ledMatrixObj.sec3Active = activateSec3;

	//get daylight color first time if turned on
	if(colorMode==1) setDaylightColor(rtc.now().hour());
	if(colormodeMin==1) setDaylightColorMin(rtc.now().hour());

	//test backlight
	//fill_solid(backlight, NUM_LEDS_BL, CRGB::Red);
	//FastLED[1].showLeds(brightness_BL);
}

void Log_println(String msg, int loglevel){
	//Wrapper function for logging with timestamp and redirect to serial
	if(logToSerial) Serial.println(msg);

	DateTime now = rtc.now();
	logging.setTimestamp((String)now.hour() + ":" + (String)now.minute() + " --> ");

	logging.println(msg, loglevel);

}

void controlSwitch(){
	//SET_TIME 2017,12,28,16,0,0
	//GET_TIME
	//GET_INTERNETTIME
	//TOGGLE_TIMESYNCSTATE
	//TOGGLE_SUMMERTIME_MARKER  <- directly toggles the bit "isSummerTime" without correcting the rtc time
	//TOGGLE_SUMMERTIME  <- toggles summertime marker WITH correction the time
	//Info: true -> false -> minus 1 hour , false -> true -> plus 1 hour
	//SET_LANGUAGE 0
	//SET_DEVICENAME NeuerName <- set the Wifi DNS name: http://devicename.local
	//SET_STANDBY_JUS  SET_STANDBY_CHE  SET_STANDBY_XMASTREE  SET_STANDBY_NULL  SET_STANDBY_OFF SET_STANDBY_TWINKLE
	//TOGGLE_AUTO_STANDBY
	//GOSLEEP
	//SET_LIGHTLEVEL 12345
	//SET_LIGHTLEVEL_ACTUAL
	//GET_LIGHTLEVEL
	//SET_BRIGHTNESS 40 (values: 40-255)
	//SET_BRIGHTNESS_AUTO
	//RGB_ALL 255,255,255
	//RGB_SEC1 255,255,255
	//RGB_SEC3 255,255,255
	//SET_COLORTEMP 2700
	//SET_TESTMODE
	//SET_DEBUGMODE
	//TEST_TIME 12:30
	//REFRESH
	//LED_TEST
	//VERSION
	//SET_DEFAULTS
	//TOGGLE_SEC1
	//TOGGLE_SEC3
	//GET_SERIAL
	//GET_MEM_USAGE
	//TOGGLE_SOFTTOUCH
	//SET_COLORMODE_AUTO_DAYLIGHT
	//SET_COLORMODE_AUTO_DAYLIGHT_WIFI  <-- not implemented
	//SET_COLORMODE_AUTO_WEATHER  <-- not implemented
	//SET_COLORMODE_MANUAL
	//SET_BACKLIGHT_OFF
	//SET_BACKLIGHT_ON -> Switch on to last saved state
	//SET_BACKLIGHTMODE_DAYLIGHT
	//SET_BACKLIGHTMODE_RAINBOW
	//SET_BACKLIGHTMODE_MANUAL 255,255,255
	//SET_BACKLIGHTMODE_FREEZE
	//SET_MINUTES_DISPLAY_MODE 0 <-- ausgeschaltet
	//SET_MINUTES_DISPLAY_MODE 1 <-- an den Kanten
	//SET_MINUTES_DISPLAY_MODE 2 <-- an den Ecken
	//SET_MINUTES_DISPLAY_COLOR 255,255,255
	//SET_MINUTES_DISPLAY_COLOR_DAYLIGHT
	//GET_CONFIG
	//IMPORT_CONFIG val1,val2,val3,...

	if(!mode.equals("")){

		if(mode.indexOf("SET_TIME") != -1){
		//set actual time

		//split command
		int spaceIndex = mode.indexOf(' ');
		int secondSpaceIndex = mode.indexOf(' ', spaceIndex + 1);
		String timeString = mode.substring(spaceIndex + 1, secondSpaceIndex);

		//split time string input
		int timeIndex = timeString.indexOf(',');
		int secondTimeIndex = timeString.indexOf(',', timeIndex + 1);
		int thirdTimeIndex = timeString.indexOf(',', secondTimeIndex + 1);
		int forthTimeIndex = timeString.indexOf(',', thirdTimeIndex + 1);
		int fithTimeIndex = timeString.indexOf(',', forthTimeIndex + 1);

		String s_year, s_month, s_day, s_hour, s_min, s_sec;

		s_year = timeString.substring(0, timeIndex);
		s_month = timeString.substring(timeIndex + 1, secondTimeIndex);
		s_day = timeString.substring(secondTimeIndex + 1, thirdTimeIndex);
		s_hour = timeString.substring(thirdTimeIndex + 1, forthTimeIndex);
		s_min = timeString.substring(forthTimeIndex + 1, fithTimeIndex);
		s_sec = timeString.substring(fithTimeIndex + 1);

		rtc.adjust(DateTime(s_year.toInt(), s_month.toInt(), s_day.toInt(), s_hour.toInt(), s_min.toInt(), s_sec.toInt()));
		//rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));

		Log_println("Zeit erfolgreich eingestellt.");

		tmpFadeSetting = fadeInOut;
		blNeedRefresh = true;
		softRefresh();

		//		Debug
		//		Log_println(s_year);
		//		Log_println(s_month);
		//		Log_println(s_day);
		//		Log_println(s_hour);
		//		Log_println(s_min);
		//		Log_println(s_sec);


	}else if(mode.equals("GET_TIME")){
		//get actual time
		DateTime now = rtc.now();

		Serial.print(now.day(), DEC);
		Serial.print('/');
		Serial.print(now.month(), DEC);
		Serial.print('/');
		Serial.print(now.year(), DEC);
		Serial.print("--");
		Serial.print(now.hour(), DEC);
		Serial.print(':');
		Serial.print(now.minute(), DEC);
		Serial.print(':');
		Serial.print(now.second(), DEC);
		Serial.println();

	}else if(mode.equals("GET_INTERNETTIME")){
		//get actual internet time
		if(timeSynced){
			int hour_ntp, min_ntp, sec_ntp, day_ntp, mon_ntp, year_ntp;
			getTimeNTP(hour_ntp, min_ntp, sec_ntp, day_ntp, mon_ntp, year_ntp);

			Serial.print(day_ntp, DEC);
			Serial.print('/');
			Serial.print(mon_ntp, DEC);
			Serial.print('/');
			Serial.print(year_ntp, DEC);
			Serial.print("--");
			Serial.print(hour_ntp, DEC);
			Serial.print(':');
			Serial.print(min_ntp, DEC);
			Serial.print(':');
			Serial.print(sec_ntp, DEC);
			Serial.println();
		}
		else Serial.println("Time not synchronized.");

	}else if(mode.equals("TOGGLE_SUMMERTIME")){
		//toggle marker for summertime with changing time
		isSummertime = !isSummertime;

		DateTime now = rtc.now();

		if(isSummertime){
			//change false -> true -> plus 1 hour
			rtc.adjust(DateTime(now.year(), now.month(), now.day(), now.hour()+1, now.minute(), now.second()));
		}else{
			//change true -> false -> minus 1 hour
			rtc.adjust(DateTime(now.year(), now.month(), now.day(), now.hour()-1, now.minute(), now.second()));
		}

		settings.begin("settings", false);
		settings.putBool("isSummertime",isSummertime);
		settings.end();

		if(isSummertime) Log_println("Die Uhr wurde auf Sommerzeit umgestellt.");
		else Log_println("Die Uhr wurde auf Winterzeit umgestellt.");

		tmpFadeSetting = fadeInOut;
		blNeedRefresh = true;
		softRefresh();

	}else if(mode.equals("TOGGLE_SUMMERTIME_MARKER")){
		//toggle marker for summertime without changing time
		isSummertime = !isSummertime;

		settings.begin("settings", false);
		settings.putBool("isSummertime",isSummertime);
		settings.end();

		if(isSummertime) Log_println("Die Sommerzeit ist nun eingeschaltet.");
		else Log_println("Die Sommerzeit ist nun ausgeschaltet.");

	}else if(mode.equals("GET_CONFIG")){
		//get all variable values saved in flash
		Serial.println(getConfig());

	}else if(mode.indexOf("IMPORT_CONFIG") != -1){
		//import configuration set
		int spaceIndex = mode.indexOf(' ');
		int secondSpaceIndex = mode.indexOf(' ', spaceIndex + 1);
		String s_conf_str = mode.substring(spaceIndex + 1, secondSpaceIndex);

		Log_println("Die Konfiguration wird gespeichert und die Uhr anschliessend neu gestartet.");

		importConfig(s_conf_str);

	}else if(mode.indexOf("SET_STANDBY") != -1){
		//set standby mode
		if(mode.equals("SET_STANDBY_JUS")){
			standbyMode = "JUS";
		}else if(mode.equals("SET_STANDBY_CHE")){
			standbyMode = "CHE";
		}else if(mode.equals("SET_STANDBY_XMASTREE")){
			standbyMode = "XMASTREE";
		}else if(mode.equals("SET_STANDBY_NULL")){
			standbyMode = "NULL";
		}else if(mode.equals("SET_STANDBY_OFF")){
			standbyMode = "OFF";
		}else if(mode.equals("SET_STANDBY_TWINKLE")){
			standbyMode = "TWINKLE";
		}

		settings.begin("settings", false);
		settings.putString("standbyMode", standbyMode);
		settings.end();

		Log_println("Standby mode erfolgreich eingestellt.");

	}else if(mode.indexOf("SET_LIGHTLEVEL") != -1){
		//set lightlevel for standby mode
		int spaceIndex = mode.indexOf(' ');
		int secondSpaceIndex = mode.indexOf(' ', spaceIndex + 1);
		String s_lightval = mode.substring(spaceIndex + 1, secondSpaceIndex);

		lightlevel = s_lightval.toInt();

		settings.begin("settings", false);
		settings.putInt("lightlevel", lightlevel);
		settings.end();

		Log_println("Lightlevel wurde eingestellt.");

	}else if(mode.indexOf("SET_LIGHTLEVEL_ACTUAL") != -1){
		//set lightlevel for standby mode to actual value

		lightlevel = getLightSensorValue();

		settings.begin("settings", false);
		settings.putInt("lightlevel", lightlevel);
		settings.end();

		Log_println("Lightlevel wurde auf aktuelle Helligkeit eingestellt.");

	}else if(mode.equals("SET_BRIGHTNESS_AUTO")){
		//set auto brightness mode on
		autoBrightness = true;
		setAutoBrightness();
		showStrip();

		settings.begin("settings", false);
		settings.putBool("autoBrightness", autoBrightness);
		settings.end();

		Log_println("Die automatische Helligkeitsanpassung ist eingeschaltet.");

	}else if(mode.indexOf("SET_BRIGHTNESS") != -1){
		//set brightness for scaling
		int spaceIndex = mode.indexOf(' ');
		int secondSpaceIndex = mode.indexOf(' ', spaceIndex + 1);
		String s_brightness = mode.substring(spaceIndex + 1, secondSpaceIndex);

		brightness = s_brightness.toInt();

		autoBrightness = false;
		showStrip();

		//save to Flash
		writeConfigToFlash();

		//settings.begin("settings", false);
		//settings.putInt("brightness", brightness);
		//settings.putBool("autoBrightness", autoBrightness);
		//settings.end();

		Log_println("Die Helligkeit wurde eingestellt.");

	}else if(mode.equals("GET_LIGHTLEVEL")){
		//get actual light sensor value

		Log_println((String)getLightSensorValue());

	}else if(mode.indexOf("SET_LANGUAGE") != -1){
		//set lightlevel for standby mode
		int spaceIndex = mode.indexOf(' ');
		int secondSpaceIndex = mode.indexOf(' ', spaceIndex + 1);
		String lang = mode.substring(spaceIndex + 1, secondSpaceIndex);

		clockLang = lang.toInt();

		settings.begin("settings", false);
		settings.putInt("clockLang", clockLang);
		settings.end();

		Log_println("Die Sprache wurde eingestellt.");

	}else if(mode.equals("SET_DEBUGMODE")){
		//set debugmode on/off
		debugmode = !debugmode;

		settings.begin("settings", false);
		settings.putBool("debugmode", debugmode);
		settings.end();

		if(debugmode) Log_println("Der Debugmodus ist jetzt eingeschaltet.");
		else Log_println("Der Debugmodus ist jetzt ausgeschaltet.");

	}else if(mode.indexOf("RGB_ALL") != -1){
		//set color

		//all leds
		colorSec1=false;
		colorSec3=false;

		colorMode = 0;

		//split command
		int spaceIndex = mode.indexOf(' ');
		int secondSpaceIndex = mode.indexOf(' ', spaceIndex + 1);
		String colorString = mode.substring(spaceIndex + 1, secondSpaceIndex);

		//split color string input
		int colorIndex = colorString.indexOf(',');
		int secondColorIndex = colorString.indexOf(',', colorIndex + 1);

		String s_red, s_green, s_blue;

		s_red = colorString.substring(0, colorIndex);
		s_green = colorString.substring(colorIndex + 1, secondColorIndex);
		s_blue = colorString.substring(secondColorIndex+1);

		rgbcolor[0] = s_red.toInt();
		rgbcolor[1] = s_green.toInt();
		rgbcolor[2] = s_blue.toInt();

		rgbcolorSec1[0] = s_red.toInt();
		rgbcolorSec1[1] = s_green.toInt();
		rgbcolorSec1[2] = s_blue.toInt();

		rgbcolorSec3[0] = s_red.toInt();
		rgbcolorSec3[1] = s_green.toInt();
		rgbcolorSec3[2] = s_blue.toInt();

		//save to Flash
		writeConfigToFlash();


		Log_println("Die Farbe fuer den gesamten Bereich wurde eingestellt.");

		//soft refresh without fade in/out
		tmpFadeSetting = fadeInOut;
		fadeInOut = false;
		softRefresh();

	}else if(mode.indexOf("RGB_SEC1") != -1){
		//set color

		//all leds
		colorSec1=true;

		colorMode = 0;

		//split command
		int spaceIndex = mode.indexOf(' ');
		int secondSpaceIndex = mode.indexOf(' ', spaceIndex + 1);
		String colorString = mode.substring(spaceIndex + 1, secondSpaceIndex);

		//split color string input
		int colorIndex = colorString.indexOf(',');
		int secondColorIndex = colorString.indexOf(',', colorIndex + 1);

		String s_red, s_green, s_blue;

		s_red = colorString.substring(0, colorIndex);
		s_green = colorString.substring(colorIndex + 1, secondColorIndex);
		s_blue = colorString.substring(secondColorIndex+1);

		rgbcolorSec1[0] = s_red.toInt();
		rgbcolorSec1[1] = s_green.toInt();
		rgbcolorSec1[2] = s_blue.toInt();

		//save to Flash
		writeConfigToFlash();


		Log_println("Die Farbe fuer Section 1 wurde eingestellt.");

		//soft refresh without fade in/out
		tmpFadeSetting = fadeInOut;
		fadeInOut = false;
		softRefresh();

	}else if(mode.indexOf("RGB_SEC3") != -1){
		//set color

		//all leds
		colorSec3=true;

		colorMode = 0;

		//split command
		int spaceIndex = mode.indexOf(' ');
		int secondSpaceIndex = mode.indexOf(' ', spaceIndex + 1);
		String colorString = mode.substring(spaceIndex + 1, secondSpaceIndex);

		//split color string input
		int colorIndex = colorString.indexOf(',');
		int secondColorIndex = colorString.indexOf(',', colorIndex + 1);

		String s_red, s_green, s_blue;

		s_red = colorString.substring(0, colorIndex);
		s_green = colorString.substring(colorIndex + 1, secondColorIndex);
		s_blue = colorString.substring(secondColorIndex+1);

		rgbcolorSec3[0] = s_red.toInt();
		rgbcolorSec3[1] = s_green.toInt();
		rgbcolorSec3[2] = s_blue.toInt();

		//save to Flash
		writeConfigToFlash();

		Log_println("Die Farbe fuer Section 3 wurde eingestellt.");

		//soft refresh without fade in/out
		tmpFadeSetting = fadeInOut;
		fadeInOut = false;
		softRefresh();

	}else if(mode.indexOf("TEST_TIME") != -1){
		//set color

		//split command
		int spaceIndex = mode.indexOf(' ');
		int secondSpaceIndex = mode.indexOf(' ', spaceIndex + 1);
		String testString = mode.substring(spaceIndex + 1, secondSpaceIndex);

		//split color string input
		int testIndex = testString.indexOf(':');
		int secondTestIndex = testString.indexOf(':', testIndex + 1);

		String s_hour, s_min;

		s_hour = testString.substring(0, testIndex);
		s_min = testString.substring(testIndex+1,secondTestIndex);

		int hour = s_hour.toInt();
		int min = s_min.toInt();

		//convert 12/24 hour mode
		if(hour>12) hour = hour-12;

		debugmode = true;

		setLedMatrix(hour,min);

	}else if(mode.equals("REFRESH")){
		tmpFadeSetting = fadeInOut;
		blNeedRefresh = true;
		softRefresh();
	}else if(mode.equals("LED_TEST")){
		Log_println("Teste LEDs...");
		testLedMatrix(rgbcolor[0], rgbcolor[1], rgbcolor[2]);
	}else if(mode.indexOf("SET_COLORTEMP") != -1){
		//set color temperature for all leds
		int spaceIndex = mode.indexOf(' ');
		int secondSpaceIndex = mode.indexOf(' ', spaceIndex + 1);
		String s_colortemp = mode.substring(spaceIndex + 1, secondSpaceIndex);

		int colortemp = s_colortemp.toInt();

		//calculate color temperature
		ledMatrixObj.calcColorTemp(colortemp);
		if(debugmode) Log_println((String)ledMatrixObj.getTempRed()+","+(String)ledMatrixObj.getTempGreen()+";"+(String)ledMatrixObj.getTempBlue());

		//set all colors to calculated temp
		rgbcolor[0] = ledMatrixObj.getTempRed();
		rgbcolor[1] = ledMatrixObj.getTempGreen();
		rgbcolor[2] = ledMatrixObj.getTempBlue();
		rgbcolorSec1[0] = ledMatrixObj.getTempRed();
		rgbcolorSec1[1] = ledMatrixObj.getTempGreen();
		rgbcolorSec1[2] = ledMatrixObj.getTempBlue();
		rgbcolorSec3[0] = ledMatrixObj.getTempRed();
		rgbcolorSec3[1] = ledMatrixObj.getTempGreen();
		rgbcolorSec3[2] = ledMatrixObj.getTempBlue();

		//reset color setting
		colorMode = 0;
		colorSec1 = false;
		colorSec3 = false;

		//save to Flash
		writeConfigToFlash();

		Log_println("Die Farbtemperatur wurde eingestellt.");

		//soft refresh without fade in/out
		tmpFadeSetting = fadeInOut;
		fadeInOut = false;
		softRefresh();
	}else if(mode.equals("VERSION")){
		//get actual firmware version

		Log_println(String(SW_VERSION));

	}else if(mode.equals("SET_DEFAULTS")){
		setDefaults();
		writeConfigToFlash();

		//soft refresh without fade in/out
		ledMatrixObj.clearMatrix();
		saveMatrix(true);
		clearLedPanel();
		current_min = 0;
		Log_println("Die Standardeinstellungen wurden wiederhergestellt.");
	}else if(mode.equals("TOGGLE_SEC1")){
		activateSec1 = !activateSec1;
		ledMatrixObj.sec1Active = activateSec1;

		//soft refresh without fade in/out
		ledMatrixObj.clearMatrix();
		saveMatrix(true);
		clearLedPanel();
		current_min = 0;

		settings.begin("settings", false);
		settings.putBool("activateSec1", activateSec1);
		settings.end();

		if(activateSec1) Log_println("Section 1 wurde eingeschaltet.");
		else Log_println("Section 1 wurde ausgeschaltet..");
	}else if(mode.equals("TOGGLE_SEC3")){
		activateSec3 = !activateSec3;
		ledMatrixObj.sec3Active = activateSec3;

		//soft refresh without fade in/out
		ledMatrixObj.clearMatrix();
		saveMatrix(true);
		clearLedPanel();
		current_min = 0;

		settings.begin("settings", false);
		settings.putBool("activateSec3", activateSec3);
		settings.end();

		if(activateSec3) Log_println("Section 3 wurde eingeschaltet.");
		else Log_println("Section 3 wurde ausgeschaltet..");
	}else if(mode.equals("GET_SERIAL")){
		Log_println(getWiFiKey(false));
	}else if(mode.equals("GET_MEM_USAGE")){
		//Get actual RAM usage
		Log_println("Verfuegbarer RAM: " + String(ESP.getHeapSize() - ESP.getFreeHeap()) + " / " + String(ESP.getHeapSize()) + " Byte.");
	}else if(mode.equals("SET_COLORMODE_AUTO_DAYLIGHT")){
		//set colormode to auto color temp daylight
		colorMode = 1;
		setDaylightColor(hour_unconverted);
		//soft refresh without fade in/out
		tmpFadeSetting = fadeInOut;
		fadeInOut = false;
		softRefresh();

		settings.begin("settings", false);
		settings.putInt("colorMode", colorMode);
		settings.end();

		Log_println("Farbmodus ist nun Tageszeitabhaengig.");

	}else if(mode.equals("SET_COLORMODE_MANUAL")){
		//set colormode to manual coloring
		colorMode = 0;

		settings.begin("settings", false);
		settings.putInt("colorMode", colorMode);
		settings.end();
		Log_println("Farbmodus ist manuell konfiguriert.");

	}else if(mode.equals("TOGGLE_SOFTTOUCH")){
		//turn on/off softtouch functionality
		enableTouch = !enableTouch;

		settings.begin("settings", false);
		settings.putBool("enableTouch", enableTouch);
		settings.end();

		if(enableTouch) Log_println("Softtouch wurde eingeschaltet.");
		else Log_println("Softtouch wurde ausgeschaltet.");
	}else if(mode.equals("TOGGLE_AUTO_STANDBY")){
		//turn on/off light-sensitive standby
		enableAutoStandby = !enableAutoStandby;

		//settings.begin("settings", false);
		//settings.putBool("enableAutoStandby", enableAutoStandby);
		//settings.end();

		//save to flash
		writeConfigToFlash();

		if(enableAutoStandby) Log_println("Lichtabhaengiger Standby wurde eingeschaltet.");
		else Log_println("Lichtabhaengiger Standby wurde ausgeschaltet.");
	}else if(mode.equals("SET_BACKLIGHT_OFF")){
		//turn on/off backlight functionality
		enableBacklight = false;
		if(displayMinutes>0){
			//if BL is off -> direct control min display
			clearBacklight();
			setMinutesBL(convertMin(current_min_rtc));
			FastLED.show();
		}else clearBacklight();

		settings.begin("settings", false);
		settings.putBool("enableBacklight", enableBacklight);
		settings.end();

		Log_println("Das Backlight wurde ausgeschaltet.");
	}else if(mode.equals("SET_BACKLIGHT_ON")){
		//turn on/off backlight functionality
		enableBacklight = true;
		blNeedRefresh = true;

		settings.begin("settings", false);
		settings.putBool("enableBacklight", enableBacklight);
		settings.end();

		Log_println("Das Backlight wurde eingeschaltet.");
	}else if(mode.indexOf("SET_BACKLIGHTMODE") != -1){
		if(NUM_LEDS_BL==0){
			Log_println("Funktion Backlight ist nur mit integriertem Hintergrundlicht verfuegbar.",1);
		}else{

			//set backlight mode
			if(mode.equals("SET_BACKLIGHTMODE_DAYLIGHT")){
				//switch to general daylight mode if not already set

				if(colorMode != 1){
					//set colormode to auto color temp daylight
					colorMode = 1;
					setDaylightColor(hour_unconverted);
					//soft refresh without fade in/out
					tmpFadeSetting = fadeInOut;
					fadeInOut = false;
					softRefresh();

					settings.begin("settings", false);
					settings.putInt("colorMode", colorMode);
					settings.end();
				}

				backlightMode = "daylight";
				blNeedRefresh = true;

			}else if(mode.equals("SET_BACKLIGHTMODE_RAINBOW")){
				backlightMode = "rainbow";
				sat = 130;
				val = 255;

			}else if(mode.equals("SET_BACKLIGHTMODE_FREEZE")){
				if(backlightMode.equals("daylight") || backlightMode.equals("rainbow")){
					backlightMode = "manual";

					blNeedRefresh = true;

				}else Log_println("Die Farbe kann nur bei aktiviertem Auto-Daylight oder Rainbow Modus gespeichert werden.",1);

			}else if(mode.indexOf("SET_BACKLIGHTMODE_MANUAL") != -1){
				backlightMode = "manual";

				//split command
				int spaceIndex = mode.indexOf(' ');
				int secondSpaceIndex = mode.indexOf(' ', spaceIndex + 1);
				String colorString = mode.substring(spaceIndex + 1, secondSpaceIndex);

				//split color string input
				int colorIndex = colorString.indexOf(',');
				int secondColorIndex = colorString.indexOf(',', colorIndex + 1);

				String s_red, s_green, s_blue;

				s_red = colorString.substring(0, colorIndex);
				s_green = colorString.substring(colorIndex + 1, secondColorIndex);
				s_blue = colorString.substring(secondColorIndex+1);

				rgbcolorBL[0] = s_red.toInt();
				rgbcolorBL[1] = s_green.toInt();
				rgbcolorBL[2] = s_blue.toInt();

				blNeedRefresh = true;

				//Log_println("Die Farbe fuer das Backlight wurde eingestellt.");

			}

			enableBacklight = true;

			settings.begin("settings", false);
			settings.putString("backlightMode", backlightMode);
			settings.putBool("enableBacklight", enableBacklight);
			settings.putInt("rgbcolorBL_1", rgbcolorBL[0]);
			settings.putInt("rgbcolorBL_2", rgbcolorBL[1]);
			settings.putInt("rgbcolorBL_3", rgbcolorBL[2]);
			settings.end();

			Log_println("Backlightmodus erfolgreich konfiguriert.");
		}

	}else if(mode.equals("TEST_BL")){
		//test backlight
		Log_println("Teste LEDs BL...");
		//fill_solid(backlight,NUM_LEDS_BL,CRGB::Red);
		fill_gradient(leds,NUM_LEDS-NUM_LEDS_BL,CHSV(220,80,255),NUM_LEDS-1,CHSV(220,80,255));
		FastLED.show();
		delay(2000);
		fill_gradient(leds,NUM_LEDS-NUM_LEDS_BL,CHSV(0,0,0),NUM_LEDS-1,CHSV(0,0,0));
		FastLED.show();

	}else if(mode.equals("TEST_BL2")){
		//test backlight
		Log_println("Teste LEDs BL...");
		testBLMatrix(rgbcolor[0], rgbcolor[1], rgbcolor[2]);
	}else if(mode.equals("TEST_POWER")){
		//test power consumption
		Log_println("Stromaufnahmetest wird ausgefuehrt...");
		testPower();
		tmpFadeSetting = fadeInOut;
		softRefresh();
		//	}else if(mode.equals("GOSLEEP")){
		//		//manual toggle standby
		//		if(forceStandby) fadeInBL=true;
		//		forceStandby = !forceStandby;
	}else if(mode.equals("TOGGLE_TIMESYNCSTATE")){
		//turn on/off network time sync functionality
		enableTimeSync = !enableTimeSync;

		settings.begin("settings", false);
		settings.putBool("enableTimeSync", enableTimeSync);
		settings.end();

		if(enableTimeSync) Log_println("Zeitsynchronisation wurde eingeschaltet.");
		else Log_println("Zeitsynchronisation wurde ausgeschaltet.");
	}else if(mode.indexOf("SET_DEVICENAME") != -1){
		//set lightlevel for standby mode
		int spaceIndex = mode.indexOf(' ');
		int secondSpaceIndex = mode.indexOf(' ', spaceIndex + 1);
		String deviceName = mode.substring(spaceIndex + 1, secondSpaceIndex);

		if(!deviceName.equals("")){
			netDevName = deviceName;

			writeConfigToFlash();

			Log_println("Der Geraetename wurde eingestellt auf '" + netDevName + "'. Neustart...");

			//Softrestart
			ESP.restart();
		}

	}else if(mode.indexOf("SET_MINUTES_DISPLAY_MODE") != -1){
		//set mode for minutes display
		int spaceIndex = mode.indexOf(' ');
		int secondSpaceIndex = mode.indexOf(' ', spaceIndex + 1);
		String s_displMode = mode.substring(spaceIndex + 1, secondSpaceIndex);

		displayMinutes = s_displMode.toInt();

		settings.begin("settings", false);
		settings.putInt("displayMinutes", displayMinutes);
		settings.end();

		//refresh BL if minutes display is on
		if(enableBacklight){
			blNeedRefresh = true;
		}else if(displayMinutes>0 && !enableBacklight){
			//if BL is off -> direct control min display
			clearBacklight();
			setMinutesBL(convertMin(current_min_rtc));
			FastLED.show();
		}else clearBacklight();

		if(displayMinutes>0) Log_println("Minutenanzeige wurde eingeschaltet.");
		else Log_println("Minutenanzeige wurde ausgeschaltet.");

	}else if(mode.equals("SET_MINUTES_DISPLAY_COLOR_DAYLIGHT")){
		//set mode for minutes display color to automatic daylight
		colormodeMin = 1;

		settings.begin("settings", false);
		settings.putInt("colormodeMin", colormodeMin);
		settings.end();

		//get color
		setDaylightColorMin(hour_unconverted);

		//refresh BL if minutes display is on
		if(displayMinutes>0 && enableBacklight){
			blNeedRefresh = true;
		}else if(displayMinutes>0 && !enableBacklight){
			//if BL is off -> direct control min display
			clearBacklight();
			setMinutesBL(convertMin(current_min_rtc));
			FastLED.show();
		}else clearBacklight();

		Log_println("Die Minutenanzeige wurde auf autom. Tageslichtfarbe gestellt.");

	}else if(mode.indexOf("SET_MINUTES_DISPLAY_COLOR") != -1){
		//set color of minute display
		//split command
		int spaceIndex = mode.indexOf(' ');
		int secondSpaceIndex = mode.indexOf(' ', spaceIndex + 1);
		String colorString = mode.substring(spaceIndex + 1, secondSpaceIndex);

		//split color string input
		int colorIndex = colorString.indexOf(',');
		int secondColorIndex = colorString.indexOf(',', colorIndex + 1);

		String s_red, s_green, s_blue;

		s_red = colorString.substring(0, colorIndex);
		s_green = colorString.substring(colorIndex + 1, secondColorIndex);
		s_blue = colorString.substring(secondColorIndex+1);

		rgbcolorMin[0] = s_red.toInt();
		rgbcolorMin[1] = s_green.toInt();
		rgbcolorMin[2] = s_blue.toInt();

		colormodeMin = 0;

		//save to Flash
		settings.begin("settings", false);
		settings.putInt("rgbcolorMin_1", rgbcolorMin[0]);
		settings.putInt("rgbcolorMin_2", rgbcolorMin[1]);
		settings.putInt("rgbcolorMin_3", rgbcolorMin[2]);
		settings.putInt("colormodeMin", colormodeMin);
		settings.end();

		//refresh BL if minutes display is on
		if(displayMinutes>0 && enableBacklight){
			blNeedRefresh = true;
		}else if(displayMinutes>0 && !enableBacklight){
			//if BL is off -> direct control min display
			clearBacklight();
			setMinutesBL(convertMin(current_min_rtc));
			FastLED.show();
		}else clearBacklight();

		Log_println("Die Farbe fuer die Minutenanzeige wurde eingestellt (manuell). Farbe: " + colorString);

	}else if(mode.equals("TEST_MIN_EDGE")){
		//test backlight
		Log_println("Teste Minutenanzeige...");
		//setPixel(115,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
		setPixel(116,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
		setPixel(117,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
		//setPixel(118,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
		FastLED.show();
		delay(2000);

		//2 min after
		//setPixel(122,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
		setPixel(123,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
		setPixel(124,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
		//setPixel(125,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
		FastLED.show();
		delay(2000);

		//3 min after
		//setPixel(129,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
		setPixel(130,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
		setPixel(131,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
		//setPixel(132,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
		FastLED.show();
		delay(2000);

		//4 min after
		//setPixel(110,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
		setPixel(111,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
		setPixel(136,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
		//setPixel(137,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
		FastLED.show();
		delay(5000);
		clearBacklight();

	}else if(mode.equals("TEST_MIN_MIDDLE")){
		//test backlight
		Log_println("Teste Minutenanzeige...");
		setPixel(112,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
		setPixel(113,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
		setPixel(114,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
		FastLED.show();
		delay(2000);
		//2 min after
		setPixel(119,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
		setPixel(120,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
		setPixel(121,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
		FastLED.show();
		delay(2000);
		//3 min after
		setPixel(126,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
		setPixel(127,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
		setPixel(128,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
		FastLED.show();
		delay(2000);
		//4 min after
		setPixel(133,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
		setPixel(134,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
		setPixel(135,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);

		FastLED.show();
		delay(5000);
		clearBacklight();

		}
	}

	


	mode = "";
}

void controlBacklight(){
	//control backlight
	//if(enableBacklight && !firstCycle && !forceStandby && !standby && isInStandby && !blockBacklight){
	if(enableBacklight && !forceStandby && !standby && !isInStandby && !blockBacklight){
		if(backlightMode.equals("daylight") && blNeedRefresh){
			CRGB rgb_val = CRGB(rgbcolor[0],rgbcolor[1],rgbcolor[2]);
			CHSV hsv_val = rgb2hsv_approximate(rgb_val);

			//store hsv values
			hue_actual = hsv_val.h;
			sat = hsv_val.s;
			val = hsv_val.v;

			//store rgb values
			rgbcolorBL[0] = rgbcolor[0];
			rgbcolorBL[1] = rgbcolor[1];
			rgbcolorBL[2] = rgbcolor[2];

			if(!firstCycle){
				fill_gradient(leds,NUM_LEDS-NUM_LEDS_BL,hsv_val,NUM_LEDS-1,hsv_val);
				if(displayMinutes>0) setMinutesBL(convertMin(current_min_rtc));
				FastLED.show();
			}

			blNeedRefresh = false;

		}else if(backlightMode.equals("rainbow")){
			currentMillisBLTimer = millis();
			if((unsigned long)(currentMillisBLTimer - previousMillisBLTimer) > 500){

				if(!firstCycle){
					fill_gradient(leds,NUM_LEDS-NUM_LEDS_BL,CHSV(hue_actual,sat,val),NUM_LEDS-1,CHSV(hue_actual,sat,val));
					if(displayMinutes>0) setMinutesBL(convertMin(current_min_rtc));
					FastLED.show();
				}

				if(hue_actual <= 255) hue_actual++;
				else hue_actual--;

				//convert and store rgb values
				CRGB rgb_val;
				hsv2rgb_rainbow(CHSV(hue_actual,sat,val), rgb_val);
				rgbcolorBL[0] = rgb_val.r;
				rgbcolorBL[1] = rgb_val.g;
				rgbcolorBL[2] = rgb_val.b;

				previousMillisBLTimer = currentMillisBLTimer;
			}
		}else if(backlightMode.equals("manual") && blNeedRefresh){
			CRGB rgb_val = CRGB(rgbcolorBL[0],rgbcolorBL[1],rgbcolorBL[2]);
			CHSV hsv_val = rgb2hsv_approximate(rgb_val);

			//store hsv values
			hue_actual = hsv_val.h;
			sat = hsv_val.s;
			val = hsv_val.v;

			if(!firstCycle){
				fill_gradient(leds,NUM_LEDS-NUM_LEDS_BL,hsv_val,NUM_LEDS-1,hsv_val);
				if(displayMinutes>0) setMinutesBL(convertMin(current_min_rtc));
				FastLED.show();
			}

			blNeedRefresh = false;

		}

	}
}

void setDefaults(){
	standbyMode = "TWINKLE";
	lightlevel = 10;

	enableTouch = false;

	debugmode = false;
	debugmatrix = false;

	rgbcolor[0] = 201;
	rgbcolor[1] = 226;
	rgbcolor[2] = 255;
	rgbcolorSec1[0] = 201;
	rgbcolorSec1[1] = 226;
	rgbcolorSec1[2] = 255;
	rgbcolorSec3[0] = 201;
	rgbcolorSec3[1] = 226;
	rgbcolorSec3[2] = 255;
	rgbcolorMin[0] = 255;
	rgbcolorMin[1] = 0;
	rgbcolorMin[2] = 0;
	brightness = 40;
	autoBrightness = true;
	enableAutoStandby = true;
	fadeInOut = true;
	colorSec1 = false;
	colorSec3 = false;
	colorMode = 1;
	setDaylightColor(hour_unconverted);

	clockLang = 0;

	activateSec1 = true;
	activateSec3 = true;

//	wifiSSID = "none";
//	wifiPassword = "none";

	netDevName = "wordclock";

	enableBacklight = false;
	backlightMode = "daylight";

	displayMinutes = 0;
	colormodeMin = 0;

	isSummertime = true;

	enableTimeSync = true;

	writeConfigToFlash();
}

String getWiFiKey(bool keyTypeShort){
	uint8_t baseMac[6];
	//Get MAC address for WiFi chipset/HWID
	esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
	char baseMacChr[18] = {0};
	//sprintf(baseMacChr, "%02X:%02X:%02X:%02X:%02X:%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
	if(keyTypeShort){
		sprintf(baseMacChr, "%02X%02X", baseMac[0], baseMac[5]);
		return baseMacChr;
	}else{
		sprintf(baseMacChr, "%02X%02X%02X%02X%02X%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
		return baseMacChr;
	}
}

void wifiStartAPmode(){
	//setup first time
	//WiFi.onEvent(WiFiEvent);
	WiFi.mode(WIFI_MODE_APSTA);
	//WiFi.softAP(AP_SSID, getWiFiKey(false).c_str());
	WiFi.softAP(wifi_config_SSID.c_str());

	//Set DNS
	if (!MDNS.begin("wordclock-config")) {
		Log_println("Error setting up MDNS service.",2);
		delay(500);
	}else MDNS.addService("http", "tcp", 80);

	server.begin();

	Log_println("AP started");
	Log_println("AP SSID: " + wifi_config_SSID);
	Log_println("AP IPv4: " + String(WiFi.softAPIP()));
}


void wifiConnect(){
	if(wifiConfigMode){
		//not configured or new config

		Log_println("WiFi configuration, starting AP mode...");

		wifiStartAPmode();

		//loop in server mode for 120 seconds

		unsigned long currentMillisAPMode = millis();
		unsigned long previousMillisAPMode = currentMillisAPMode;
		while((unsigned long)(currentMillisAPMode - previousMillisAPMode) <= 300000){
			wifiAPClientHandle();
			//if(WiFi.status() == WL_CONNECTED) break;
			currentMillisAPMode = millis();
		}
		if(WiFi.status() != WL_CONNECTED){
			//Auskommentierung wieder entfernen!
			wifiActive = false;
			WiFi.mode(WIFI_OFF);

			clearLedPanel();

			Log_println("Not connected, disabling WiFi and continue booting.",1);

			//Das hier nur im Debug
			//wifiConnect();
		}

	}
	else{
		//if credentials exist, try to connect

		WiFi.mode(WIFI_MODE_STA);
		//WiFi.setHostname("Wordclock");
		WiFi.setHostname(netDevName.c_str());
		WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

		//pruefen, ob verbindung vorhanden ist
		unsigned long currentMillisConnect = millis();
		unsigned long previousMillisConnect = currentMillisConnect;
		while ((unsigned long)(currentMillisConnect - previousMillisConnect) <= 10000){
			if(WiFi.status() == WL_CONNECTED) break;
			//Wait for connection being established...
			delay(500);
			Serial.print(".");
			currentMillisConnect = millis();
		}
		if(WiFi.status() != WL_CONNECTED){
			//If connection fails -> Shut down Wifi
			wifiActive = false;
			WiFi.mode(WIFI_OFF);

			//Flash LEDs red if connection failed
			clearLedPanel();
			delay(400);
			setPixel(52,220,20,20);
			setPixel(35,220,20,20);
			setPixel(55,220,20,20);
			setPixel(54,220,20,20);
			showStrip();
			delay(400);
			clearLedPanel();
			delay(400);
			setPixel(52,220,20,20);
			setPixel(35,220,20,20);
			setPixel(55,220,20,20);
			setPixel(54,220,20,20);
			showStrip();
			delay(400);
			clearLedPanel();
			delay(400);
			setPixel(52,220,20,20);
			setPixel(35,220,20,20);
			setPixel(55,220,20,20);
			setPixel(54,220,20,20);
			showStrip();
			delay(400);
			clearLedPanel();
			delay(400);

			Log_println("Cannot connect to local WiFi. :(",2);
		}
		else{

			//Set DNS
			if (!MDNS.begin(netDevName.c_str())) {
				Log_println("Error setting up MDNS service.");
				delay(500);
			}else MDNS.addService("http", "tcp", 80);

			server.begin();

			wifiConnected = true;

			//Flash LEDs green if connected successful
			clearLedPanel();
			delay(400);
			setPixel(52,20,220,20);
			setPixel(35,20,220,20);
			setPixel(55,20,220,20);
			setPixel(54,20,220,20);
			showStrip();
			delay(400);
			clearLedPanel();
			delay(400);
			setPixel(52,20,220,20);
			setPixel(35,20,220,20);
			setPixel(55,20,220,20);
			setPixel(54,20,220,20);
			showStrip();
			delay(400);
			clearLedPanel();
			delay(400);
			setPixel(52,20,220,20);
			setPixel(35,20,220,20);
			setPixel(55,20,220,20);
			setPixel(54,20,220,20);
			showStrip();
			delay(400);
			clearLedPanel();
			delay(400);

			Log_println("The Wordclock is connected to the WiFi! :)");
			Log_println("IP: " + WiFi.localIP().toString());
			deviceIP = WiFi.localIP().toString();
		}

	}
}


void wifiAPClientHandle()
{
	WiFiClient client = server.available();   // listen for incoming clients

	if (client) {                          // if you get a client,
		//Log_println("Client connected");
		String currentLine = "";                // make a String to hold incoming data from the client
		while (client.connected()) {            // loop while the client's connected
			if (client.available()) {             // if there's bytes to read from the client,
				char c = client.read();             // read a byte, then
				//Serial.write(c);                    // print it out the serial monitor
				if (c == '\n') {                    // if the byte is a newline character

					// if the current line is blank, you got two newline characters in a row.
					// that's the end of the client HTTP request, so send a response:
					if (currentLine.length() == 0) {
						// HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
						// and a content-type so the client knows what's coming, then a blank line:
						// Display the HTML web page
						client.println("<!DOCTYPE html><html>");
						client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
						client.println("<link rel=\"icon\" href=\"data:,\">");
						// CSS to style the on/off buttons
						// Feel free to change the background-color and font-size attributes to fit your preferences
						client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: left;}");
						client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
						client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
						client.println(".button2 {background-color: #555555;}");
						client.println("input[type=submit].link {background-color: #4CAF50; border: none; color: white; padding: 16px 20px;");
						client.println("text-decoration: none; font-size: 18px; margin: 2px; cursor: pointer;}");
						client.println("</style></head>");

						// the content of the HTTP response follows the header:
						client.print("<h2>Wordclock mit WiFi Netz verbinden</h2>");
						client.print("Verbindungsdaten des Routers eingeben:<br><br>");
						client.print("<form method='get' action='a'>");
						client.print("<label>WiFi Name (SSID): </label><input name='ssid' length=32><br><br>");
						client.print("<label>Kennwort (WPA Key): </label><input name='pass' length=64><br><br><br>");
						client.print("<input type='submit' class='link' value='Senden & Wordclock neu starten'>");
						client.print("</form>");

						// The HTTP response ends with another blank line:
						client.println();
						// break out of the while loop:
						break;
					} else {    // if you got a newline, then clear currentLine:
						currentLine = "";
					}
				} else if (c != '\r') {  // if you got anything else but a carriage return character,
					currentLine += c;      // add it to the end of the currentLine
					continue;
				}

				if (currentLine.startsWith("GET /a?ssid=") ) {
					//Expecting something like:
					//GET /a?ssid=blahhhh&pass=poooo

					String qsid;
					qsid = currentLine.substring(12, currentLine.indexOf('&')); //parse ssid
					String qpass;
					qpass = currentLine.substring(currentLine.lastIndexOf('=') + 1, currentLine.lastIndexOf(' ')); //parse password

					settings.begin("wifi", false); // Note: Namespace name is limited to 15 chars
					settings.putString("ssid", qsid);
					//nur Debug
					//wifiSSID = qsid;
					//wifiPassword = qpass;

					settings.putString("password", qpass);
					delay(300);
					settings.end();

					client.println("HTTP/1.1 200 OK");
					client.println("Content-type:text/html");
					client.println();

					// the content of the HTTP response follows the header:
					client.print("<h1>OK! Restarting in 5 seconds...</h1>");
					Log_println("Got SSID and Key, restarting clock...");
					client.println();
					delay(5000);
					ESP.restart();
				}
			}else{
				client.stop();
				break;
			}
		}
	}
}

double getWifiSignalStrength(){
	//return map(WiFi.RSSI(), -100, 0, 0, 100);

	//Convert rssi (dBm to percent)
	int rssi = WiFi.RSSI();
	double signalQuality = 0;

	if(rssi <= -100)
		signalQuality = 0;
	else if(rssi >= -50)
		signalQuality = 100;
	else
		signalQuality = 2 * (rssi + 100);

	return signalQuality;
}

void wifiConnectedHandle(WiFiClient client){
	//Log_println("New Client.");
	String currentLine = "";
	bool execControlSwitch = false;
	bool refreshPage = false;
	String waitRefresh = "3";

	while (client.connected()) {
		if (client.available()) {
			char c = client.read();
			//Serial.write(c);
			header += c;
			if (c == '\n') {
				// if the current line is blank, you got two newline characters in a row.
				// that's the end of the client HTTP request, so send a response:
				if (currentLine.length() == 0) {
					// HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
					// and a content-type so the client knows what's coming, then a blank line:
					client.println("HTTP/1.1 200 OK");
					client.println("Content-type:text/html");
					client.println("Connection: close");
					client.println();


					// handle the wordclock function based on header data
					if(header.indexOf("GET /toggle_backlight") >= 0) {
						//Backlight last mode on / off
						if(enableBacklight) mode = "SET_BACKLIGHT_OFF";
						else mode = "SET_BACKLIGHT_ON";
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.indexOf("GET /set_backlight_rainbow") >= 0) {
						//Set Backlight Rainbow mode
						mode = "SET_BACKLIGHTMODE_RAINBOW";
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.indexOf("GET /set_backlight_daylight") >= 0) {
						//Set Backlight auto daylight mode
						mode = "SET_BACKLIGHTMODE_DAYLIGHT";
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.indexOf("GET /set_backlight_freeze") >= 0) {
						//Set Backlight actual color
						mode = "SET_BACKLIGHTMODE_FREEZE";
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.indexOf("GET /go_sleep") >= 0) {
						//clock to standby mode
						//mode = "GOSLEEP";
						//manual toggle standby
						if(forceStandby) fadeInBL=true;
						forceStandby = true;
						refreshPage = true;
						Log_println("Wechsel in Modus Standby (manuell).");
					}else if (header.indexOf("GET /go_wake") >= 0) {
						//clock to standby mode
						mode = "GOWAKE";
						forceStandby = false;
						refreshPage = true;
						Log_println("Wechsel in Modus Betrieb (manuell).");
					}else if (header.indexOf("GET /toggle_sec1") >= 0) {
						//Toggle Es ist
						mode = "TOGGLE_SEC1";
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.indexOf("GET /toggle_sec3") >= 0) {
						//Toggle Uhr
						mode = "TOGGLE_SEC3";
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.indexOf("GET /toggle_auto_standby") >= 0) {
						//Toggle automatic standby
						mode = "TOGGLE_AUTO_STANDBY";
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.indexOf("GET /toggle_touch") >= 0) {
						//Toggle touch button
						mode = "TOGGLE_SOFTTOUCH";
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.indexOf("GET /toggle_timesync") >= 0) {
						//Toggle time sync setting
						mode = "TOGGLE_TIMESYNCSTATE";
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.indexOf("GET /set_standby_jus") >= 0) {
						//Set standby mode JUS
						mode = "SET_STANDBY_JUS";
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.indexOf("GET /set_standby_che") >= 0) {
						//Set standby mode JCHE
						mode = "SET_STANDBY_CHE";
						execControlSwitch = true;
						refreshPage = true;

					//}//else if (header.indexOf("GET /set_standby_xmastree") >= 0) {
						//Set standby mode XMASTREE
						//mode = "SET_STANDBY_XMASTREE";
						//execControlSwitch = true;

					}else if (header.indexOf("GET /set_standby_twinkle") >= 0) {
						//Set standby mode Twinkle
						mode = "SET_STANDBY_TWINKLE";
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.indexOf("GET /set_displ_min_off") >= 0) {
						//Set min display off
						mode = "SET_MINUTES_DISPLAY_MODE 0";
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.indexOf("GET /set_displ_min_1") >= 0) {
						//Set min display middle
						mode = "SET_MINUTES_DISPLAY_MODE 1";
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.indexOf("GET /set_displ_min_2") >= 0) {
						//Set min display edge
						mode = "SET_MINUTES_DISPLAY_MODE 2";
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.indexOf("GET /set_displ_min_3") >= 0) {
						//Set min display middle one side
						mode = "SET_MINUTES_DISPLAY_MODE 3";
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.indexOf("GET /set_displ_min_4") >= 0) {
						//Set min display edge one side
						mode = "SET_MINUTES_DISPLAY_MODE 4";
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.indexOf("GET /set_colormode_min_daylight") >= 0) {
						//Set min display color to automatic daylight
						mode = "SET_MINUTES_DISPLAY_COLOR_DAYLIGHT";
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.indexOf("GET /presentation_xmastree") >= 0) {
						//clock to presentation mode
						//manual toggle standby
						if(forceStandby) fadeInBL=true;
						forceStandby = true;

						isInPresentationMode = true;
						standbyModeSaved = standbyMode;
						standbyMode = "XMASTREE";
						refreshPage = true;

					}else if (header.indexOf("GET /settings") >= 0) {
						// Display the HTML web page
						client.println("<!DOCTYPE html><html>");
						client.println("<head><meta charset=\"utf-8\" name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
						client.println("<link rel=\"icon\" href=\"data:,\">");
						// CSS to style the on/off buttons
						// Feel free to change the background-color and font-size attributes to fit your preferences
						client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
						client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
						client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
						client.println(".button2 {background-color: #555555;}");
						client.println("fieldset {margin: 8px; border: 1px solid silver; padding: 8px; border-radius: 4px;}");
						client.println("legend {padding: 2px;}");
						client.println("input[type=text].send {font-size:20px;}");
						client.println(".button3 {font-size: 20px;}");
						client.println(".button4 {font-size: 20px; background-color: #555555;}</style></head>");

						// Web Page Heading
						client.println("<body><h1>Weitere Einstellungen</h1>");


						client.println("<p><a href=\"http://" + deviceIP + "/\"><button class=\"button button4\">Zurück zum Hauptmenü</button></a></p>");

						// Change color front
						client.println("<fieldset>");
						client.println("<legend>Farbe Uhrzeitanzeige</legend>");
						//Auto daylight color
						if(colorMode == 1) client.println("<p><a href=\"/set_color_auto_daylight\"><button class=\"button button3\">Tageszeitabhängige Farbe</button></a></p>");
						else client.println("<p><a href=\"/set_color_auto_daylight\"><button class=\"button button4\">Tageszeitabhängige Farbe</button></a></p>");

						client.println("<p>Farbe eingeben:</p>");

						client.print("<form method='get' action='a'>");
						client.print("<input name='color' length=11>&nbsp;");
						client.print("<input type='submit' class='send' value='OK'><br><br><b>Format:</b><br>RGB-Wert => RRR,GGG,BBB<br>Farbtemp. in Kelvin => Zahl<br>");
						client.print("</form>");

						client.println("</fieldset><br>");

						// Brightness settings
						client.println("<fieldset>");
						client.println("<legend>Helligkeit einstellen</legend>");
						if(autoBrightness) client.println("<p><a href=\"/set_brightness_auto\"><button class=\"button button3\">Automatisch anpassen</button></a></p>");
						else client.println("<p><a href=\"/set_brightness_auto\"><button class=\"button button4\">Automatisch anpassen</button></a></p>");

						client.println("<p>Wert eingeben:</p>");

						client.print("<form method='get' action='a'>");
						client.print("<input name='brightness' length=11>&nbsp;");
						client.print("<input type='submit' class='send' value='OK'><br><br>Wert zwischen 5% und 100%");
						client.print("</form>");

						client.println("</fieldset><br>");

						// Change color backlight
						client.println("<fieldset>");
						client.println("<legend>Benutzerdefinierte Farbe für Backlight</legend>");

						client.println("<p>Farbe eingeben:</p>");

						client.print("<form method='get' action='a'>");
						client.print("<input name='color_bl' length=11>&nbsp;");
						client.print("<input type='submit' class='send' value='OK'><br><br><b>Format:</b><br>RGB-Wert -> RRR,GGG,BBB");
						client.print("</form>");
						client.println("</fieldset><br>");

						// Display minutes with backlight
						if(NUM_LEDS_BL==28){
							client.println("<fieldset>");
							client.println("<legend>Minuten mit Backlight anzeigen</legend>");

							client.println("<p>Minutenanzeige konfigurieren:</p>");
							//Buttons anpassen
							if(displayMinutes==0) client.println("<p><a href=\"/settings\"><button class=\"button button3\">Aus</button></a>&nbsp;");
							else client.println("<p><a href=\"/set_displ_min_off\"><button class=\"button button4\">Aus</button></a>&nbsp;");

							if(displayMinutes==1) client.println("<a href=\"/settings\"><button class=\"button button3\">Mitte</button></a></p>");
							else client.println("<a href=\"/set_displ_min_1\"><button class=\"button button4\">Mitte</button></a></p>");

							if(displayMinutes==2) client.println("<p><a href=\"/settings\"><button class=\"button button3\">Ecke</button></a>&nbsp;");
							else client.println("<p><a href=\"/set_displ_min_2\"><button class=\"button button4\">Ecke</button></a>&nbsp;");

							if(displayMinutes==3) client.println("<a href=\"/settings\"><button class=\"button button3\">Mitte2</button></a></p>");
							else client.println("<a href=\"/set_displ_min_3\"><button class=\"button button4\">Mitte2</button></a></p>");

							if(displayMinutes==4) client.println("<p><a href=\"/settings\"><button class=\"button button3\">Ecke2</button></a>&nbsp;");
							else client.println("<p><a href=\"/set_displ_min_4\"><button class=\"button button4\">Ecke2</button></a>&nbsp;");

							client.print("<br><br>Zu Mitte2/Ecke2: Es leuchtet pro Minute immer nur eine Seite.");

							client.println("<p>Farbe eingeben:</p>");

							client.print("<form method='get' action='a'>");
							client.print("<input name='color_min' length=11>&nbsp;");
							client.print("<input type='submit' class='send' value='OK'><br><br><b>Format:</b><br>RGB-Wert -> RRR,GGG,BBB");
							client.print("</form>");

							//Button minutes color daylight
							if(displayMinutes && colormodeMin==1) client.println("<p><a href=\"/settings\"><button class=\"button button3\">Minutenanzeige Tageslichtfarbe</button></a></p>");
							else client.println("<p><a href=\"/set_colormode_min_daylight\"><button class=\"button button4\">Minutenanzeige Tageslichtfarbe</button></a></p>");

							client.println("</fieldset><br>");
						}


						// Set time
						client.println("<fieldset>");
						client.println("<legend>Zeit einstellen</legend>");

						if(isSummertime) client.println("<p><a href=\"/toggle_summertime\"><button class=\"button button4\">Auf Winterzeit umstellen</button></a></p>");
						else client.println("<p><a href=\"/toggle_summertime\"><button class=\"button button4\">Auf Sommerzeit umstellen</button></a></p>");

						client.println("<p>Datum und Zeit eingeben:</p>");

						client.print("<form method='get' action='a'>");
						client.print("<input name='time' length=32>&nbsp;");
						client.print("<input type='submit' class='send' value='OK'><br><br><b>Format:</b><br>jjjj-mm-dd-hh-mm-ss");
						client.print("</form>");
						client.println("</fieldset>");

						//Toggle auto Standby
						client.println("<br><fieldset>");
						client.println("<legend>Standby-Modus konfigurieren</legend>");

						client.println("<p>Lichtabhängiger Standby:</p>");
						if(enableAutoStandby) client.println("<p><a href=\"/toggle_auto_standby\"><button class=\"button button3\">Auto-Standby ausschalten</button></a></p>");
						else client.println("<p><a href=\"/toggle_auto_standby\"><button class=\"button button4\">Auto-Standby einschalten</button></a></p>");

						//Set lightlevel to actual level
						client.println("<p>Helligkeitsgrenze auf momentane Umgebungshelligkeit setzen:</p>");
						client.println("<p><a href=\"/set_lightlevel_act\"><button class=\"button button4\">Festlegen</button></a>");
						client.println("<br>Aktueller Wert -> " + (String)getLightSensorValue() + "</p>");

						client.println("</fieldset>");

						//Misc functions
						client.println("<br><fieldset>");
						client.println("<legend>Weitere Funktionen</legend>");

						//Toggle Softtouch
						client.println("<p>Touch-Button:</p>");
						if(enableTouch) client.println("<p><a href=\"/toggle_touch\"><button class=\"button button3\">Touch-Button ausschalten</button></a></p>");
						else client.println("<p><a href=\"/toggle_touch\"><button class=\"button button4\">Touch-Button einschalten</button></a></p>");

						//Toggle time sync state
						client.println("<p>Automatische Zeitsynchronisation:</p>");
						if(enableTimeSync) client.println("<p><a href=\"/toggle_timesync\"><button class=\"button button3\">Zeitsync. ausschalten</button></a></p>");
						else client.println("<p><a href=\"/toggle_timesync\"><button class=\"button button4\">Zeitsync. einschalten</button></a></p>");

						//Send recovery link
						client.println("<br><p><a href=\"/get_recoverylink\" target=\"_blank\"><button class=\"button button4\">Recovery-Link erzeugen</button></a></p>");

						//Show debug information
						client.println("<p><a href=\"/get_debugdata\" target=\"_blank\"><button class=\"button button4\">Statusinfos anzeigen</button></a></p>");

						client.println("</fieldset>");

						client.println("<br><br><br>");

						client.println("</body></html>");

						// The HTTP response ends with another blank line
						client.println();
						break;

					}else if (header.startsWith("GET /a?color=")) {
						String color,tmp;
						tmp = header.substring(0,header.indexOf("\n"));
						color = tmp.substring(13, tmp.lastIndexOf(' '));

						color.replace("%2C",",");

						if (color.indexOf(",") >= 0){
							//If RGB value input
							mode = "RGB_ALL " + color;
						}
						else{
							//If Kelvin value input
							mode = "SET_COLORTEMP " + color;
						}
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.startsWith("GET /a?color_bl=")) {
						String color,tmp;
						tmp = header.substring(0,header.indexOf("\n"));
						color = tmp.substring(16, tmp.lastIndexOf(' '));

						color.replace("%2C",",");

						if (color.indexOf(",") >= 0){
							//If RGB value input
							mode = "SET_BACKLIGHTMODE_MANUAL " + color;
						}
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.startsWith("GET /a?color_min=")) {
						String color,tmp;
						tmp = header.substring(0,header.indexOf("\n"));
						color = tmp.substring(17, tmp.lastIndexOf(' '));

						color.replace("%2C",",");

						if (color.indexOf(",") >= 0){
							//If RGB value input
							mode = "SET_MINUTES_DISPLAY_COLOR " + color;
						}
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.startsWith("GET /a?time=")) {
						String time,tmp;
						tmp = header.substring(0,header.indexOf("\n"));
						time = tmp.substring(12, tmp.lastIndexOf(' '));

						time.replace("%2D","-");

						if (time.indexOf("-") >= 0){
							//If RGB value input
							mode = "SET_TIME " + time;
						}
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.indexOf("GET /set_lightlevel_act") >= 0) {
						//Set standby mode off
						mode = "SET_LIGHTLEVEL_ACTUAL";
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.indexOf("GET /set_color_auto_daylight") >= 0) {
						//Set daylight color on
						mode = "SET_COLORMODE_AUTO_DAYLIGHT";
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.indexOf("GET /set_brightness_auto") >= 0) {
						//Set brightness automatically adjusted
						mode = "SET_BRIGHTNESS_AUTO";
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.startsWith("GET /a?brightness=")) {
						String brightness,tmp;
						tmp = header.substring(0,header.indexOf("\n"));
						brightness = tmp.substring(18, tmp.lastIndexOf(' '));

						brightness = (String)(map(brightness.toInt(),5,100,40,255));

						mode = "SET_BRIGHTNESS " + brightness;
						//Log_println(mode);

						execControlSwitch = true;
						refreshPage = true;

					}else if (header.startsWith("GET /a?set_devicename=")) {
						String deviceName,tmp;
						tmp = header.substring(0,header.indexOf("\n"));
						deviceName = tmp.substring(22, tmp.lastIndexOf(' '));

						mode = "SET_DEVICENAME " + deviceName;

						execControlSwitch = true;
						refreshPage = true;

					}else if (header.indexOf("GET /get_debugdata") >= 0) {
						//Set daylight color on
						String confString = getConfig();

						// Display the HTML web page
						client.println("<!DOCTYPE html><html>");
						client.println("<head><meta charset=\"utf-8\" name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
						client.println("<link rel=\"icon\" href=\"data:,\">");
						// CSS to style the on/off buttons
						// Feel free to change the background-color and font-size attributes to fit your preferences
						client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: left;}");
						client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
						client.println("text-decoration: none; font-size: 25px; margin: 2px; cursor: pointer;}");
						client.println(".button2 {background-color: #555555;}");
						client.println(".button3 {font-size: 20px;}");
						client.println(".button4 {font-size: 20px; background-color: #555555;}</style></head>");

						// Web Page Heading
						client.println("<body><h1>Statusinformationen</h1>");


						// Display current state of wordclock
						client.println("<p>Folgende Werte konnten ausgelesen werden:</p>");
						client.println("<p>" + getDebugData() + "</p>");

						client.println("<br><br><br><br><br>");

						client.println("</body></html>");
						client.println();

						break;

					}else if (header.indexOf("GET /presentation") >= 0) {
						//Set daylight color on
						String confString = getConfig();

						// Display the HTML web page
						client.println("<!DOCTYPE html><html>");
						client.println("<head><meta charset=\"utf-8\" name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
						client.println("<link rel=\"icon\" href=\"data:,\">");
						// CSS to style the on/off buttons
						// Feel free to change the background-color and font-size attributes to fit your preferences
						client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: left;}");
						client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
						client.println("text-decoration: none; font-size: 25px; margin: 2px; cursor: pointer;}");
						client.println(".button2 {background-color: #555555;}");
						client.println(".button3 {font-size: 20px;}");
						client.println(".button4 {font-size: 20px; background-color: #555555;}</style></head>");

						// Web Page Heading
						client.println("<body><h1>Grafik anzeigen</h1>");

						client.println("<p><a href=\"http://" + deviceIP + "/\"><button class=\"button button4\">Zurück zum Hauptmenü</button></a></p>");

						// Display current state of wordclock
						client.println("<p>Anzeige wählen:</p>");
						client.println("<p><a href=\"/presentation_xmastree\"><button class=\"button button3\">Weihnachtsbaum</button></a></p><br>");
						//Template
						//client.println("<p><a href=\"/presentation_xyz\"><button class=\"button button4\">Neuer Modus</button></a></p><br>");

						client.println("<br><br><br><br><br>");

						client.println("</body></html>");
						client.println();

						break;

					}else if (header.indexOf("GET /get_recoverylink") >= 0) {
						//Set daylight color on
						String confString = getConfig();

						// Display the HTML web page
						client.println("<!DOCTYPE html><html>");
						client.println("<head><meta charset=\"utf-8\" name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
						client.println("<link rel=\"icon\" href=\"data:,\">");
						// CSS to style the on/off buttons
						// Feel free to change the background-color and font-size attributes to fit your preferences
						client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: left;}");
						client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
						client.println("text-decoration: none; font-size: 25px; margin: 2px; cursor: pointer;}");
						client.println(".button2 {background-color: #555555;}");
						client.println(".button3 {font-size: 20px;}");
						client.println(".button4 {font-size: 20px; background-color: #555555;}</style></head>");

						// Web Page Heading
						client.println("<body><h1>Wiederherstellungslink</h1>");


						// Display current state of wordclock
						client.println("<p>Der Wiederherstellungslink für die aktuelle Konfiguration ist:</p>");
						client.println("<p>http://wordclock.local/a?recovery=" + confString + "</p><br>");
						client.println("<p>Hinweis: WiFi Konfigurationsdaten werden nicht übertragen und müssen nach der Wiederherstellung erneut eingegeben werden.</p>");

						client.println("<br><br><br><br><br>");

						client.println("</body></html>");
						client.println();

						break;

					}else if (header.startsWith("GET /a?recovery=")) {
						String confString,tmp;
						tmp = header.substring(0,header.indexOf("\n"));
						confString = tmp.substring(16, tmp.lastIndexOf(' '));

						confString.replace("%2C",",");

						mode = "IMPORT_CONFIG " + confString;

						execControlSwitch = true;

					}else if (header.indexOf("GET /toggle_summertime") >= 0) {
						//Set standby mode off
						mode = "TOGGLE_SUMMERTIME";
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.indexOf("GET /toggle_summertime_marker") >= 0) {
						//Set standby mode off
						mode = "TOGGLE_SUMMERTIME_MARKER";
						controlSwitch();

						// Display the HTML web page
						client.println("<!DOCTYPE html><html>");
						client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
						client.println("<link rel=\"icon\" href=\"data:,\">");
						if(isSummertime) client.println("Marker auf Sommerzeit umgestellt.");
						else client.println("Marker auf Winterzeit umgestellt.");
						client.println("</body></html>");

						// The HTTP response ends with another blank line
						client.println();

						break;
					}

					if(execControlSwitch) controlSwitch();
					execControlSwitch = false;


					// Display the HTML web page
					client.println("<!DOCTYPE html><html>");
					client.println("<head>");
					client.println("<meta charset=\"utf-8\" name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
					if(refreshPage) client.println("<meta http-equiv=\"refresh\" content=\"" + waitRefresh + ";url=http://" + deviceIP + "/\" />");
					refreshPage = false;
					client.println("<link rel=\"icon\" href=\"data:,\">");
					// CSS to style the on/off buttons
					// Feel free to change the background-color and font-size attributes to fit your preferences
					client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
					client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
					client.println("text-decoration: none; font-size: 25px; margin: 2px; cursor: pointer;}");
					client.println(".button2 {background-color: #555555;}");
					client.println(".button3 {font-size: 20px;}");
					client.println(".striped-border {border: 1px dashed #000; width: 100%; margin: auto; margin-top: 5%; margin-bottom: 5%;}");
					client.println(".button4 {font-size: 20px; background-color: #555555;}</style></head>");

					// Web Page Heading
					client.println("<body><h1>Wordclock<br>Konfiguration</h1>");


					// Display current state of wordclock
					client.println("<p>Status Wordclock: " + (String) ((!forceStandby) ? "Eingeschaltet" : "Standby") + "</p>");
					// If the output26State is off, it displays the ON button
					if (!forceStandby) {
						client.println("<p><a href=\"/go_sleep\"><button class=\"button\">In Standby wechseln</button></a></p>");
					} else {
						client.println("<p><a href=\"/go_wake\"><button class=\"button button2\">Einschalten</button></a></p>");
					}



					// Display current state of backlight
					client.println("<p>Status Backlight: " + (String) ((enableBacklight) ? "Eingeschaltet" : "Ausgeschaltet") + "</p>");
					// If the output26State is off, it displays the ON button
					if (!enableBacklight) {
						client.println("<p><a href=\"/toggle_backlight\"><button class=\"button button2\">Einschalten</button></a></p>");
					} else {
						client.println("<p><a href=\"/toggle_backlight\"><button class=\"button\">Ausschalten</button></a></p><br>");
					}

					//client.println("<hr class=\"striped-border\" />");
					//client.println("---------------------------------------------------------------------------------");

					// Display current mode of backlight
					client.println("<p>Backlight Modus einstellen:</p>");

					// Display buttons for BL mode change
					if(backlightMode.equals("rainbow") && enableBacklight) client.println("<p><a href=\"/set_backlight_rainbow\"><button class=\"button\">Farbwechsel</button></a></p>");
					else client.println("<p><a href=\"/set_backlight_rainbow\"><button class=\"button button2\">Farbwechsel</button></a></p>");

					if(backlightMode.equals("daylight") && enableBacklight) client.println("<p><a href=\"/set_backlight_daylight\"><button class=\"button\">Tageslichtabh. Farbe</button></a></p>");
					else client.println("<p><a href=\"/set_backlight_daylight\"><button class=\"button button2\">Tageslichtabh. Farbe</button></a></p>");

					client.println("<p><a href=\"/set_backlight_freeze\"><button class=\"button button2\">Aktuelle Farbe verwenden</button></a></p>");

					client.println("<p>'Es ist' und 'Uhr' einstellen:</p>");
					if(activateSec1) client.println("<p><a href=\"/toggle_sec1\"><button class=\"button\">'Es ist' ausschalten</button></a></p>");
					else client.println("<p><a href=\"/toggle_sec1\"><button class=\"button button2\">'Es ist' einschalten</button></a></p>");

					if(activateSec3) client.println("<p><a href=\"/toggle_sec3\"><button class=\"button\">'Uhr' ausschalten</button></a></p>");
					else client.println("<p><a href=\"/toggle_sec3\"><button class=\"button button2\">'Uhr' einschalten</button></a></p>");


					//Change Standby mode
					client.println("<p>Anzeige im Standby-Modus:</p>");
					if(standbyMode.equals("JUS")) client.println("<p><a href=\"/set_standby_jus\"><button class=\"button button3\">JUS</button></a>&nbsp;");
					else client.println("<p><a href=\"/set_standby_jus\"><button class=\"button button4\">JUS</button></a>&nbsp;");

					if(standbyMode.equals("CHE")) client.println("<a href=\"/set_standby_che\"><button class=\"button button3\">CHE</button></a></p>");
					else client.println("<a href=\"/set_standby_che\"><button class=\"button button4\">CHE</button></a></p>");

					if(standbyMode.equals("TWINKLE")) client.println("<p><a href=\"/set_standby_twinkle\"><button class=\"button button3\">Blinken</button></a>&nbsp;");
					else client.println("<p><a href=\"/set_standby_twinkle\"><button class=\"button button4\">Blinken</button></a>&nbsp;");

					//if(standbyMode.equals("XMASTREE")) client.println("<a href=\"/set_standby_xmastree\"><button class=\"button button3\">Weihnachtsbaum</button></a></p>");
					//else client.println("<a href=\"/set_standby_xmastree\"><button class=\"button button4\">Weihnachtsbaum</button></a></p>");

					if(standbyMode.equals("NULL")) client.println("<a href=\"/set_standby_null\"><button class=\"button button3\">Aus</button></a></p>");
					else client.println("<a href=\"/set_standby_null\"><button class=\"button button4\">Aus</button></a></p><br>");

					//client.println("<hr class=\"striped-border\" />");
					//client.println("---------------------------------------------------------------------------------");

					client.println("<p><a href=\"/presentation\"><button class=\"button button4\">Grafik anzeigen</button></a></p><br>");
					client.println("<p><a href=\"/settings\"><button class=\"button button4\">Weitere Einstellungen</button></a></p><br>");
					//client.println("<p><a href=\"/clock_restart\"><button class=\"button button4\">Uhr neustarten</button></a></p><br>");

					//Display firmware Version
					client.println("<br><br><p>WiFi Signalqualität: " + (String)(int)getWifiSignalStrength() + "%</p>");
					client.println("<p>Firmwareversion: " + String(SW_VERSION) + "</p><br>");

					client.println("</body></html>");

					// The HTTP response ends with another blank line
					client.println();
					// Break out of the while loop
					break;
				} else { // if you got a newline, then clear currentLine
					currentLine = "";
				}
			} else if (c != '\r') {  // if you got anything else but a carriage return character,
				currentLine += c;      // add it to the end of the currentLine
			}
		}else{
			client.stop();
			break;
		}
	}

	header = "";

	client.stop();

	//Log_println("Client disconnected.");
	//Log_println("");
}

bool summertime_EU(int year, byte month, byte day, byte hour, byte tzHours)
// European Daylight Savings Time calculation by "jurs" for German Arduino Forum
// input parameters: "normal time" for year, month, day, hour and tzHours (0=UTC, 1=MEZ)
// return value: returns true during Daylight Saving Time, false otherwise
{
  if (month<3 || month>10) return false; // no DST in Jan, Feb, Nov, Dez
  if (month>3 && month<10) return true; // DST in Apr, Mai, Jun, Jul, Aug, Sep
  if ((month==3 && (hour + 24 * day)>=(1 + tzHours + 24*(31 - (5 * year /4 + 4) % 7))) || (month==10 && (hour + 24 * day)<(1 + tzHours + 24*(31 - (5 * year /4 + 1) % 7))))
    return true;
  else
    return false;
}

void getTimeNTP(int &hour, int &min, int &sec, int &day, int &month, int &year){
	time_t now = time(nullptr);
	struct tm * timeinfo = localtime(&now);

	hour = timeinfo->tm_hour;
	min = timeinfo->tm_min;
	sec = timeinfo->tm_sec;
	day = timeinfo->tm_mday;
	month = timeinfo->tm_mon + 1;
	year = timeinfo->tm_year + 1900;
}

int convertMin(int min){
	if(min>=10){
		min = min%10;
	}

	if(min<5){
		return min;
	}else return min-5;

}

bool readConfigFromFlash(){
	settings.begin("settings", false);

	if(!settings.getBool("configStored", false)) return false;

	//read all settings from Flash
	standbyMode = settings.getString("standbyMode", "TWINKLE");
	lightlevel = settings.getInt("lightlevel", 10);
	clockLang = settings.getInt("clockLang", 0);
	enableTouch = settings.getBool("enableTouch", false);

	debugmode = settings.getBool("debugmode", false);
	debugmatrix = settings.getBool("debugmatrix", false);

	colorSec1 = settings.getBool("colorSec1", false);
	colorSec3 = settings.getBool("colorSec3", false);
	activateSec1 = settings.getBool("activateSec1", true);
	activateSec3 = settings.getBool("activateSec3", true);
	colorMode = settings.getInt("colorMode", 1);

	rgbcolor[0] = settings.getInt("rgbcolor_1", 201);
	rgbcolor[1] = settings.getInt("rgbcolor_2", 226);
	rgbcolor[2] = settings.getInt("rgbcolor_3", 255);

	rgbcolorBL[0] = settings.getInt("rgbcolorBL_1", 201);
	rgbcolorBL[1] = settings.getInt("rgbcolorBL_2", 226);
	rgbcolorBL[2] = settings.getInt("rgbcolorBL_3", 255);

	rgbcolorSec1[0] = settings.getInt("rgbcolorSec1_1", 201);
	rgbcolorSec1[1] = settings.getInt("rgbcolorSec1_2", 226);
	rgbcolorSec1[2] = settings.getInt("rgbcolorSec1_3", 255);

	rgbcolorSec3[0] = settings.getInt("rgbcolorSec3_1", 201);
	rgbcolorSec3[1] = settings.getInt("rgbcolorSec3_2", 226);
	rgbcolorSec3[2] = settings.getInt("rgbcolorSec3_3", 255);

	brightness = settings.getInt("brightness", 40);
	autoBrightness = settings.getBool("autoBrightness", true);

	fadeInOut = settings.getBool("fadeInOut", true);

	enableBacklight = settings.getBool("enableBacklight", false);
	//enableBacklight = false;
	enableAutoStandby = settings.getBool("autoStandby", true);
	backlightMode = settings.getString("backlightMode", "daylight");

	isSummertime = settings.getBool("isSummertime", true);

	enableTimeSync = settings.getBool("enableTimeSync", true);

	netDevName = settings.getString("netDevName", "wordclock");

	displayMinutes = settings.getInt("displayMinutes", 0);
	colormodeMin = settings.getInt("colormodeMin", 0);
	rgbcolorMin[0] = settings.getInt("rgbcolorMin_1", 255);
	rgbcolorMin[1] = settings.getInt("rgbcolorMin_2", 0);
	rgbcolorMin[2] = settings.getInt("rgbcolorMin_3", 0);


	settings.end();

	settings.begin("wifi", false);
	wifiSSID =  settings.getString("ssid", "none");
	wifiPassword =  settings.getString("password", "none");
	settings.end();

	return true;
}

void writeConfigToFlash(){
	settings.begin("settings", false);

	settings.putString("standbyMode", standbyMode);
	settings.putInt("lightlevel", lightlevel);
	settings.putInt("clockLang", clockLang);
	settings.putBool("enableTouch", enableTouch);

	settings.putBool("debugmode", debugmode);
	settings.putBool("debugmatrix", debugmatrix);

	settings.putBool("colorSec1", colorSec1);
	settings.putBool("colorSec3", colorSec3);
	settings.putBool("activateSec1", activateSec1);
	settings.putBool("activateSec3", activateSec3);
	settings.putInt("colorMode", colorMode);

	settings.putInt("rgbcolor_1", rgbcolor[0]);
	settings.putInt("rgbcolor_2", rgbcolor[1]);
	settings.putInt("rgbcolor_3", rgbcolor[2]);

	settings.putInt("rgbcolorBL_1", rgbcolorBL[0]);
	settings.putInt("rgbcolorBL_2", rgbcolorBL[1]);
	settings.putInt("rgbcolorBL_3", rgbcolorBL[2]);

	settings.putInt("rgbcolorSec1_1", rgbcolorSec1[0]);
	settings.putInt("rgbcolorSec1_2", rgbcolorSec1[1]);
	settings.putInt("rgbcolorSec1_3", rgbcolorSec1[2]);

	settings.putInt("rgbcolorSec3_1", rgbcolorSec3[0]);
	settings.putInt("rgbcolorSec3_2", rgbcolorSec3[1]);
	settings.putInt("rgbcolorSec3_3", rgbcolorSec3[2]);

	settings.putInt("brightness", brightness);
	settings.putBool("autoBrightness", autoBrightness);

	settings.putBool("fadeInOut", fadeInOut);

	settings.putBool("configStored", true);

	settings.putString("backlightMode", backlightMode);
	settings.putBool("enableBacklight", enableBacklight);
	settings.putBool("autoStandby", enableAutoStandby);

	settings.putBool("isSummertime", isSummertime);

	settings.putBool("enableTimeSync", enableTimeSync);

	settings.putString("netDevName", netDevName);

	settings.putInt("displayMinutes", displayMinutes);
	settings.putInt("colormodeMin", colormodeMin);
	settings.putInt("rgbcolorMin_1", rgbcolorMin[0]);
	settings.putInt("rgbcolorMin_2", rgbcolorMin[1]);
	settings.putInt("rgbcolorMin_3", rgbcolorMin[2]);

	settings.end();

	settings.begin("wifi", false);
	settings.putString("ssid", wifiSSID);
	settings.putString("password", wifiPassword);
	settings.end();
}

String getConfig(){
	String confTmp = "";

	//1
	confTmp += standbyMode;
	//2
	confTmp += "," + String(lightlevel);
	//3
	confTmp += "," + String(clockLang);
	//4
	confTmp += "," + String(enableTouch);
	//5
	confTmp += "," + String(debugmode);
	//6
	confTmp += "," + String(debugmatrix);

	//7
	confTmp += "," + String(colorSec1);
	//8
	confTmp += "," + String(colorSec3);
	//9
	confTmp += "," + String(activateSec1);
	//10
	confTmp += "," + String(activateSec3);
	//11
	confTmp += "," + String(colorMode);

	//12
	confTmp += "," + String(rgbcolor[0]);
	//13
	confTmp += "," + String(rgbcolor[1]);
	//14
	confTmp += "," + String(rgbcolor[2]);

	//15
	confTmp += "," + String(rgbcolorBL[0]);
	//16
	confTmp += "," + String(rgbcolorBL[1]);
	//17
	confTmp += "," + String(rgbcolorBL[2]);

	//18
	confTmp += "," + String(rgbcolorSec1[0]);
	//19
	confTmp += "," + String(rgbcolorSec1[1]);
	//20
	confTmp += "," + String(rgbcolorSec1[2]);

	//21
	confTmp += "," + String(rgbcolorSec3[0]);
	//22
	confTmp += "," + String(rgbcolorSec3[1]);
	//23
	confTmp += "," + String(rgbcolorSec3[2]);

	//24
	confTmp += "," + String(brightness);
	//25
	confTmp += "," + String(autoBrightness);

	//26
	confTmp += "," + String(fadeInOut);

	//27
	confTmp += "," + backlightMode;
	//28
	confTmp += "," + String(enableBacklight);
	//29
	confTmp += "," + String(enableAutoStandby);
	//30
	confTmp += "," + String(isSummertime);
	//31
	confTmp += "," + String(enableTimeSync);
	//32
	confTmp += "," + netDevName;
	//33
	confTmp += "," + String(displayMinutes);

	//34
	confTmp += "," + String(rgbcolorMin[0]);
	//35
	confTmp += "," + String(rgbcolorMin[1]);
	//36
	confTmp += "," + String(rgbcolorMin[2]);

	//37
	confTmp += "," + String(colormodeMin);
	return confTmp;
}

String getDebugData(){
	String logStr = "~~~~ Actual system values ~~~~<br><br>";

	logStr += "SW Version: " + String(SW_VERSION) + "<br>";

	logStr += "Hardware s/n: " + getWiFiKey(false) + "<br>";
	logStr += "Chipset revision: " + String(ESP.getChipRevision()) + "<br>";

	//Get actual RAM usage
	logStr += "Mem. pressure: " + String(ESP.getHeapSize() - ESP.getFreeHeap()) + " / " + String(ESP.getHeapSize()) + " Bytes<br>";
	logStr += "Main CPU clock freqency: " + String(ESP.getCpuFreqMHz()) + " MHz<br>";

	logStr += "Communication sync errors: " + (String)commErrorCounter + "<br>";

	DateTime now = rtc.now();

	char actTime[120];

	int Hour = now.hour();
	int Minute = now.minute();
	int Second= now.second();

	sprintf(actTime, "%02d:%02d:%02d", Hour,Minute,Second);

	logStr += "Actual hardware time: " + (String)actTime + "<br>";

	char internetTime[120];
	sprintf(internetTime, "-");

	if(timeSynced){
		int hour_ntp, min_ntp, sec_ntp, day_ntp, mon_ntp, year_ntp;
		getTimeNTP(hour_ntp, min_ntp, sec_ntp, day_ntp, mon_ntp, year_ntp);
		sprintf(internetTime, "%02d:%02d:%02d", hour_ntp,min_ntp,sec_ntp);
	}

	logStr += "Actual internet time: " + String(internetTime) + "<br>";

	logStr += "WiFi related:<br>";

	logStr += "IP: " + WiFi.localIP().toString() + "<br>";
	logStr += "Signal strength: " + (String)(int)getWifiSignalStrength() + " %<br>";
	logStr += "Network device name: " + netDevName + "<br>";
	logStr += "--> Should be available via 'http://" + netDevName + ".local/'<br><br>";

	logStr += "Matrix:<br><br>";

	for(int k=9;k>=0;k--){
		//rows
		for(int l=0;l<11;l++){
			//cols
			logStr += (String)ledMatrixObj.ledMatrix[k][l] + " ";
		}
		logStr += " <-- Row " + String(k) + "<br>";
	}
	logStr += "<br><br>";

	logStr += "~~~~ Complete Eventlog ~~~~<br><br>";
	logStr += "<b>Note messages:</b><br>";
	logStr += logging.getNotes();
	logStr += "<br><br>";
	logStr += "<b>Warning messages:</b><br>";
	logStr += logging.getWarnings();
	logStr += "<br><br>";
	logStr += "<b>Error messages:</b><br>";
	logStr += logging.getErrors();
	logStr += "<br><br>";

	return logStr;

}

void importConfig(String confStr){
	char* stringPtr;
	char csv_data[1024];
	strncpy(csv_data, confStr.c_str(), sizeof(csv_data));

	stringPtr = strtok(csv_data, ",");
	int argNum = 1;

	while (stringPtr != NULL)
	{
		switch(argNum){
		case 1:
			//standby mode
			standbyMode = stringPtr;
			break;
		case 2:
			//lightlevel
			lightlevel = ((String)stringPtr).toInt();
			break;
		case 3:
			//clockLang
			clockLang = ((String)stringPtr).toInt();
			break;
		case 4:
			//enableTouch
			if(((String)stringPtr).equals("1")) enableTouch = true;
			else enableTouch = false;
			break;
		case 5:
			//debugmode
			if(((String)stringPtr).equals("1")) debugmode = true;
			else debugmode = false;
			break;
		case 6:
			//debugmatrix
			if(((String)stringPtr).equals("1")) debugmatrix = true;
			else debugmatrix = false;
			break;
		case 7:
			//colorSec1
			if(((String)stringPtr).equals("1")) colorSec1 = true;
			else colorSec1 = false;
			break;
		case 8:
			//colorSec3
			if(((String)stringPtr).equals("1")) colorSec3 = true;
			else colorSec3 = false;
			break;
		case 9:
			//colorSec1
			if(((String)stringPtr).equals("1")) activateSec1 = true;
			else activateSec1 = false;
			break;
		case 10:
			//colorSec3
			if(((String)stringPtr).equals("1")) activateSec3 = true;
			else activateSec3 = false;
			break;
		case 11:
			//colorMode
			colorMode = ((String)stringPtr).toInt();
			break;
		case 12:
			//rgbcolor
			rgbcolor[0] = ((String)stringPtr).toInt();
			break;
		case 13:
			//rgbcolor
			rgbcolor[1] = ((String)stringPtr).toInt();
			break;
		case 14:
			//rgbcolor
			rgbcolor[2] = ((String)stringPtr).toInt();
			break;
		case 15:
			//rgbcolorBL
			rgbcolorBL[0] = ((String)stringPtr).toInt();
			break;
		case 16:
			//rgbcolorBL
			rgbcolorBL[1] = ((String)stringPtr).toInt();
			break;
		case 17:
			//rgbcolorBL
			rgbcolorBL[2] = ((String)stringPtr).toInt();
			break;
		case 18:
			//rgbcolorSec1
			rgbcolorSec1[0] = ((String)stringPtr).toInt();
			break;
		case 19:
			//rgbcolorSec1
			rgbcolorSec1[1] = ((String)stringPtr).toInt();
			break;
		case 20:
			//rgbcolorSec1
			rgbcolorSec1[2] = ((String)stringPtr).toInt();
			break;
		case 21:
			//rgbcolorSec3
			rgbcolorSec1[0] = ((String)stringPtr).toInt();
			break;
		case 22:
			//rgbcolorSec3
			rgbcolorSec1[1] = ((String)stringPtr).toInt();
			break;
		case 23:
			//rgbcolorSec3
			rgbcolorSec1[2] = ((String)stringPtr).toInt();
			break;
		case 24:
			//brightness
			brightness = ((String)stringPtr).toInt();
			break;
		case 25:
			//autoBrightness
			if(((String)stringPtr).equals("1")) autoBrightness = true;
			else autoBrightness = false;
			break;
		case 26:
			//fadeInOut
			if(((String)stringPtr).equals("1")) fadeInOut = true;
			else fadeInOut = false;
			break;
		case 27:
			//backlight mode
			backlightMode = stringPtr;
			break;
		case 28:
			//enable backlight
			if(((String)stringPtr).equals("1")) enableBacklight = true;
			else enableBacklight = false;
			break;
		case 29:
			//enable auto standby
			if(((String)stringPtr).equals("1")) enableAutoStandby = true;
			else enableAutoStandby = false;
			break;
		case 30:
			//isSummertime
			if(((String)stringPtr).equals("1")) isSummertime = true;
			else isSummertime = false;
			break;
		case 31:
			//enableTimeSync
			if(((String)stringPtr).equals("1")) enableTimeSync = true;
			else enableTimeSync = false;
			break;
		case 32:
			//netDevName
			netDevName = stringPtr;
			break;
		case 33:
			//displayMinutes
			displayMinutes = ((String)stringPtr).toInt();
			break;
		case 34:
			//rgbcolorMin
			rgbcolorMin[0] = ((String)stringPtr).toInt();
			break;
		case 35:
			//rgbcolorMin
			rgbcolorMin[1] = ((String)stringPtr).toInt();
			break;
		case 36:
			//rgbcolorMin
			rgbcolorMin[2] = ((String)stringPtr).toInt();
			break;
		case 37:
			//colormodeMin
			colormodeMin = ((String)stringPtr).toInt();
			break;

		}

		//Log_println(stringPtr);
		stringPtr = strtok(NULL, ",");
		argNum++;
	}

	//set number according to parameter
	if(argNum != 30){
		Log_println("Importing error: Wrong input format -> Number of parameters not correct.",2);
		return;
	}

	//Save config to flash
	writeConfigToFlash();

	//Softrestart
	ESP.restart();
}

void loop()
{
	//Get commands over serial connection
	while(Serial.available()) {
		mode= Serial.readString();
	}

	//WiFi -> Listen for incoming clients
	client = server.available();

	//Handle client if connected
	if (client) wifiConnectedHandle(client);

	//execute switch for control options
	if(!mode.equals("")) controlSwitch();


	//execute backlight control sequence
	controlBacklight();

	//Check time interval
	unsigned long currentMillis = millis();

	if (((unsigned long)(currentMillis - previousMillis) >= refresh_interval)) {

		//Check touch sensor if enabled
		if(enableTouch && !firstCycle){
			if(readTouchSens()>=30){
				//if sensor touched
				//wait for second touch
				unsigned long currentMillisTouch = millis();
				unsigned long previousMillisTouch = currentMillisTouch;
				delay(100);
				while((unsigned long)(currentMillisTouch - previousMillisTouch) <= 500){
					if(readTouchSens()>=30){
						if(forceStandby) fadeInBL=true;
						forceStandby = !forceStandby;
						//Log_println((String)readTouchSens());
						currentMillisTouch=0;
						previousMillisTouch=0;
						if(forceStandby) Log_println("Wechsel in Modus Standby (manuell).");
						else Log_println("Wechsel in Modus Betrieb (manuell).");
						break;
					}
					currentMillisTouch = millis();
				}
			}
		}

		//Set standby-variable according to lightlevel and mode
		if(getLightSensorValue()>lightlevel && (getLightSensorValue()>(lastStandbyValue+5))){
			if(standby) Log_println("Wechsel in Modus Betrieb (automatisch).");
			standby=false;
			lastStandbyValue = 0;
		}else if(getLightSensorValue()<lightlevel && enableAutoStandby){
			if(!standby) Log_println("Wechsel in Modus Standby (automatisch).");
			standby=true;
		}

		//Map brightness value to sensor value
		if(autoBrightness){
			setAutoBrightness();
			if(abs(brightness-lastBrightnessChange)>=40){
				lastBrightnessChange = brightness;
				showStrip();
			}

		}

		//check if standby is active
		if((standby && (standbyMode.equals("JUS")||standbyMode.equals("CHE")||standbyMode.equals("NULL")||standbyMode.equals("TWINKLE")||standbyMode.equals("XMASTREE"))) || forceStandby){
			if(standbyMode.equals("JUS") && isInStandby==false){
				fadeOutBL = true;
				ledMatrixObj.setStandby("JUS");

				//turn off minutes display if BL is off -> turn off all BL leds
				if(!enableBacklight && displayMinutes) clearBacklight();

				refreshLedPanel(debugmatrix);
				//save actual matrix
				saveMatrix(false);
				isInStandby=true;
				lastStandbyValue = getLightSensorValue();
				fadeInBL = true;
			}
			else if(standbyMode.equals("CHE") && isInStandby==false){
				fadeOutBL = true;
				ledMatrixObj.setStandby("CHE");

				//turn off minutes display if BL is off -> turn off all BL leds
				if(!enableBacklight && displayMinutes) clearBacklight();

				refreshLedPanel(debugmatrix);
				//save actual matrix
				saveMatrix(false);
				isInStandby=true;
				lastStandbyValue = getLightSensorValue();
				fadeInBL = true;
			}else if(standbyMode.equals("XMASTREE") && isInStandby==false){
				fadeOutBL = true;
				ledMatrixObj.setStandby("XMASTREE");

				//turn off minutes display if BL is off -> turn off all BL leds
				if(!enableBacklight && displayMinutes) clearBacklight();

				refreshLedPanel(debugmatrix);
				//save actual matrix
				saveMatrix(false);
				isInStandby=true;
				lastStandbyValue = getLightSensorValue();
				fadeInBL = true;
			}
			else if(standbyMode.equals("NULL") && isInStandby==false){
				fadeOutBL = true;
				ledMatrixObj.setStandby("NULL");

				//turn off minutes display if BL is off -> turn off all BL leds
				if(!enableBacklight && displayMinutes) clearBacklight();

				refreshLedPanel(debugmatrix);
				//save actual matrix
				saveMatrix(false);
				isInStandby=true;
				lastStandbyValue = getLightSensorValue();
				fadeInBL = true;
			}
			else if(standbyMode.equals("TWINKLE") && isInStandby==false){
				fadeOutBL = true;
				ledMatrixObj.setStandby("NULL");

				//turn off minutes display if BL is off -> turn off all BL leds
				if(!enableBacklight && displayMinutes) clearBacklight();

				refreshLedPanel(debugmatrix);

				isInStandby=true;
				lastStandbyValue = getLightSensorValue();
				//change to standby twinkle mode if not touched
//				if(!forceStandby){
//					//reset matrix
//					saveMatrix(true);
//					standby_Twinkle();
//					fadeInBL = true;
//				}else saveMatrix(false); //save matrix

				if(!forceStandby) saveMatrix(true);
				else saveMatrix(false);
				standby_Twinkle(forceStandby);
				fadeInBL = true;
			}
		}else {
			//get actual time
			DateTime now = rtc.now();
			int hour = now.hour();
			hour_unconverted = now.hour();
			current_min_rtc = now.minute();

			//check communication
			if(hour>24 || hour<0){
				commErrorCounter++;
				Log_println("Communication error with DS3231. Try to restart i2c bus...",2);

				//restart i2c RTC comm
				rtc.begin();
				delay(500);
				return;
			}

			//convert 12/24 hour mode
			if(hour>12) hour = hour-12;

			if(current_min!=current_min_rtc || isInStandby){

				//check time setting after at least 2 minutes
				if(!firstCycle && !timeSynced && cycleCounter>2){
					if(!timeSyncFail){
						timeSynced = true;

						int hour_ntp, min_ntp, sec_ntp, day_ntp, mon_ntp, year_ntp;
						getTimeNTP(hour_ntp, min_ntp, sec_ntp, day_ntp, mon_ntp, year_ntp);

						//check time
						if(year_ntp>=2019){
							DateTime now = rtc.now();
							if(now.hour() != hour_ntp || now.minute() != min_ntp){
								//If time is not correct, adjust
								Log_println("Checking time -> Not correct. Setting to synchronized NTP time...",1);
								rtc.adjust(DateTime(year_ntp, mon_ntp, day_ntp, hour_ntp, min_ntp, sec_ntp));
								//rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
								delay(500);
							}else Log_println("Checking time -> Correct.");
						}else{
							Log_println("Synchronized time not plausible.",1);
							timeSynced=false;
							timeSyncFail=true;
						}
					}
				}
				if(cycleCounter<=3) cycleCounter++;

				//refresh led matrix
				current_min = current_min_rtc;

				//refresh BL if minutes display is on
				if(displayMinutes>0 && enableBacklight){
					if(convertMin(current_min_rtc)==0 || firstCycle){
						//refresh BL before matrix if matrix changed
						blNeedRefresh = true;
						controlBacklight();
					}else blNeedRefresh = true;
				}else if(displayMinutes>0 && !enableBacklight){
					//if BL is off -> direct control min display
					if(convertMin(current_min_rtc)!=0 || firstCycle){
						if(displayMinutes>2) clearBacklight();
						setMinutesBL(convertMin(current_min_rtc));
						FastLED.show();
					}
					else clearBacklight();
				}

				//set current time to matrix
				setLedMatrix(hour,current_min_rtc);
				if(current_hour!=hour) current_hour = hour;
				isInStandby=false;
				if(isInPresentationMode){
					isInPresentationMode = false;
					standbyMode = standbyModeSaved;
				}
				if(firstCycle) firstCycle = false;

			}
		}

		previousMillis = currentMillis;
	}
}

void softRefresh(){
	//debugmode = true;
	refreshed=true;
	current_min = 0;
	current_hour = 0;
}

void setLedMatrix(int hour, int min){

	ledMatrixObj.setTime(hour, min, debugmode);

	//if(debugmode) Log_println(ledMatrixObj.getDebug());

	//Compare Matrices and only refresh if different
	if(matrixChanges()||refreshed){
		refreshLedPanel(debugmatrix);
		//save actual matrix
		saveMatrix(false);
		if(refreshed) fadeInOut = tmpFadeSetting;
		refreshed=false;
	}

}

void setMinutesBL(int min){
	//If mode=0 -> No display
	if(displayMinutes==0) return;

	if(displayMinutes==1){
		//Middle
		switch(min){
		case 1:
			//1 min after
			setPixel(112,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(113,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(114,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			break;
		case 2:
			//2 min after
			setPixel(112,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(113,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(114,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(119,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(120,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(121,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			break;
		case 3:
			//3 min after
			setPixel(112,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(113,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(114,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(119,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(120,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(121,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(126,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(127,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(128,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			break;
		case 4:
			//4 min after
			setPixel(112,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(113,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(114,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(119,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(120,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(121,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(126,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(127,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(128,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(133,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(134,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(135,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			break;
		}

	}else if(displayMinutes==2){
		//Edges
		switch(min){
		case 1:
			//1 min after
			//setPixel(115,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(116,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(117,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			//setPixel(118,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			break;
		case 2:
			//2 min after
			//setPixel(122,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(116,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(117,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(123,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(124,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			//setPixel(125,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			break;
		case 3:
			//3 min after
			//setPixel(129,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(116,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(117,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(123,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(124,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(130,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(131,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			//setPixel(132,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			break;
		case 4:
			//4 min after
			//setPixel(110,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(116,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(117,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(123,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(124,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(130,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(131,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(111,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(136,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			//setPixel(137,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			break;
		}

	}else if(displayMinutes==3){
		//Middle only one side at once
		switch(min){
		case 1:
			//1 min after
			setPixel(112,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(113,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(114,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			break;
		case 2:
			//2 min after
			setPixel(119,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(120,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(121,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			break;
		case 3:
			//3 min after
			setPixel(126,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(127,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(128,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			break;
		case 4:
			//4 min after
			setPixel(133,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(134,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(135,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			break;
		}
	}else if(displayMinutes==4){
		//Edges only one side at once
		switch(min){
		case 1:
			//1 min after
			//setPixel(115,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(116,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(117,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			//setPixel(118,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			break;
		case 2:
			//2 min after
			//setPixel(122,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(123,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(124,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			//setPixel(125,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			break;
		case 3:
			//3 min after
			//setPixel(129,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(130,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(131,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			//setPixel(132,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			break;
		case 4:
			//4 min after
			//setPixel(110,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(111,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			setPixel(136,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			//setPixel(137,rgbcolorMin[0],rgbcolorMin[1],rgbcolorMin[2]);
			break;
		}
	}
}

int getLightSensorValue(){
	int photocellReading = analogRead(ldrPin);

//	if (photocellReading < 10) {
//		Log_println(" - Dunkel");
//	} else if (photocellReading < 200) {
//		Log_println(" - Halbdunkel/Daemmerung");
//	} else if (photocellReading < 500) {
//		Log_println(" - Hell");
//	} else if (photocellReading < 800) {
//		Log_println(" - Tageslicht");
//	} else {
//		Log_println(" - Sonnenlicht");
//	}

	return photocellReading;
}

void setAutoBrightness(){
	int boundary = 300;
	int sensorVal = getLightSensorValue();
	if(sensorVal>=1500) sensorVal = 1500;
	if(sensorVal < boundary) sensorVal = boundary;

	brightness = map(sensorVal, boundary, 1500, 40, 220);
}

int readTouchSens()
{
	int out = (50 - touchRead(touchPin));
	// change to adjust sensitivity as required
	if (out > 10 ){
		return (out + 2);
	}
	else{
		return 0;
	}
}

void setDaylightColor(int hour){
	//set daytime dependent color temps
	if(hour >= 0 && hour < 7){
		//temp 2500K
		ledMatrixObj.calcColorTemp(2500);
		//set all colors to calculated temp
		rgbcolor[0] = ledMatrixObj.getTempRed();
		rgbcolor[1] = ledMatrixObj.getTempGreen();
		rgbcolor[2] = ledMatrixObj.getTempBlue();
		rgbcolorSec1[0] = ledMatrixObj.getTempRed();
		rgbcolorSec1[1] = ledMatrixObj.getTempGreen();
		rgbcolorSec1[2] = ledMatrixObj.getTempBlue();
		rgbcolorSec3[0] = ledMatrixObj.getTempRed();
		rgbcolorSec3[1] = ledMatrixObj.getTempGreen();
		rgbcolorSec3[2] = ledMatrixObj.getTempBlue();
		//refresh matrix if color has changed
		if(previousDaytimeSection!=0) saveMatrix(true); //reset matrix
		previousDaytimeSection = 0;

	}else if(hour >= 7 && hour < 10){
		//temp 3000K
		ledMatrixObj.calcColorTemp(3000);
		//set all colors to calculated temp
		rgbcolor[0] = ledMatrixObj.getTempRed();
		rgbcolor[1] = ledMatrixObj.getTempGreen();
		rgbcolor[2] = ledMatrixObj.getTempBlue();
		rgbcolorSec1[0] = ledMatrixObj.getTempRed();
		rgbcolorSec1[1] = ledMatrixObj.getTempGreen();
		rgbcolorSec1[2] = ledMatrixObj.getTempBlue();
		rgbcolorSec3[0] = ledMatrixObj.getTempRed();
		rgbcolorSec3[1] = ledMatrixObj.getTempGreen();
		rgbcolorSec3[2] = ledMatrixObj.getTempBlue();
		//refresh matrix if color has changed
		if(previousDaytimeSection!=1) saveMatrix(true); //reset matrix
		previousDaytimeSection = 1;

	}else if(hour >= 10 && hour < 15){
		//temp 6000K
		ledMatrixObj.calcColorTemp(6000);
		//set all colors to calculated temp
		rgbcolor[0] = ledMatrixObj.getTempRed();
		rgbcolor[1] = ledMatrixObj.getTempGreen();
		rgbcolor[2] = ledMatrixObj.getTempBlue();
		rgbcolorSec1[0] = ledMatrixObj.getTempRed();
		rgbcolorSec1[1] = ledMatrixObj.getTempGreen();
		rgbcolorSec1[2] = ledMatrixObj.getTempBlue();
		rgbcolorSec3[0] = ledMatrixObj.getTempRed();
		rgbcolorSec3[1] = ledMatrixObj.getTempGreen();
		rgbcolorSec3[2] = ledMatrixObj.getTempBlue();
		//refresh matrix if color has changed
		if(previousDaytimeSection!=2) saveMatrix(true); //reset matrix
		previousDaytimeSection = 2;

	}else if(hour >= 15 && hour < 18){
		//temp 3000K
		ledMatrixObj.calcColorTemp(3000);
		//set all colors to calculated temp
		rgbcolor[0] = ledMatrixObj.getTempRed();
		rgbcolor[1] = ledMatrixObj.getTempGreen();
		rgbcolor[2] = ledMatrixObj.getTempBlue();
		rgbcolorSec1[0] = ledMatrixObj.getTempRed();
		rgbcolorSec1[1] = ledMatrixObj.getTempGreen();
		rgbcolorSec1[2] = ledMatrixObj.getTempBlue();
		rgbcolorSec3[0] = ledMatrixObj.getTempRed();
		rgbcolorSec3[1] = ledMatrixObj.getTempGreen();
		rgbcolorSec3[2] = ledMatrixObj.getTempBlue();
		//refresh matrix if color has changed
		if(previousDaytimeSection!=3) saveMatrix(true); //reset matrix
		previousDaytimeSection = 3;

	}else if((hour >= 18 && hour < 23) || hour < 7){
		//temp 2500K
		ledMatrixObj.calcColorTemp(2500);
		//set all colors to calculated temp
		rgbcolor[0] = ledMatrixObj.getTempRed();
		rgbcolor[1] = ledMatrixObj.getTempGreen();
		rgbcolor[2] = ledMatrixObj.getTempBlue();
		rgbcolorSec1[0] = ledMatrixObj.getTempRed();
		rgbcolorSec1[1] = ledMatrixObj.getTempGreen();
		rgbcolorSec1[2] = ledMatrixObj.getTempBlue();
		rgbcolorSec3[0] = ledMatrixObj.getTempRed();
		rgbcolorSec3[1] = ledMatrixObj.getTempGreen();
		rgbcolorSec3[2] = ledMatrixObj.getTempBlue();
		//refresh matrix if color has changed
		if(previousDaytimeSection!=4) saveMatrix(true); //reset matrix
		previousDaytimeSection = 4;

	}

	blNeedRefresh = true;
}

void setDaylightColorMin(int hour){
	//set daytime dependent color temps
	if(hour >= 0 && hour < 7){
		//temp 2500K
		ledMatrixObj.calcColorTemp(2500);
		//set all colors to calculated temp
		rgbcolorMin[0] = ledMatrixObj.getTempRed();
		rgbcolorMin[1] = ledMatrixObj.getTempGreen();
		rgbcolorMin[2] = ledMatrixObj.getTempBlue();

	}else if(hour >= 7 && hour < 10){
		//temp 3000K
		ledMatrixObj.calcColorTemp(3000);
		//set all colors to calculated temp
		rgbcolorMin[0] = ledMatrixObj.getTempRed();
		rgbcolorMin[1] = ledMatrixObj.getTempGreen();
		rgbcolorMin[2] = ledMatrixObj.getTempBlue();

	}else if(hour >= 10 && hour < 15){
		//temp 6000K
		ledMatrixObj.calcColorTemp(6000);
		//set all colors to calculated temp
		rgbcolorMin[0] = ledMatrixObj.getTempRed();
		rgbcolorMin[1] = ledMatrixObj.getTempGreen();
		rgbcolorMin[2] = ledMatrixObj.getTempBlue();

	}else if(hour >= 15 && hour < 18){
		//temp 3000K
		ledMatrixObj.calcColorTemp(3000);
		//set all colors to calculated temp
		rgbcolorMin[0] = ledMatrixObj.getTempRed();
		rgbcolorMin[1] = ledMatrixObj.getTempGreen();
		rgbcolorMin[2] = ledMatrixObj.getTempBlue();

	}else if((hour >= 18 && hour < 23) || hour < 7){
		//temp 2500K
		ledMatrixObj.calcColorTemp(2500);
		//set all colors to calculated temp
		rgbcolorMin[0] = ledMatrixObj.getTempRed();
		rgbcolorMin[1] = ledMatrixObj.getTempGreen();
		rgbcolorMin[2] = ledMatrixObj.getTempBlue();

	}

	blNeedRefresh = true;
}

//void setLedRow(int row, int array[]){
//	memcpy(ledMatrix[row], array, sizeof(ledMatrix));
//}

void setPixel(int Pixel, int red, int green, int blue) {
	// FastLED
	if(NUM_LEDS_BL<28 && Pixel>126) return;
	else{
		leds[Pixel].g = red;
		leds[Pixel].r = green;
		leds[Pixel].b = blue;
	}

}

void showStrip() {
	FastLED.setBrightness(brightness);
	FastLED.show();
}

void saveMatrix(bool reset){
	for(int i=0;i<10;i++){
			//rows
			for(int j=0;j<11;j++){
				//Cols
				if(reset) ledMatrixSaved[i][j]=-1;
				else ledMatrixSaved[i][j]=ledMatrixObj.ledMatrix[i][j];
			}
		}
}

bool matrixChanges(){
	for(int i=0;i<10;i++){
			//rows
			for(int j=0;j<11;j++){
				//Cols
				if(ledMatrixSaved[i][j] != ledMatrixObj.ledMatrix[i][j]) return true;
			}
		}
	return false;
}

void fadeInChanges(int speed){
	bool fade_BL = false;
	if(fadeInBL) fade_BL = true;

	//calc stepsize for color
	float tmpRed = 0.0;
	float tmpGreen = 0.0;
	float tmpBlue = 0.0;
	float stepRed = (float)rgbcolor[0]/100;
	float stepGreen = (float)rgbcolor[1]/100;
	float stepBlue = (float)rgbcolor[2]/100;

	float tmpRedSec1 = 0.0;
	float tmpGreenSec1 = 0.0;
	float tmpBlueSec1 = 0.0;
	float stepRedSec1 = (float)rgbcolorSec1[0]/100;
	float stepGreenSec1 = (float)rgbcolorSec1[1]/100;
	float stepBlueSec1 = (float)rgbcolorSec1[2]/100;

	float tmpRedSec3 = 0.0;
	float tmpGreenSec3 = 0.0;
	float tmpBlueSec3 = 0.0;
	float stepRedSec3 = (float)rgbcolorSec3[0]/100;
	float stepGreenSec3 = (float)rgbcolorSec3[1]/100;
	float stepBlueSec3 = (float)rgbcolorSec3[2]/100;

	float tmpRedBL = 0.0, tmpGreenBL = 0.0, tmpBlueBL = 0.0, stepRedBL = 0.0, stepGreenBL = 0.0, stepBlueBL = 0.0, actRedBL = 0.0, actGreenBL = 0.0, actBlueBL = 0.0;

	//Get colors of BL
	if(enableBacklight && fade_BL){

		CRGB rgb_val;
		hsv2rgb_rainbow(CHSV(hue_actual,sat,val), rgb_val);
		actRedBL = rgb_val.r;
		actGreenBL = rgb_val.g;
		actBlueBL = rgb_val.b;

		stepRedBL = (float)rgb_val.r/100;
		stepGreenBL = (float)rgb_val.g/100;
		stepBlueBL = (float)rgb_val.b/100;
	}


	for(int k=0;k<100;k++){
		//calc new color values
		//subtract 0.5 to round down, casting to int truncates decimal places
		if((tmpRed + stepRed)<rgbcolor[0]) tmpRed = tmpRed + stepRed;
		else tmpRed=rgbcolor[0];

		if((tmpGreen + stepGreen)<rgbcolor[1]) tmpGreen = tmpGreen + stepGreen;
		else tmpGreen = rgbcolor[1];

		if((tmpBlue + stepBlue)<rgbcolor[2]) tmpBlue = tmpBlue + stepBlue;
		else tmpBlue = rgbcolor[2];

		if(colorSec1){
			if((tmpRedSec1 + stepRedSec1)<rgbcolorSec1[0]) tmpRedSec1 = tmpRedSec1 + stepRedSec1;
			else tmpRedSec1=rgbcolorSec1[0];

			if((tmpGreenSec1 + stepGreenSec1)<rgbcolorSec1[1]) tmpGreenSec1 = tmpGreenSec1 + stepGreenSec1;
			else tmpGreenSec1 = rgbcolorSec1[1];

			if((tmpBlueSec1 + stepBlueSec1)<rgbcolorSec1[2]) tmpBlueSec1 = tmpBlueSec1 + stepBlueSec1;
			else tmpBlueSec1 = rgbcolorSec1[2];
		}

		if(colorSec3){
			if((tmpRedSec3 + stepRedSec3)<rgbcolorSec3[0]) tmpRedSec3 = tmpRedSec3 + stepRedSec3;
			else tmpRedSec3=rgbcolorSec3[0];

			if((tmpGreenSec3 + stepGreenSec3)<rgbcolorSec3[1]) tmpGreenSec3 = tmpGreenSec3 + stepGreenSec3;
			else tmpGreenSec3 = rgbcolorSec3[1];

			if((tmpBlueSec3 + stepBlueSec3)<rgbcolorSec3[2]) tmpBlueSec3 = tmpBlueSec3 + stepBlueSec3;
			else tmpBlueSec3 = rgbcolorSec3[2];
		}

		if(enableBacklight && fade_BL){
			if((tmpRedBL + stepRedBL)<actRedBL) tmpRedBL = tmpRedBL + stepRedBL;
			else tmpRedBL=actRedBL;

			if((tmpGreenBL + stepGreenBL)<actGreenBL) tmpGreenBL = tmpGreenBL + stepGreenBL;
			else tmpGreenBL = actGreenBL;

			if((tmpBlueBL + stepBlueBL)<actBlueBL) tmpBlueBL = tmpBlueBL + stepBlueBL;
			else tmpBlueBL = actBlueBL;
		}


		for(int i=0;i<10;i++){
			//rows
			for(int j=0;j<11;j++){
				//Cols
				if(ledMatrixSaved[i][j] != ledMatrixObj.ledMatrix[i][j] && ledMatrixObj.ledMatrix[i][j] != -1){
					//If the matrix position is different AND not turned off in saved

					//set new color to led
					if(colorSec1 && inSec1(ledMatrixObj.ledMatrix[i][j])) setPixel(ledMatrixObj.ledMatrix[i][j], (int)tmpRedSec1, (int)tmpGreenSec1, (int)tmpBlueSec1);
					else if(colorSec3 && inSec3(ledMatrixObj.ledMatrix[i][j])) setPixel(ledMatrixObj.ledMatrix[i][j], (int)tmpRedSec3, (int)tmpGreenSec3, (int)tmpBlueSec3);
					else setPixel(ledMatrixObj.ledMatrix[i][j], (int)tmpRed, (int)tmpGreen, (int)tmpBlue);
				}
			}
		}

		//Fade in Backlight
		if(enableBacklight && fade_BL){
			fill_gradient_RGB(leds,NUM_LEDS-NUM_LEDS_BL,CRGB((int)tmpRedBL,(int)tmpGreenBL,(int)tmpBlueBL),NUM_LEDS-1,CRGB((int)tmpRedBL,(int)tmpGreenBL,(int)tmpBlueBL));
		}

		showStrip();
		delay(speed);
	}

	//set remaining values to rgbcolor[]
	//Backlight
	if(enableBacklight && fade_BL){
		fill_gradient_RGB(leds,NUM_LEDS-NUM_LEDS_BL,CHSV(hue_actual,sat,val),NUM_LEDS-1,CHSV(hue_actual,sat,val));
	}

	//Front
	for(int l=0;l<10;l++){
		//rows
		for(int m=0;m<11;m++){
			//Cols
			if(ledMatrixSaved[l][m] != ledMatrixObj.ledMatrix[l][m] && ledMatrixObj.ledMatrix[l][m] != -1){
				//If the matrix position is different AND not turned off in saved

				//set new color to led
				//setPixel(ledMatrixObj.ledMatrix[l][m], rgbcolor[0], rgbcolor[1], rgbcolor[2]);
				if(colorSec1 && inSec1(ledMatrixObj.ledMatrix[l][m])) setPixel(ledMatrixObj.ledMatrix[l][m], rgbcolorSec1[0], rgbcolorSec1[1], rgbcolorSec1[2]);
				else if(colorSec3 && inSec3(ledMatrixObj.ledMatrix[l][m])) setPixel(ledMatrixObj.ledMatrix[l][m], rgbcolorSec3[0], rgbcolorSec3[1], rgbcolorSec3[2]);
				else setPixel(ledMatrixObj.ledMatrix[l][m], rgbcolor[0], rgbcolor[1], rgbcolor[2]);
			}
		}
	}
	fadeInBL = false;
	blockBacklight = false;

	showStrip();

}

void fadeOutChanges(int speed){
	bool fade_BL = false;
	if(fadeOutBL) fade_BL = true;


	//calc stepsize for color
	float tmpRed = (float)rgbcolor[0];
	float tmpGreen = (float)rgbcolor[1];
	float tmpBlue = (float)rgbcolor[2];
	float stepRed = (float)rgbcolor[0]/100;
	float stepGreen = (float)rgbcolor[1]/100;
	float stepBlue = (float)rgbcolor[2]/100;

	float tmpRedSec1 = (float)rgbcolorSec1[0];
	float tmpGreenSec1 = (float)rgbcolorSec1[1];
	float tmpBlueSec1 = (float)rgbcolorSec1[2];
	float stepRedSec1 = (float)rgbcolorSec1[0]/100;
	float stepGreenSec1 = (float)rgbcolorSec1[1]/100;
	float stepBlueSec1 = (float)rgbcolorSec1[2]/100;

	float tmpRedSec3 = (float)rgbcolorSec3[0];
	float tmpGreenSec3 = (float)rgbcolorSec3[1];
	float tmpBlueSec3 = (float)rgbcolorSec3[2];
	float stepRedSec3 = (float)rgbcolorSec3[0]/100;
	float stepGreenSec3 = (float)rgbcolorSec3[1]/100;
	float stepBlueSec3 = (float)rgbcolorSec3[2]/100;

	float tmpRedBL = 0.0, tmpGreenBL = 0.0, tmpBlueBL = 0.0, stepRedBL = 0.0, stepGreenBL = 0.0, stepBlueBL = 0.0;

	//Get colors of BL
	if(enableBacklight && fade_BL){

		CRGB rgb_val;
		hsv2rgb_rainbow(CHSV(hue_actual,sat,val), rgb_val);

		tmpRedBL = (float)rgb_val.r;
		tmpGreenBL = (float)rgb_val.g;
		tmpBlueBL = (float)rgb_val.b;
		stepRedBL = (float)rgb_val.r/100;
		stepGreenBL = (float)rgb_val.g/100;
		stepBlueBL = (float)rgb_val.b/100;
	}

	for(int k=0;k<100;k++){
		//calc new color values
		//add 0.5 to round up, casting to int truncates decimal places
		if(tmpRed - stepRed>0) tmpRed = tmpRed - stepRed;
		else tmpRed=0;

		if(tmpGreen - stepGreen>0) tmpGreen = tmpGreen - stepGreen;
		else tmpGreen = 0;

		if(tmpBlue - stepBlue>0) tmpBlue = tmpBlue - stepBlue;
		else tmpBlue = 0;

		if(colorSec1){
			if(tmpRedSec1 - stepRedSec1>0) tmpRedSec1 = tmpRedSec1 - stepRedSec1;
			else tmpRedSec1=0;

			if(tmpGreenSec1 - stepGreenSec1>0) tmpGreenSec1 = tmpGreenSec1 - stepGreenSec1;
			else tmpGreenSec1 = 0;

			if(tmpBlueSec1 - stepBlueSec1>0) tmpBlueSec1 = tmpBlueSec1 - stepBlueSec1;
			else tmpBlueSec1 = 0;
		}

		if(colorSec3){
			if(tmpRedSec3 - stepRedSec3>0) tmpRedSec3 = tmpRedSec3 - stepRedSec3;
			else tmpRedSec3=0;

			if(tmpGreenSec3 - stepGreenSec3>0) tmpGreenSec3 = tmpGreenSec3 - stepGreenSec3;
			else tmpGreenSec3 = 0;

			if(tmpBlueSec3 - stepBlueSec3>0) tmpBlueSec3 = tmpBlueSec3 - stepBlueSec3;
			else tmpBlueSec3 = 0;
		}

		if(enableBacklight && fade_BL){
			if(tmpRedBL - stepRedBL>0) tmpRedBL = tmpRedBL - stepRedBL;
			else tmpRedBL=0;

			if(tmpGreenBL - stepGreenBL>0) tmpGreenBL = tmpGreenBL - stepGreenBL;
			else tmpGreenSec3 = 0;

			if(tmpBlueBL - stepBlueBL>0) tmpBlueBL = tmpBlueBL - stepBlueBL;
			else tmpBlueBL = 0;
		}

		for(int i=0;i<10;i++){
			//rows
			for(int j=0;j<11;j++){
				//Cols
				if((ledMatrixSaved[i][j] != ledMatrixObj.ledMatrix[i][j]) && (ledMatrixSaved[i][j] != -1) && !refreshed){
					//If the matrix position is different AND not turned off in saved

					//set new color to led
					if(colorSec1 && inSec1(ledMatrixSaved[i][j])) setPixel(ledMatrixSaved[i][j], (int)tmpRedSec1, (int)tmpGreenSec1, (int)tmpBlueSec1);
					else if(colorSec3 && inSec3(ledMatrixSaved[i][j])) setPixel(ledMatrixSaved[i][j], (int)tmpRedSec3, (int)tmpGreenSec3, (int)tmpBlueSec3);
					else setPixel(ledMatrixSaved[i][j], (int)tmpRed, (int)tmpGreen, (int)tmpBlue);

					//Auskommentiert seit 2.6.1
					//Fade Backlight
					//if(enableBacklight && fade_BL){
					//	fill_gradient_RGB(leds,NUM_LEDS-NUM_LEDS_BL,CRGB((int)tmpRedBL,(int)tmpGreenBL,(int)tmpBlueBL),NUM_LEDS-1,CRGB((int)tmpRedBL,(int)tmpGreenBL,(int)tmpBlueBL));
					//}

				}
				else if(refreshed && (ledMatrixSaved[i][j] != -1)){
					//fade out if refreshed
					if(colorSec1 && inSec1(ledMatrixSaved[i][j])) setPixel(ledMatrixSaved[i][j], (int)tmpRedSec1, (int)tmpGreenSec1, (int)tmpBlueSec1);
					else if(colorSec3 && inSec3(ledMatrixSaved[i][j])) setPixel(ledMatrixSaved[i][j], (int)tmpRedSec3, (int)tmpGreenSec3, (int)tmpBlueSec3);
					else setPixel(ledMatrixSaved[i][j], (int)tmpRed, (int)tmpGreen, (int)tmpBlue);
				}
			}
		}

		//Fade Backlight
		if(!refreshed){
			if(enableBacklight && fade_BL){
				fill_gradient_RGB(leds,NUM_LEDS-NUM_LEDS_BL,CRGB((int)tmpRedBL,(int)tmpGreenBL,(int)tmpBlueBL),NUM_LEDS-1,CRGB((int)tmpRedBL,(int)tmpGreenBL,(int)tmpBlueBL));
			}
		}

		fadeOutBL = false;

		showStrip();
		delay(speed);
	}

	//set remaining values to (0,0,0)
	for(int l=0;l<10;l++){
		//rows
		for(int m=0;m<11;m++){
			//Cols
			if(ledMatrixSaved[l][m] != ledMatrixObj.ledMatrix[l][m] && ledMatrixSaved[l][m] != -1){
				//If the matrix position is different AND not turned off in saved

				//set new color to led
				setPixel(ledMatrixSaved[l][m], 0, 0, 0);
			}
		}
	}

	//Backlight:
	if(!refreshed){
		if(enableBacklight && fade_BL){
			fill_gradient_RGB(leds,NUM_LEDS-NUM_LEDS_BL,CRGB(0,0,0),NUM_LEDS-1,CRGB(0,0,0));
		}
	}

	showStrip();
}

void refreshLedPanel(bool serialOutput){

	if(fadeInOut){
		//Fade in/out effect
		//Fade out all changed LEDs to black (0,0,0)
		if(!firstCycle) fadeOutChanges(20);
		//change color if auto mode is on
		if(colorMode == 1){
			int hour = 0;
			if(hour_unconverted>12) hour = hour_unconverted-12;
			if(hour != current_hour){
				setDaylightColor(hour_unconverted);
				if(displayMinutes && colormodeMin==1) setDaylightColorMin(hour_unconverted);
			}
		}
		if(refreshed) saveMatrix(true);
		delay(100);
		//Fade in all changed LEDs to rgbcolor[]
		fadeInChanges(20);
	}
	else{
		//Hard reset+update all LEDs
		clearLedPanel();

		//change color if auto mode is on
		if(colorMode == 1){
			int hour = 0;
			if(hour_unconverted>12) hour = hour_unconverted-12;
			if(hour != current_hour){
				setDaylightColor(hour_unconverted);
				if(displayMinutes && colormodeMin==1) setDaylightColorMin(hour_unconverted);
			}
		}
		if(refreshed) saveMatrix(true);

		//iterate through matrix
		for(int i=0;i<10;i++){
			//rows
			for(int j=0;j<11;j++){
				//Cols
				//turn on LEDs
				if(ledMatrixObj.ledMatrix[i][j]!=-1){
					//Set color for led number
					if(colorSec1 && inSec1(ledMatrixObj.ledMatrix[i][j])) setPixel(ledMatrixObj.ledMatrix[i][j], rgbcolorSec1[0], rgbcolorSec1[1], rgbcolorSec1[2]);
					else if(colorSec3 && inSec3(ledMatrixObj.ledMatrix[i][j]))setPixel(ledMatrixObj.ledMatrix[i][j], rgbcolorSec3[0], rgbcolorSec3[1], rgbcolorSec3[2]);
					else setPixel(ledMatrixObj.ledMatrix[i][j], rgbcolor[0], rgbcolor[1], rgbcolor[2]);
				}
			}
		}

		showStrip();
	}


	//Debug
	//if(serialOutput){
	//	for(int k=0;k<10;k++){
			//rows
	//		for(int l=0;l<11;l++){
				//Cols
	//			Serial.print((String)ledMatrixObj.ledMatrix[k][l] + " ");
	//		}
	//		Log_println("");
			//m--;
	//	}
	//	Log_println("");
	//}
}

void clearLedPanel(){
	fill_solid(leds, NUM_LEDS-NUM_LEDS_BL, CRGB::Black);
	FastLED.show();
}

void clearBacklight(){
	fill_gradient(leds,NUM_LEDS-NUM_LEDS_BL,CHSV(0,0,0),NUM_LEDS-1,CHSV(0,0,0));
	FastLED.show();
}

bool inSec1(int led){
	//define words for Sec1
	int sec1Include [5] = {109,108,106,105,104};

	for(int i=0;i<5;i++){
		if(led==sec1Include[i]){
			return true;
		}
	}
	return false;
}

bool inSec3(int led){
	//define words for Sec3
	int sec3Include [3] = {8,9,10};

	for(int i=0;i<3;i++){
		if(led==sec3Include[i]){
			return true;
		}
	}
	return false;
}

void testLedMatrix(int red, int blue, int green){
	//iterate through all leds in array
	//while(true){
		for(int i = 0; i < NUM_LEDS; i++ ) {
			clearLedPanel();
			setPixel(i, red, green, blue);
			showStrip();
			delay(600);
		}
		clearLedPanel();
		showStrip();
	//}

}

void testBLMatrix(int red, int blue, int green){
	//iterate through all leds in array
	//while(true){
	for(int i = NUM_LEDS-NUM_LEDS_BL; i < NUM_LEDS; i++ ) {


		leds[i].g = green;
		leds[i].r = red;
		leds[i].b = blue;

		//FastLED.setBrightness(brightness);
		FastLED.show();

		delay(600);

		leds[i].g = 0;
		leds[i].r = 0;
		leds[i].b = 0;

		//FastLED.setBrightness(brightness);
		FastLED.show();
	}


}

void testPower(){
	//Light up one character
	clearLedPanel();
	brightness = 40;
	setPixel(109, 255, 255, 255);
	showStrip();
	delay(3000);
	brightness = 130;
	showStrip();
	delay(3000);
	brightness = 220;
	showStrip();
	delay(3000);

	//Light up one line
	clearLedPanel();
	brightness = 40;
	setPixel(109, 255, 255, 255);
	setPixel(108, 255, 255, 255);
	setPixel(107, 255, 255, 255);
	setPixel(106, 255, 255, 255);
	setPixel(105, 255, 255, 255);
	setPixel(104, 255, 255, 255);
	setPixel(103, 255, 255, 255);
	setPixel(102, 255, 255, 255);
	setPixel(101, 255, 255, 255);
	setPixel(100, 255, 255, 255);
	setPixel(99, 255, 255, 255);
	showStrip();
	delay(3000);
	brightness = 130;
	showStrip();
	delay(3000);
	brightness = 220;
	showStrip();
	delay(3000);

	//Light up the whole panel w/o BL
	clearLedPanel();
	brightness = 40;
	for(int i = 0; i < NUM_LEDS; i++ ) {
		setPixel(i, 255, 255, 255);
	}
	showStrip();
	delay(3000);
	brightness = 130;
	showStrip();
	delay(3000);
	brightness = 220;
	showStrip();
	delay(3000);

	//Light up the whole panel with BL
	if(enableBacklight){
		brightness = 40;
		fill_gradient_RGB(leds,NUM_LEDS-NUM_LEDS_BL,CRGB(255,255,255),NUM_LEDS-1,CRGB(255,255,255));
		showStrip();
		delay(3000);
		brightness = 130;
		showStrip();
		delay(3000);
		brightness = 220;
		showStrip();
		delay(3000);
	}

	clearLedPanel();

}


unsigned int rand_interval(unsigned int min, unsigned int max)
{
    int r;
    const unsigned int range = 1 + max - min;
    const unsigned int buckets = RAND_MAX / range;
    const unsigned int limit = buckets * range;

    /* Create equal size buckets all in a row, then fire randomly towards
     * the buckets until you land in one of them. All buckets are equally
     * likely. If you land off the end of the line of buckets, try again. */
    do
    {
        r = rand();
    } while (r >= limit);

    return min + (r / buckets);
}

void standby_Twinkle(bool forced){
	float tmpRed;
	float tmpGreen;
	float tmpBlue;
	float stepRed;
	float stepGreen;
	float stepBlue;

	bool stop = false;
	bool lock = false;

	int randomLED = 0;

	unsigned long currentMillisTouch = millis();
	unsigned long previousMillisTouch = currentMillisTouch;

	while(true){
		//calc random LED num
		randomLED = rand_interval(0,109);

		tmpRed = 0.0;
		tmpGreen = 0.0;
		tmpBlue = 0.0;
		stepRed = (float)rgbcolor[0]/100;
		stepGreen = (float)rgbcolor[1]/100;
		stepBlue = (float)rgbcolor[2]/100;

		//FADE IN
		//-------
		for(int k=0;k<100;k++){
			//calc new color values
			//subtract 0.5 to round down, casting to int truncates decimal places
			if((tmpRed + stepRed)<rgbcolor[0]) tmpRed = tmpRed + stepRed;
			else tmpRed=rgbcolor[0];

			if((tmpGreen + stepGreen)<rgbcolor[1]) tmpGreen = tmpGreen + stepGreen;
			else tmpGreen = rgbcolor[1];

			if((tmpBlue + stepBlue)<rgbcolor[2]) tmpBlue = tmpBlue + stepBlue;
			else tmpBlue = rgbcolor[2];


			setPixel(randomLED, (int)tmpRed, (int)tmpGreen, (int)tmpBlue);

			if(readTouchSens()>=30 && !lock){
				//if sensor touched
				previousMillisTouch = currentMillisTouch;
				lock = true;
			}

			//WiFi -> Listen for incoming clients
			client = server.available();

			//Handle client if connected
			if (client) wifiConnectedHandle(client);

			if(mode.equals("GOWAKE")) stop = true;

			currentMillisTouch = millis();

			if((unsigned long)(currentMillisTouch - previousMillisTouch) >= 1000 && lock){
				lock = false;
			}

			if(((unsigned long)(currentMillisTouch - previousMillisTouch) >= 200) && ((unsigned long)(currentMillisTouch - previousMillisTouch) <= 500) && (readTouchSens()>=30)){
				stop = true;
			}

			showStrip();
			delay(30);
		}

		setPixel(randomLED, rgbcolor[0], rgbcolor[1], rgbcolor[2]);

		showStrip();

		//FADE OUT
		//--------
		//calc stepsize for color
		tmpRed = (float)rgbcolor[0];
		tmpGreen = (float)rgbcolor[1];
		tmpBlue = (float)rgbcolor[2];
		stepRed = (float)rgbcolor[0]/100;
		stepGreen = (float)rgbcolor[1]/100;
		stepBlue = (float)rgbcolor[2]/100;

		for(int k=0;k<100;k++){
			//calc new color values
			//add 0.5 to round up, casting to int truncates decimal places
			if(tmpRed - stepRed>0) tmpRed = tmpRed - stepRed;
			else tmpRed=0;

			if(tmpGreen - stepGreen>0) tmpGreen = tmpGreen - stepGreen;
			else tmpGreen = 0;

			if(tmpBlue - stepBlue>0) tmpBlue = tmpBlue - stepBlue;
			else tmpBlue = 0;

			setPixel(randomLED, (int)tmpRed, (int)tmpGreen, (int)tmpBlue);

			if(readTouchSens()>=30 && !lock && !stop){
				//if sensor touched

				previousMillisTouch = currentMillisTouch;

				lock = true;
			}

			//WiFi -> Listen for incoming clients
			client = server.available();

			//Handle client if connected
			if (client) wifiConnectedHandle(client);

			if(mode.equals("GOWAKE")) stop = true;


			currentMillisTouch = millis();

			if((unsigned long)(currentMillisTouch - previousMillisTouch) >= 1000 && !stop && lock){
				lock = false;
			}

			if(((unsigned long)(currentMillisTouch - previousMillisTouch) >= 200) && ((unsigned long)(currentMillisTouch - previousMillisTouch) <= 500) && (readTouchSens()>=30) && !stop){
				stop = true;
			}

			showStrip();
			delay(30);
		}



		//set remaining values to (0,0,0)
		for(int l=0;l<10;l++){
			//rows
			for(int m=0;m<11;m++){
				//Cols
				//set new color to led
				setPixel(ledMatrixSaved[l][m], 0, 0, 0);
			}
		}
		showStrip();

		if(getLightSensorValue()>lightlevel && !forced){
			standby=false;
			blockBacklight = true;
			break;
		}
		else if(stop && forced){
			forceStandby = false;
			blockBacklight = true;
			if(isInPresentationMode){
				isInPresentationMode = false;
				standbyMode = standbyModeSaved;
			}
			break;
		}

	}
}

