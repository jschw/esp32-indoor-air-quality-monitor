#include <Arduino.h>
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
#define NUM_LEDS 3

//Data pins for front/back
#define DATA_PIN 18

CRGB leds[NUM_LEDS];

//Set current firmware version
extern const char SW_VERSION[] = {"1.0.0"};
//1.0.0: 28.05.2022 -> Initial proof of concept with wordclock base

String netDevName = "airquality";
String deviceIP = "";

int commErrorCounter = 0;

Preferences settings;

String mode = "";

//debug settings
bool debugmode = false;

unsigned long refresh_interval=500;
unsigned long previousMillis=0;
bool firstRun = true;

//LED settings
uint8_t brightness = 40;
bool ledsEnabled = true;
//automatic brightness adjustment

////=====Logging functionality======
EventLog logging;
bool logToSerial;

////=====WiFi functionality======
bool wifiActive = false;
bool wifiEnabled = true;
static volatile bool wifiConnected = false;
//#define AP_SSID "Wordclock_WiFi_Config";       //can set ap hostname here
String wifi_config_SSID = "Airquality_WiFi_Config";
WiFiServer server(80);
WiFiClient client;
String wifiSSID="", wifiPassword="";
bool wifiConfigMode = false;
String header="";
unsigned long currentMillisWifiTimer = 0;
unsigned long previousMillisWifiTimer = 0;
unsigned long currentMillisWifiTimer2 = 0;
unsigned long previousMillisWifiTimer2 = 0;

////=====MQTT functionality======
bool mqttEnabled = true;
String mqttIp = "192.160.0.1";
int mqttPort = 1883;
String mqttSendTopic = "myroom";
String mqttClientName = "ESP32Airquality";
String mqttUserName = "";
String mqttPassword = "";

void wifiStartAPmode();
void wifiAPClientHandle();
void wifiConnectedHandle(WiFiClient client);
void wifiConnect();
double getWifiSignalStrength();

int cycleCounter = 0;


//Functions
void controlSwitch();
void setPixel(int Pixel, int red, int green, int blue);
void setLedMatrix(int hour, int min);
void showStrip();
void softRefresh();
void setDefaults();
String getWiFiKey(bool keyTypeShort); //keyTypeShort=true -> first+last two digits, otherwise -> complete
String getConfig();
String getDebugData();
void importConfig(String confStr);
void Log_println(String msg, int loglevel=0); //<-- Wrapper function for logging class

//Config
bool readConfigFromFlash();
void writeConfigToFlash();
bool configStored = false;

void clearLedPanel();

void setup()
{
	//Open serial interface
	Serial.begin(9600);

	//Configure logging, turn on or off
	logToSerial = true;
	logging.setLineBreak("<br>\n");

	//Turn off Bluetooth
	btStop();

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

	//Light all 3 LEDs in a color to signal boot process
	setPixel(0,40,115,255);
	setPixel(1,40,115,255);
	setPixel(2,40,115,255);
	showStrip();
	delay(200);
	Log_println("Ready for Wifi!");

	//Check if ssid / key are already stored
	if(!wifiSSID.equals("") && !wifiPassword.equals("")) wifiActive = true;

	if(!wifiActive || !wifiEnabled){
		Log_println("WiFi not active, continue booting...");
		WiFi.mode(WIFI_OFF);
		clearLedPanel();
	}else{
		//try to connect to wifi if turned on
		Log_println("WiFi active, try to connect...");
		wifiConnect();
	}
}

void Log_println(String msg, int loglevel){
	//Wrapper function for logging with timestamp and redirect to serial
	if(logToSerial) Serial.println(msg);

	logging.setTimestamp(" --> ");

	logging.println(msg, loglevel);

}

void controlSwitch(){

	// SET_WIFI_CREDETIALS SSID,Password
	// SET_MQTT_CONNECTION IP,Port
	// SET_MQTT_CREDENTIALS Clientname,Username,Password
	// SET_MQTT_SEND_TOPIC Topic
	// TOGGLE_WIFI
	// TOGGLE_MQTT
	// TOGGLE_LEDS
	// SET_BRIGHTNESS 40 (values: 40-255)
	// SET_DEVICENAME NeuerName <- set the Wifi DNS name: http://devicename.local
	// SET_DEBUGMODE
	// VERSION
	// SET_DEFAULTS
	// GET_SERIAL
	// GET_MEM_USAGE
	// GET_CONFIG
	// IMPORT_CONFIG val1,val2,val3,...
	

	if(!mode.equals("")){

		if(mode.equals("TOGGLE_WIFI")){
		//toggle wifi functionality
		wifiEnabled = !wifiEnabled;
		mqttEnabled = false;

		settings.begin("settings", false);
		settings.putBool("wifiEnabled",wifiEnabled);
		settings.putBool("mqttEnabled",mqttEnabled);
		settings.end();

		if(wifiEnabled) Log_println("WiFi ist nun eingeschaltet.");
		else Log_println("WiFi ist nun ausgeschaltet.");

		}else if(mode.equals("TOGGLE_MQTT")){
			//toggle mqtt functionality
			mqttEnabled = !mqttEnabled;

			settings.begin("settings", false);
			settings.putBool("mqttEnabled",mqttEnabled);
			settings.end();

			if(mqttEnabled) Log_println("MQTT ist nun eingeschaltet.");
			else Log_println("MQTT ist nun ausgeschaltet.");

		}else if(mode.equals("TOGGLE_LEDS")){
			//toggle mqtt functionality
			ledsEnabled = !ledsEnabled;

			settings.begin("settings", false);
			settings.putBool("ledsEnabled",ledsEnabled);
			settings.end();

			if(ledsEnabled) Log_println("LEDs sind nun eingeschaltet.");
			else Log_println("LEDs sind nun ausgeschaltet.");

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

		}else if(mode.indexOf("SET_BRIGHTNESS") != -1){
			//set brightness for scaling
			int spaceIndex = mode.indexOf(' ');
			int secondSpaceIndex = mode.indexOf(' ', spaceIndex + 1);
			String s_brightness = mode.substring(spaceIndex + 1, secondSpaceIndex);

			brightness = s_brightness.toInt();

			showStrip();

			//save to Flash
			writeConfigToFlash();

			//settings.begin("settings", false);
			//settings.putInt("brightness", brightness);
			//settings.putBool("autoBrightness", autoBrightness);
			//settings.end();

			Log_println("Die Helligkeit wurde eingestellt.");

		}else if(mode.equals("SET_DEBUGMODE")){
			//set debugmode on/off
			debugmode = !debugmode;

			settings.begin("settings", false);
			settings.putBool("debugmode", debugmode);
			settings.end();

			if(debugmode) Log_println("Der Debugmodus ist jetzt eingeschaltet.");
			else Log_println("Der Debugmodus ist jetzt ausgeschaltet.");

		}else if(mode.indexOf("SET_WIFI_CREDETIALS") != -1){
			// Set wifi access credentials
			wifiEnabled = true;

			// Split command
			int spaceIndex = mode.indexOf(' ');
			int secondSpaceIndex = mode.indexOf(' ', spaceIndex + 1);
			String credString = mode.substring(spaceIndex + 1, secondSpaceIndex);

			// Split credential string input
			int credIndex = credString.indexOf(',');
			int secondCredIndex = credString.indexOf(',', credIndex + 1);

			wifiSSID = credString.substring(0, credIndex);
			wifiPassword = credString.substring(credIndex + 1, secondCredIndex);

			// Save to Flash
			writeConfigToFlash();

			Log_println("Die Zugangsdaten fuer WiFi wurden eingestellt.");

			softRefresh();

		}else if(mode.indexOf("SET_MQTT_CONNECTION") != -1){
			// Set MQTT connection info
			mqttEnabled = true;

			// Split command
			int spaceIndex = mode.indexOf(' ');
			int secondSpaceIndex = mode.indexOf(' ', spaceIndex + 1);
			String credString = mode.substring(spaceIndex + 1, secondSpaceIndex);

			// Split credential string input
			int credIndex = credString.indexOf(',');
			int secondCredIndex = credString.indexOf(',', credIndex + 1);

			mqttIp = credString.substring(0, credIndex);
			mqttPort = credString.substring(credIndex + 1, secondCredIndex).toInt();

			// Save to Flash
			writeConfigToFlash();

			Log_println("Die Verbindungsdaten des MQTT Brokers wurden eingestellt.");

			softRefresh();

		}else if(mode.indexOf("SET_MQTT_CREDENTIALS") != -1){
			// Set MQTT Broker credentials
			mqttEnabled = true;
			
			// Split command
			int spaceIndex = mode.indexOf(' ');
			int secondSpaceIndex = mode.indexOf(' ', spaceIndex + 1);
			String credString = mode.substring(spaceIndex + 1, secondSpaceIndex);

			// Split color string input
			int credIndex = credString.indexOf(',');
			int secondCredIndex = credString.indexOf(',', credIndex + 1);

			mqttClientName = credString.substring(0, credIndex);
			mqttUserName = credString.substring(credIndex + 1, secondCredIndex);
			mqttPassword = credString.substring(secondCredIndex+1);

			// Save to Flash
			settings.begin("settings", false);
			settings.putBool("mqttEnabled", mqttEnabled);
			settings.putString("mqttClientName", mqttClientName);
			settings.putString("mqttUserName", mqttUserName);
			settings.putString("mqttPassword", mqttPassword);
			settings.end();

			Log_println("Die Zugangsdaten des MQTT Brokers wurden eingestellt.");

			softRefresh();

		}else if(mode.indexOf("SET_MQTT_SEND_TOPIC") != -1){
			// Set MQTT topic name for sending
			int spaceIndex = mode.indexOf(' ');
			int secondSpaceIndex = mode.indexOf(' ', spaceIndex + 1);
			mqttSendTopic = mode.substring(spaceIndex + 1, secondSpaceIndex);

			// Save to Flash
			settings.begin("settings", false);
			settings.putBool("mqttSendTopic", mqttEnabled);
			settings.end();

			Log_println("Das MQTT Topic zum Senden wurde eingestellt.");

		}else if(mode.equals("VERSION")){
			//get actual firmware version

			Log_println(String(SW_VERSION));

		}else if(mode.equals("SET_DEFAULTS")){
			setDefaults();
			writeConfigToFlash();
			Log_println("Die Standardeinstellungen wurden wiederhergestellt.");
		
		}else if(mode.equals("GET_SERIAL")){
			Log_println(getWiFiKey(false));
		
		}else if(mode.equals("GET_MEM_USAGE")){
			//Get actual RAM usage
			Log_println("Verfuegbarer RAM: " + String(ESP.getHeapSize() - ESP.getFreeHeap()) + " / " + String(ESP.getHeapSize()) + " Byte.");
		
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

		}
	}


	mode = "";
}

void setDefaults(){
	debugmode = false;

	brightness = 40;

	mqttEnabled = true;
	mqttIp = "192.160.0.1";
 	mqttPort = 1883;
	mqttSendTopic = "myroom";
	mqttClientName = "ESP32Airquality";
	mqttUserName = "";
	mqttPassword = "";

//	wifiSSID = "none";
//	wifiPassword = "none";

	netDevName = "airquality";

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
	if (!MDNS.begin("airquality-config")) {
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
			setPixel(0,220,20,20);
			setPixel(1,220,20,20);
			setPixel(2,220,20,20);
			showStrip();
			delay(400);
			clearLedPanel();
			delay(400);
			setPixel(0,220,20,20);
			setPixel(1,220,20,20);
			setPixel(2,220,20,20);
			showStrip();
			delay(400);
			clearLedPanel();
			delay(400);
			setPixel(0,220,20,20);
			setPixel(1,220,20,20);
			setPixel(2,220,20,20);
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
			setPixel(0,20,220,20);
			setPixel(1,20,220,20);
			setPixel(2,20,220,20);
			showStrip();
			delay(400);
			clearLedPanel();
			delay(400);
			setPixel(0,20,220,20);
			setPixel(1,20,220,20);
			setPixel(2,20,220,20);
			showStrip();
			delay(400);
			clearLedPanel();
			delay(400);
			setPixel(0,20,220,20);
			setPixel(1,20,220,20);
			setPixel(2,20,220,20);
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
						client.print("<h2>Airquality Monitor mit WiFi Netz verbinden</h2>");
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
	/*
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

	*/
}

bool readConfigFromFlash(){
	settings.begin("settings", false);

	if(!settings.getBool("configStored", false)) return false;

	//read all settings from Flash

	debugmode = settings.getBool("debugmode", false);
	
	brightness = settings.getInt("brightness", 40);

	netDevName = settings.getString("netDevName", "airquality");

	// MQTT settings
	mqttEnabled = settings.getBool("mqttEnabled", true);
	mqttIp = settings.getString("mqttIp", "192.160.0.1");
 	mqttPort = settings.getInt("brightness", 1883);
	mqttSendTopic = settings.getString("mqttSendTopic", "myroom");
	mqttClientName = settings.getString("mqttClientName", "ESP32Airquality");
	mqttUserName = settings.getString("mqttUserName", "");
	mqttPassword = settings.getString("mqttPassword", "");

	settings.end();

	settings.begin("wifi", false);
	wifiSSID =  settings.getString("ssid", "none");
	wifiPassword =  settings.getString("password", "none");
	settings.end();

	return true;
}

void writeConfigToFlash(){
	settings.begin("settings", false);
	settings.putBool("configStored", true);

	settings.putBool("debugmode", debugmode);
	settings.putInt("brightness", brightness);
	settings.putString("netDevName", netDevName);

	// MQTT settings
	settings.putBool("mqttEnabled", mqttEnabled);
	settings.putString("mqttIp", mqttIp);
	settings.putInt("mqttPort", mqttPort);
	settings.putString("mqttSendTopic", mqttSendTopic);
	settings.putString("mqttClientName", mqttClientName);
	settings.putString("mqttUserName", mqttUserName);
	settings.putString("mqttPassword", mqttPassword);

	settings.end();

	settings.begin("wifi", false);
	settings.putString("ssid", wifiSSID);
	settings.putString("password", wifiPassword);
	settings.end();
}

String getConfig(){
	String confTmp = "";

	//1
	confTmp += netDevName;
	//2
	confTmp += "," + String(debugmode);
	//3
	confTmp += "," + String(brightness);
	//4
	confTmp += "," + String(mqttEnabled);
	//5
	confTmp += "," + mqttIp;
	//6
	confTmp += "," + String(mqttPort);
	//7
	confTmp += "," + mqttSendTopic;
	//8
	confTmp += "," + mqttClientName;
	//9
	confTmp += "," + mqttUserName;
	//10
	confTmp += "," + mqttPassword;

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

	logStr += "WiFi related:<br>";

	logStr += "IP: " + WiFi.localIP().toString() + "<br>";
	logStr += "Signal strength: " + (String)(int)getWifiSignalStrength() + " %<br>";
	logStr += "Network device name: " + netDevName + "<br>";
	logStr += "--> Should be available via 'http://" + netDevName + ".local/'<br><br>";

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
			//netDevName
			netDevName = stringPtr;
			break;
		case 2:
			//debugmode
			if(((String)stringPtr).equals("1")) debugmode = true;
			else debugmode = false;
			break;
		case 3:
			//brightness
			brightness = ((String)stringPtr).toInt();
			break;
		case 4:
			//mqtt enabled
			if(((String)stringPtr).equals("1")) mqttEnabled = true;
			else mqttEnabled = false;
			break;
		case 5:
			//mqtt ip
			mqttIp = stringPtr;
			break;
		case 6:
			//mqtt port
			mqttPort = ((String)stringPtr).toInt();
			break;
		case 7:
			//mqtt send topic
			mqttSendTopic = stringPtr;
			break;
		case 8:
			//mqtt client name
			mqttClientName = stringPtr;
			break;
		case 9:
			//mqtt user name
			mqttUserName = stringPtr;
			break;
		case 10:
			//mqtt password
			mqttPassword = stringPtr;
			break;
		case 11:
			//spare
			break;
		case 12:
			//spare
			break;
		case 13:
			//spare
			break;
		case 14:
			//spare
			break;
		case 15:
			//spare
			break;

		}

		//Log_println(stringPtr);
		stringPtr = strtok(NULL, ",");
		argNum++;
	}

	//set number according to parameter
	if(argNum != 10){
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


void clearLedPanel(){
	fill_solid(leds, NUM_LEDS, CRGB::Black);
	FastLED.show();
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

