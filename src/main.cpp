#include <Arduino.h>
#include <bsec.h>
#include <bsec_serialized_configurations_selectivity.h>
#include "time.h"
#include "Preferences.h"
#include "WiFi.h"
#include "ESPmDNS.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "EventLog.h"
#include "FastLED.h"

//FastLED settings
#define FASTLED_ESP32
#define COLOR_ORDER 	GRB
#define CHIPSET    		WS2812

//Number of LEDs in Array for front/back
#define NUM_LEDS 3

//Data pins for front/back
#define DATA_PIN 18

CRGB leds[NUM_LEDS];

// === Settings for color mapping ===
// Min/Max color mapped value
int min_color_mapped = 96; // = green
int max_color_mapped = 0; // = red
// Min/Max input values for mapping
int min_input_mapping = 0;
int max_input_mapping = 250;

// Mode for display values
// iaq, co2, o3, pm10 (=Pollen)
String displayMode = "iaq";

// General variables
//Set current firmware version
extern const char SW_VERSION[] = {"1.0.0"};
//1.0.0: 28.05.2022 -> Initial proof of concept with wordclock base

String netDevName = "airquality";
String deviceIP = "";

int commErrorCounter = 0;

bool ventingActive = false;

Preferences settings;

String mode = "";

//debug settings
bool debugmode = false;

unsigned long reconnect_mqtt_interval = 15000;
unsigned long previousMillis = 0;
bool firstRun = true;

//LED settings
uint8_t brightness = 40;
bool ledsEnabled = true;
//automatic brightness adjustment

////=====Logging functionality======
EventLog logging;
bool logToSerial;
bool logBsecToSerial = false;

////=====WiFi functionality======
bool wifiActive = false;
bool wifiEnabled = true;
static volatile bool wifiConnected = false;
//#define AP_SSID "Wordclock_WiFi_Config";       //can set ap hostname here
String wifi_config_SSID = "Airquality_WiFi_Config";
WiFiServer server(80);
WiFiClient client;
WiFiClient clientMqtt;
String wifiSSID="", wifiPassword="";
bool wifiConfigMode = false;
String header="";
unsigned long currentMillisWifiTimer = 0;
unsigned long previousMillisWifiTimer = 0;
unsigned long currentMillisWifiTimer2 = 0;
unsigned long previousMillisWifiTimer2 = 0;

////=====MQTT functionality======
bool mqttEnabled = true;
bool mqttConnected = false;
String mqttIp = "192.160.0.33";
int mqttPort = 1883;
String mqttSendTopic = "myroom";
String mqttClientName = "ESP32Airquality";
String mqttUserName = "";
String mqttPassword = "";
PubSubClient mqttClient(clientMqtt);
// MQTT meta info
String mqttMetaRoomName = "myroom";

////======Openweathermap functionality======
bool enableOwmData = false;
String owmApiKey = "";
String longitude = "0.0";
String latitude = "0.0";
unsigned long lastTimeWeatherState = 0;
unsigned long refreshWeatherInterval = 60000;
String temp_out = "0.0";
String humidity_out = "0.0";
String windspeed_out = "0.0";
String aqi_out = "0.0";
String no2_out = "0.0";
String o3_out = "0.0";
String pm10_out = "0.0";


void wifiStartAPmode();
void wifiAPClientHandle();
void wifiConnectedHandle(WiFiClient client);
void wifiConnect();
double getWifiSignalStrength();

int cycleCounter = 0;


//Functions
void controlSwitch();
void setPixelRgb(int Pixel, int red, int green, int blue);
void showStrip();
void setDefaults();
String getWiFiKey(bool keyTypeShort); //keyTypeShort=true -> first+last two digits, otherwise -> complete
String getConfig();
String getDebugData();
void importConfig(String confStr);
void Log_println(String msg, int loglevel=0); //<-- Wrapper function for logging class
String httpGETRequest(const char* serverName);
void refreshOpenweatherData();

//Config
bool readConfigFromFlash();
void writeConfigToFlash();
bool configStored = false;

void clearLedPanel();

// BME688 Sensor related functions
void errLeds(void);
void checkBsecStatus(Bsec& bsec);
void updateBsecState(Bsec& bsec);
void bsecCallback(const bme68x_data& input, const BsecOutput& outputs);
void sendMqttData(void);
void reconnectMqttClient(void);
// Create an object of the class Bsec
Bsec bsecInst(bsecCallback);

// Air quality runtime variable declaration
// Runtime variable declaration
float outputTemp;
float outputPressure;
float outputHumidity;
float outputCo2;
float outputVocEquiv;
float outputIaq;
float outputIaqAcc;
float outputGasRes;

int airQualityState = 0;  // 0 is first LED -> Red
int lastChangeHueValue = 0;
int stateChangeHysteresis = 10;
float tempOffset = -2.6;

void setup()
{
	// Open serial interface
	Serial.begin(115200);

	// Init the sensor interface
	bsec_virtual_sensor_t sensorList[] = { 
		BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
		BSEC_OUTPUT_RAW_PRESSURE,
		BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
		BSEC_OUTPUT_IAQ,
		BSEC_OUTPUT_CO2_EQUIVALENT,
		BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
		BSEC_OUTPUT_COMPENSATED_GAS
	};

	if(!bsecInst.begin(0x77, Wire) ||
		!bsecInst.setConfig(bsec_config_selectivity) ||
		!bsecInst.updateSubscription(sensorList, ARRAY_LEN(sensorList), BSEC_SAMPLE_RATE_LP))
		checkBsecStatus(bsecInst);
		
	Serial.println("\nBSEC library version " + String(bsecInst.getVersion().major) + "." + String(bsecInst.getVersion().minor) + "." + String(bsecInst.getVersion().major_bugfix) + "." + String(bsecInst.getVersion().minor_bugfix));
	delay(10);

	// Configure logging, turn on or off
	logToSerial = true;
	logging.setLineBreak("<br>\n");

	// Turn off Bluetooth
	btStop();

	// init FastLED
	pinMode(DATA_PIN, OUTPUT);
	FastLED.addLeds<CHIPSET, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
	fill_solid(leds, 3, CRGB(0,0,0));
	FastLED.show();
	delay(500);

	// init random number generator
	randomSeed(analogRead(35));

	// Read config if stored in Flash or initialize
	// Reset EEPROM if corrupted
	// setDefaults();
	if(readConfigFromFlash()) configStored=true;
	else writeConfigToFlash();

	// Light all 3 LEDs in a color to signal boot process
	setPixelRgb(0,40,115,255);
	setPixelRgb(1,40,115,255);
	setPixelRgb(2,40,115,255);
	showStrip();
	delay(200);
	Log_println("Ready for Wifi!");

	// Check if ssid / key are already stored
	if(!wifiSSID.equals("") && !wifiPassword.equals("")) wifiActive = true;

	if(!wifiActive || !wifiEnabled){
		Log_println("WiFi not active, continue booting...");
		WiFi.mode(WIFI_OFF);
		clearLedPanel();
	}else{
		//try to connect to wifi if turned on
		Log_println("WiFi active, try to connect...");
		Serial.println(wifiSSID+"<");
		Serial.println(wifiPassword+"<");
		wifiConnect();
	}

	if (wifiConnected && mqttEnabled) {
		mqttClient.setServer(mqttIp.c_str(), mqttPort);
	
		for (int i = 0; i<=2; i++) {
			if (!mqttClient.connected()) {
				Log_println("Attempting MQTT connection to broker " + mqttIp + ":" + mqttPort + "...");
		
				if (mqttClient.connect("ESP32Client", mqttUserName.c_str(), mqttPassword.c_str() )) {
					mqttConnected = true;
					break;
				}
				else {
					Log_println("Connecting to MQTT broker failed with state: " + mqttClient.state());
					delay(1000);
				}
			} else{
				mqttConnected = true;
				break;
			}
		}

		if (mqttConnected){
			Log_println("Connected to the MQTT broker");
		}
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
	// SET_OWM_KEY xxxxx
	// SET_OWM_LON_LAT lon,lat
	// SET_ROOM_NAME room
	// SET_TEMP_OFFSET offset
	// TOGGLE_WIFI
	// TOGGLE_MQTT
	// TOGGLE_OWM
	// TOGGLE_LEDS
	// TOGGLE_BSEC_SERIAL_LOG
	// TOGGLE_VENT_STATE
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

		// Trim trailing whitespace and Linebreaks
		mode.trim();

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

		}else if(mode.equals("TOGGLE_OWM")){
			//toggle mqtt functionality
			enableOwmData = !enableOwmData;

			settings.begin("settings", false);
			settings.putBool("enableOwmData",enableOwmData);
			settings.end();

			if(mqttEnabled) Log_println("Openweathermap ist nun eingeschaltet.");
			else Log_println("Openweathermap ist nun ausgeschaltet.");

		}else if(mode.equals("TOGGLE_LEDS")){
			//toggle mqtt functionality
			ledsEnabled = !ledsEnabled;

			settings.begin("settings", false);
			settings.putBool("ledsEnabled",ledsEnabled);
			settings.end();

			if(ledsEnabled) Log_println("LEDs sind nun eingeschaltet.");
			else {
				// Turn off all LEDs
				fill_solid(leds, 3, CRGB(0,0,0));
				FastLED.show();

				Log_println("LEDs sind nun ausgeschaltet.");
			} 

		}else if(mode.equals("TOGGLE_BSEC_SERIAL_LOG")){
			//toggle bsec output logging direct to serial
			logBsecToSerial = !logBsecToSerial;

			settings.begin("settings", false);
			settings.putBool("logBsecToSerial",logBsecToSerial);
			settings.end();

			if(logBsecToSerial) Log_println("BSEC Output Log an Serial ist nun eingeschaltet.");
			else Log_println("BSEC Output Log an Serial ist nun ausgeschaltet.");

		}else if(mode.equals("TOGGLE_VENTING_STATE")){
			//toggle the venting state flag
			ventingActive = !ventingActive;

			if(ventingActive) Log_println("Es wird gerade gelueftet.");
			else Log_println("Es wird nun nicht mehr gelueftet.");

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

			settings.begin("settings", false);
			settings.putInt("brightness", brightness);
			settings.end();

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

			//Softrestart
			ESP.restart();

		}else if(mode.indexOf("SET_OWM_LON_LAT") != -1){
			// Set wifi access credentials
			wifiEnabled = true;

			// Split command
			int spaceIndex = mode.indexOf(' ');
			int secondSpaceIndex = mode.indexOf(' ', spaceIndex + 1);
			String credString = mode.substring(spaceIndex + 1, secondSpaceIndex);

			// Split credential string input
			int credIndex = credString.indexOf(',');
			int secondCredIndex = credString.indexOf(',', credIndex + 1);

			longitude = credString.substring(0, credIndex);
			latitude = credString.substring(credIndex + 1, secondCredIndex);

			// Save to Flash
			settings.begin("settings", false);
			settings.putString("longitude", longitude);
			settings.putString("latitude", latitude);
			settings.end();

			Log_println("Die Positionsdaten fuer Openweathermap wurden eingestellt.");

		}else if(mode.indexOf("SET_OWM_KEY") != -1){
			//set brightness for scaling
			int spaceIndex = mode.indexOf(' ');
			int secondSpaceIndex = mode.indexOf(' ', spaceIndex + 1);
			owmApiKey = mode.substring(spaceIndex + 1, secondSpaceIndex);

			settings.begin("settings", false);
			settings.putString("owmApiKey", owmApiKey);
			settings.end();

			Log_println("Der API Key fuer Openweathermap  wurde eingestellt.");

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
			settings.begin("settings", false);
			settings.putString("mqttIp", mqttIp);
			settings.putInt("mqttPort", mqttPort);
			settings.end();

			Log_println("Die Verbindungsdaten des MQTT Brokers wurden eingestellt.");

			//Softrestart
			ESP.restart();

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
			// Replace - with whitespace for empty Username
			if (mqttUserName.equals("-")) mqttUserName = "";
			mqttPassword = credString.substring(secondCredIndex+1);
			// Replace - with whitespace for empty Password
			if (mqttPassword.equals("-")) mqttPassword = "";

			// Save to Flash
			settings.begin("settings", false);
			settings.putBool("mqttEnabled", mqttEnabled);
			settings.putString("mqttClientName", mqttClientName);
			settings.putString("mqttUserName", mqttUserName);
			settings.putString("mqttPassword", mqttPassword);
			settings.end();

			Log_println("Die Zugangsdaten des MQTT Brokers wurden eingestellt.");

			//Softrestart
			ESP.restart();

		}else if(mode.indexOf("SET_MQTT_SEND_TOPIC") != -1){
			// Set MQTT topic name for sending
			int spaceIndex = mode.indexOf(' ');
			int secondSpaceIndex = mode.indexOf(' ', spaceIndex + 1);
			mqttSendTopic = mode.substring(spaceIndex + 1, secondSpaceIndex);

			// Save to Flash
			settings.begin("settings", false);
			settings.putString("mqttSendTopic", mqttSendTopic);
			settings.end();

			Log_println("Das MQTT Topic zum Senden wurde eingestellt.");

		}else if(mode.indexOf("SET_ROOM_NAME") != -1){
			// Set MQTT meta info room name
			int spaceIndex = mode.indexOf(' ');
			int secondSpaceIndex = mode.indexOf(' ', spaceIndex + 1);
			mqttMetaRoomName = mode.substring(spaceIndex + 1, secondSpaceIndex);

			// Save to Flash
			settings.begin("settings", false);
			settings.putString("mqttRoomName", mqttMetaRoomName);
			settings.end();

			Log_println("Das MQTT Metainfo 'room' wurde eingestellt.");

		}
		else if(mode.indexOf("SET_TEMP_OFFSET") != -1){
			// Set static temperature offset
			int spaceIndex = mode.indexOf(' ');
			int secondSpaceIndex = mode.indexOf(' ', spaceIndex + 1);
			tempOffset = mode.substring(spaceIndex + 1, secondSpaceIndex).toFloat();

			// Save to Flash
			settings.begin("settings", false);
			settings.putFloat("tempOffset", tempOffset);
			settings.end();

			Log_println("Der statische Temperaturoffset wurde eingestellt.");

		}else if(mode.equals("VERSION")){
			//get actual firmware version

			Log_println(String(SW_VERSION));

		}else if(mode.equals("SET_DEFAULTS")){
			setDefaults();
			writeConfigToFlash();
			Log_println("Die Standardeinstellungen wurden wiederhergestellt.");

			//Softrestart
			ESP.restart();
		
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
	logBsecToSerial = true;
	brightness = 40;
	tempOffset = -2.6;

	mqttEnabled = true;
	mqttIp = "192.160.0.1";
 	mqttPort = 1883;
	mqttSendTopic = "myroom";
	mqttClientName = "ESP32Airquality";
	mqttUserName = "";
	mqttPassword = "";
	mqttMetaRoomName = "myroom";

	enableOwmData = false;
	owmApiKey = "";
	longitude = "0.0";
	latitude = "0.0";

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
		// if credentials exist, try to connect

		WiFi.mode(WIFI_MODE_STA);
		// WiFi.setHostname("Wordclock");
		WiFi.setHostname(netDevName.c_str());
		WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

		// Pruefen, ob Verbindung vorhanden ist
		unsigned long currentMillisConnect = millis();
		unsigned long previousMillisConnect = currentMillisConnect;
		while ((unsigned long)(currentMillisConnect - previousMillisConnect) <= 10000){
			if(WiFi.status() == WL_CONNECTED) break;
			// Wait for connection being established...
			delay(500);
			Serial.print(".");
			currentMillisConnect = millis();
		}
		if(WiFi.status() != WL_CONNECTED){
			//If connection fails -> Shut down Wifi
			wifiActive = false;
			wifiConnected = false;
			mqttEnabled = false;
			mqttConnected = false;

			WiFi.mode(WIFI_OFF);

			//Flash LEDs red if connection failed
			clearLedPanel();
			delay(400);
			setPixelRgb(0,220,20,20);
			setPixelRgb(1,220,20,20);
			setPixelRgb(2,220,20,20);
			showStrip();
			delay(400);
			clearLedPanel();
			delay(400);
			setPixelRgb(0,220,20,20);
			setPixelRgb(1,220,20,20);
			setPixelRgb(2,220,20,20);
			showStrip();
			delay(400);
			clearLedPanel();
			delay(400);
			setPixelRgb(0,220,20,20);
			setPixelRgb(1,220,20,20);
			setPixelRgb(2,220,20,20);
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
			setPixelRgb(0,20,220,20);
			setPixelRgb(1,20,220,20);
			setPixelRgb(2,20,220,20);
			showStrip();
			delay(400);
			clearLedPanel();
			delay(400);
			setPixelRgb(0,20,220,20);
			setPixelRgb(1,20,220,20);
			setPixelRgb(2,20,220,20);
			showStrip();
			delay(400);
			clearLedPanel();
			delay(400);
			setPixelRgb(0,20,220,20);
			setPixelRgb(1,20,220,20);
			setPixelRgb(2,20,220,20);
			showStrip();
			delay(400);
			clearLedPanel();
			delay(400);

			Log_println("The Airquality Monitor is connected to the WiFi! :)");
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
						client.println(".button { background-color: #008CC2; border: none; color: white; padding: 16px 40px;");
						client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
						client.println(".button2 {background-color: #555555;}");
						client.println("input[type=submit].link {background-color: #008CC2; border: none; color: white; padding: 16px 20px;");
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
	// Log_println("New Client.");
	String currentLine = "";
	bool execControlSwitch = false;
	bool refreshPage = false;
	String waitRefresh = "3";

	while (client.connected()) {
		if (client.available()) {
			char c = client.read();
			// Debug write received http response
			// Serial.write(c);
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


					// handle the airquality monitor function based on header data
					if (header.indexOf("GET /toggle_venting") >= 0) {
						// Toggle Zustand Fenster
						mode = "TOGGLE_VENTING_STATE";
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.indexOf("GET /toggle_wifi") >= 0) {
						// Toggle WiFi
						mode = "TOGGLE_WIFI";
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.indexOf("GET /toggle_leds") >= 0) {
						// Toggle LEDs on/off
						mode = "TOGGLE_LEDS";
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.indexOf("GET /settings") >= 0) {
						// Display the settings HTML web page
						client.println("<!DOCTYPE html><html>");
						client.println("<head><meta charset=\"utf-8\" name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
						client.println("<link rel=\"icon\" href=\"data:,\">");
						// CSS code for card style design
						client.println("<link rel=\"stylesheet\" href=\"https://use.fontawesome.com/releases/v5.7.2/css/all.css\" integrity=\"sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr\" crossorigin=\"anonymous\">");
						client.println("<link rel=\"icon\" href=\"data:,\">");
						client.println("<style>");
						client.println("html {font-family: Arial; display: inline-block; text-align: center;}");
						client.println("p {  font-size: 1.2rem;}");
						client.println("body {  margin: 0;}");
						client.println(".topnav { overflow: hidden; background-color: #008CC2; color: white; font-size: 1.2rem; }");
						client.println(".content { padding: 20px; }");
						client.println(".card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }");
						client.println(".cards { max-width: 700px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); }");
						client.println(".reading { font-size: 2.8rem; }");
						client.println(".card.temperature { color: #008CC2; }");
						client.println(".card.humidity { color: #008CC2; }");
						client.println(".card.pressure { color: #3fca6b; }");
						client.println(".card.gas { color: #d62246; }");
						client.println("</style>");
						// CSS to style the on/off buttons
						client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
						client.println(".button { background-color: #008CC2; border: none; color: white; padding: 16px 40px;");
						client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
						client.println(".button2 {background-color: #555555;}");
						client.println("fieldset {margin: 8px; border: 1px solid silver; padding: 8px; border-radius: 4px;}");
						client.println("legend {padding: 2px;}");
						client.println("input[type=text].send {font-size:20px;}");
						client.println(".button3 {font-size: 20px;}");
						client.println(".button4 {font-size: 20px; background-color: #555555;}</style></head>");

						// Web Page Heading
						client.println("<div class=\"topnav\"><h3>Einstellungen</h3></div>");


						client.println("<p><a href=\"http://" + deviceIP + "/\"><button class=\"button button4\">Zurück zur Hauptseite</button></a></p>");

						// Change color front
						client.println("<fieldset>");
						client.println("<legend>MQTT Verbindung</legend>");
						// Toggle MQTT functionality
						if(mqttEnabled) client.println("<p><a href=\"/toggle_mqtt\"><button class=\"button button3\">MQTT Client ausschalten</button></a></p>");
						else client.println("<p><a href=\"/toggle_mqtt\"><button class=\"button button4\">MQTT Client einschalten</button></a></p>");
						
						// Input IP/Port
						client.println("<p>MQTT Verbindungsdaten eingeben:</p>");
						client.print("<form method='get' action='a'>");
						client.print("<input name='mqtt_conn' length=11>&nbsp;");
						client.print("<input type='submit' class='send' value='OK'><br><br><b>Format: </b> IP,Port<br>");
						client.print("</form>");

						// Input MQTT User settings
						client.println("<p>MQTT Zugangsdaten zum Broker eingeben:</p>");
						client.print("<form method='get' action='a'>");
						client.print("<input name='mqtt_user' length=11>&nbsp;");
						client.print("<input type='submit' class='send' value='OK'><br><br><b>Format: </b> Clientname,Username,Password<br>");
						client.print("</form>");

						// Input MQTT Topic
						client.println("<p>MQTT Topic (senden) eingeben:</p>");
						client.print("<form method='get' action='a'>");
						client.print("<input name='mqtt_topic' length=11>&nbsp;");
						client.print("<input type='submit' class='send' value='OK'><br>");
						client.print("</form>");
						client.println("</fieldset><br>");

						// Openweathermaps settings
						client.println("<fieldset>");
						client.println("<legend>Openweathermap Verbindung</legend>");
						if(enableOwmData) client.println("<p><a href=\"/toggle_owm\"><button class=\"button button3\">OWM ausschalten</button></a></p>");
						else client.println("<p><a href=\"/toggle_owm\"><button class=\"button button4\">OWM einschalten</button></a></p>");

						// Input OWM API Key
						client.println("<p>Openweathermap API Key eingeben:</p>");
						client.print("<form method='get' action='a'>");
						client.print("<input name='owm_key' length=11>&nbsp;");
						client.print("<input type='submit' class='send' value='OK'><br>");
						client.print("</form>");

						// Input OWM location
						client.println("<p>Standort eingeben:</p>");
						client.print("<form method='get' action='a'>");
						client.print("<input name='owm_location' length=11>&nbsp;");
						client.print("<input type='submit' class='send' value='OK'><br><br><b>Format: </b> Longitude,Latitude<br>");
						client.print("</form>");
						client.println("</fieldset><br>");

						// Input sonstige Einstellungen
						client.println("<fieldset>");
						client.println("<legend>Sonstige Geräteeinstellungen</legend>");

						// Input room name
						client.println("<p>Name des Raums eingeben:</p>");
						client.print("<form method='get' action='a'>");
						client.print("<input name='room_name' length=11>&nbsp;");
						client.print("<input type='submit' class='send' value='OK'><br>");
						client.print("</form>");

						// Input Temperatur Offset
						client.println("<p>Temperaturoffset eingeben:</p>");
						client.print("<form method='get' action='a'>");
						client.print("<input name='room_name' length=11>&nbsp;");
						client.print("<input type='submit' class='send' value='OK'><br><br>(Dezimalzahl mit Punkt, z.B. 2.6)<br>");
						client.print("</form>");

						// Input Device Name
						client.println("<p>Gerätename eingeben:</p>");
						client.print("<form method='get' action='a'>");
						client.print("<input name='device_name' length=11>&nbsp;");
						client.print("<input type='submit' class='send' value='OK'><br>");
						client.print("</form>");
						client.println("</fieldset><br>");

						//Misc functions
						client.println("<br><fieldset>");
						client.println("<legend>Weitere Funktionen</legend>");

						//Send recovery link
						client.println("<p><a href=\"/get_recoverylink\" target=\"_blank\"><button class=\"button button4\">Recovery-Link erzeugen</button></a></p>");

						//Show debug information
						client.println("<p><a href=\"/get_debugdata\" target=\"_blank\"><button class=\"button button4\">Statusinfos anzeigen</button></a></p>");

						client.println("</fieldset>");

						client.println("<br><br><br>");

						client.println("</body></html>");

						// The HTTP response ends with another blank line
						client.println();
						break;

					}else if (header.indexOf("GET /toggle_mqtt") >= 0) {
						// Toggle MQTT functionality
						mode = "TOGGLE_MQTT";
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.indexOf("GET /toggle_owm") >= 0) {
						// Toggle openweathermap data
						mode = "TOGGLE_OWM";
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.startsWith("GET /a?mqtt_conn=")) {
						// Get MQTT connection settings
						String param,tmp;
						tmp = header.substring(0,header.indexOf("\n"));
						param = tmp.substring(13, tmp.lastIndexOf(' '));

						param.replace("%2C",",");

						if (param.indexOf(",") >= 0){
							mode = "SET_MQTT_CONNECTION " + param;
						}
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.startsWith("GET /a?mqtt_user=")) {
						// Get MQTT broker credentials
						String param,tmp;
						tmp = header.substring(0,header.indexOf("\n"));
						param = tmp.substring(13, tmp.lastIndexOf(' '));

						param.replace("%2C",",");

						if (param.indexOf(",") >= 0){
							mode = "SET_MQTT_CREDENTIALS " + param;
						}
						execControlSwitch = true;
						refreshPage = true;

					}else if (header.startsWith("GET /a?mqtt_topic=")) {
						// Get MQTT sending topic
						String param,tmp;
						tmp = header.substring(0,header.indexOf("\n"));
						param = tmp.substring(13, tmp.lastIndexOf(' '));

						param.replace("%2C",",");
						mode = "SET_MQTT_SEND_TOPIC " + param;

						execControlSwitch = true;
						refreshPage = true;

					}else if (header.startsWith("GET /a?owm_key=")) {
						// Get Openweathermap API Key
						String param,tmp;
						tmp = header.substring(0,header.indexOf("\n"));
						param = tmp.substring(13, tmp.lastIndexOf(' '));

						param.replace("%2C",",");
						mode = "SET_OWM_KEY " + param;

						execControlSwitch = true;
						refreshPage = true;

					}else if (header.startsWith("GET /a?owm_location=")) {
						// Get Openweathermap location longitude, latitude
						String param,tmp;
						tmp = header.substring(0,header.indexOf("\n"));
						param = tmp.substring(13, tmp.lastIndexOf(' '));

						param.replace("%2C",",");

						if (param.indexOf(",") >= 0){
							mode = "SET_OWM_LON_LAT " + param;
						}

						execControlSwitch = true;
						refreshPage = true;

					}else if (header.startsWith("GET /a?room_name=")) {
						// Get name of the actual room
						String param,tmp;
						tmp = header.substring(0,header.indexOf("\n"));
						param = tmp.substring(13, tmp.lastIndexOf(' '));

						param.replace("%2C",",");
						mode = "SET_ROOM_NAME " + param;

						execControlSwitch = true;
						refreshPage = true;

					}else if (header.startsWith("GET /a?temp_offset=")) {
						// Get temperature correction offset
						String param,tmp;
						tmp = header.substring(0,header.indexOf("\n"));
						param = tmp.substring(13, tmp.lastIndexOf(' '));

						param.replace("%2C",",");
						mode = "SET_TEMP_OFFSET " + param;

						execControlSwitch = true;
						refreshPage = true;

					}else if (header.startsWith("GET /a?device_name=")) {
						// Get the devicename
						String param,tmp;
						tmp = header.substring(0,header.indexOf("\n"));
						param = tmp.substring(13, tmp.lastIndexOf(' '));

						param.replace("%2C",",");
						mode = "SET_DEVICENAME " + param;

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
						client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: left;}");
						client.println(".button { background-color: #008CC2; border: none; color: white; padding: 16px 40px;");
						client.println("text-decoration: none; font-size: 25px; margin: 2px; cursor: pointer;}");
						client.println(".button2 {background-color: #555555;}");
						client.println(".button3 {font-size: 20px;}");
						client.println(".button4 {font-size: 20px; background-color: #555555;}</style></head>");

						// Web Page Heading
						client.println("<body><h1>Statusinformationen</h1>");

						// Display current state of airquality monitor
						client.println("<p>Folgende Werte konnten ausgelesen werden:</p>");
						client.println("<p>" + getDebugData() + "</p>");

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
						client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: left;}");
						client.println(".button { background-color: #008CC2; border: none; color: white; padding: 16px 40px;");
						client.println("text-decoration: none; font-size: 25px; margin: 2px; cursor: pointer;}");
						client.println(".button2 {background-color: #008CC2;}");
						client.println(".button3 {font-size: 20px;}");
						client.println(".button4 {font-size: 20px; background-color: #555555;}</style></head>");

						// Web Page Heading
						client.println("<body><h1>Wiederherstellungslink</h1>");

						// Display current state of wordclock
						client.println("<p>Der Wiederherstellungslink für die aktuelle Konfiguration ist:</p>");
						client.println("<p>http://airquality.local/a?recovery=" + confString + "</p><br>");
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
					// Set colors for aqi and co2
					String colorAqiIn, colorAqiOut, colorCo2in;
					// IAQ
					if (outputIaq <= 100) colorAqiIn = "#14DC14"; // green
					else if (outputIaq > 100 && outputIaq <= 200) colorAqiIn = "#FAF01E"; // yellow
					else colorAqiIn = "#DC1414"; //red
					// IAQ out
					if (aqi_out.toInt() == 1 || aqi_out.toInt() == 2) colorAqiOut = "#14DC14"; // green
					else if (aqi_out.toInt() == 3) colorAqiOut = "#FAF01E"; // yellow
					else colorAqiOut = "#DC1414"; //red
					// IAQ out
					if (outputCo2 <= 550) colorCo2in = "#14DC14"; // green
					else if (outputCo2 > 550 && outputCo2 < 800) colorCo2in = "#FAF01E"; // yellow
					else colorCo2in = "#DC1414"; //red
					
					
					// CSS code for card style design
					client.println("<link rel=\"stylesheet\" href=\"https://use.fontawesome.com/releases/v5.7.2/css/all.css\" integrity=\"sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr\" crossorigin=\"anonymous\">");
  					client.println("<link rel=\"icon\" href=\"data:,\">");
					client.println("<style>");
    				client.println("html {font-family: Arial; display: inline-block; text-align: center;}");
    				client.println("p {  font-size: 1.2rem;}");
    				client.println("body {  margin: 0;}");
    				client.println(".topnav { overflow: hidden; background-color: #008CC2; color: white; font-size: 1.2rem; }");
    				client.println(".content { padding: 20px; }");
    				client.println(".card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }");
    				client.println(".cards { max-width: 700px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); }");
    				client.println(".reading { font-size: 2.8rem; }");
					client.println(".card.temperature { color: #008CC2; }");
					client.println(".card.humidity { color: #008CC2; }");
    				client.println(".card.pressure { color: #3fca6b; }");
    				client.println(".card.gas { color: #d62246; }");
					client.println(".card.aqii { color: " + colorAqiIn + "; }");
					client.println(".card.aqia { color: " + colorAqiOut + "; }");
					client.println(".card.co2i { color: " + colorCo2in + "; }");
  					client.println("</style>");
					// CSS to style the on/off buttons
					// Feel free to change the background-color and font-size attributes to fit your preferences
					client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
					client.println(".button { background-color: #008CC2; border: none; color: white; padding: 16px 40px;");
					client.println("text-decoration: none; font-size: 25px; margin: 2px; cursor: pointer;}");
					client.println(".button2 {background-color: #555555;}");
					client.println(".button3 {font-size: 20px;}");
					client.println(".striped-border {border: 1px dashed #000; width: 100%; margin: auto; margin-top: 5%; margin-bottom: 5%;}");
					client.println(".button4 {font-size: 20px; background-color: #555555;}</style></head>");

					// Web Page Heading
					client.println("<div class=\"topnav\"><h3>Airquality Monitor</h3></div>");

					client.println("<div class=\"content\"><div class=\"cards\">");
					client.println("<div class=\"card aqii\"><h4><i class=\"fas fa-wind\"></i> AQI INNEN</h4><p><span class=\"reading\"><span id=\"gas\">" + String(outputIaq,2) + "</span> </span></p></div>");
      				client.println("<div class=\"card temperature\"><h4><i class=\"fas fa-thermometer-half\"></i> TEMPERATUR INNEN</h4><p><span class=\"reading\"><span id=\"temp\">" + String(outputTemp,2) + "</span> &deg;C</span></p></div>");
					client.println("<div class=\"card co2i\"><h4><i class=\"fas fa-wind\"></i> CO2 INNEN</h4><p><span class=\"reading\"><span id=\"gas\">" + (String)(int)outputCo2 + "</span> ppm</span></p></div>");
      				client.println("<div class=\"card humidity\"><h4><i class=\"fas fa-tint\"></i> LUFTFEUCHTIGKEIT INNEN</h4><p><span class=\"reading\"><span id=\"hum\">" + String(outputHumidity,2) + "</span> &percnt;</span></p></div>");
					client.println("<div class=\"card temperature\"><h4><i class=\"fas fa-thermometer-half\"></i> TEMPERATUR AUSSEN</h4><p><span class=\"reading\"><span id=\"temp\">" + String(temp_out.toFloat(),2) + "</span> &deg;C</span></p></div>");
      				client.println("<div class=\"card aqia\"><h4><i class=\"fas fa-wind\"></i> AQI AUSSEN </h4><p><span class=\"reading\"><span id=\"gas\">" + String(aqi_out.toInt()) + "</span> </span></p></div>");
    				client.println("</div></div>");

					// Display some additional values
					// Set colors for ozone, nitrogen dioxide and pollen
					String colorO3, colorNo2, colorPm10;
					// Ozone
					if (o3_out.toFloat() <= 120) colorO3 = "#14DC14"; // green
					else if (o3_out.toFloat() > 120 && o3_out.toFloat() <= 180) colorO3 = "#FAF01E"; // yellow
					else colorO3 = "#DC1414"; //red
					// Nitrogen dioxide
					if (no2_out.toFloat() <= 100) colorNo2 = "#14DC14"; // green
					else if (no2_out.toFloat() > 100 && no2_out.toFloat() <= 200) colorNo2 = "#FAF01E"; // yellow
					else colorNo2 = "#DC1414"; //red
					// Pollen
					if (pm10_out.toFloat() <= 50) colorPm10 = "#14DC14"; // green
					else if (pm10_out.toFloat() > 50 && pm10_out.toFloat() <= 90) colorPm10 = "#FAF01E"; // yellow
					else colorPm10 = "#DC1414"; //red
					// Print to HTML page
					client.println("<p><br><u>Weitere Werte:</u></p>");
					client.println("<p>Luftfeuchtigkeit außen:  " + String(humidity_out.toFloat(),2) + " % </p>");
					client.println("<p>Luftdruck:  " + String(outputPressure,2) + " hPa </p>");
					client.println("<p style=\"color:" + colorO3 + "\";>Bodenozon (O3):  " + String(o3_out.toFloat(),2) + " ug/m^3 </p>");
					client.println("<p style=\"color:" + colorNo2 + "\";>Stickoxid (NO2):  " + String(no2_out.toFloat(),2) + " ug/m^3 </p>");
					client.println("<p style=\"color:" + colorPm10 + "\";>Pollen (Pm10):  " + String(pm10_out.toFloat(),2) + " ug/m^3 </p>");


					// Display current state of venting only if MQTT enabled & connected
					if (mqttEnabled && mqttConnected) {
						client.println("<p><br>Lüftungsstatus aktuell: " + (String) ((ventingActive) ? "Fenster geöffnet" : "Fenster geschlossen") + "</p>");

						if (ventingActive) {
							client.println("<p><a href=\"/toggle_venting\"><button class=\"button\">Schließen</button></a></p>");
						} else {
							client.println("<p><a href=\"/toggle_venting\"><button class=\"button button2\">Öffnen</button></a></p>");
						}
					}

					// Display current state of backlight
					client.println("<p>LED-Anzeige ein/ausschalten</p>");

					if (!ledsEnabled) {
						client.println("<p><a href=\"/toggle_leds\"><button class=\"button button2\">Einschalten</button></a></p>");
					} else {
						client.println("<p><a href=\"/toggle_leds\"><button class=\"button\">Ausschalten</button></a></p><br>");
					}


					//Change Standby mode
					client.println("<p>Ampel schalten nach Messwert:</p>");
					if(displayMode.equals("iaq")) client.println("<p><a href=\"/set_display_iaq\"><button class=\"button button3\">IAQ</button></a>&nbsp;");
					else client.println("<p><a href=\"/set_display_iaq\"><button class=\"button button4\">IAQ</button></a>&nbsp;");

					if(displayMode.equals("co2")) client.println("<a href=\"/set_display_co2\"><button class=\"button button3\">CO2</button></a></p>");
					else client.println("<a href=\"/set_display_co2\"><button class=\"button button4\">CO2</button></a></p>");
					
					if (enableOwmData){
						if(displayMode.equals("o3")) client.println("<p><a href=\"/set_display_o3\"><button class=\"button button3\">Ozonn</button></a>&nbsp;");
						else client.println("<p><a href=\"/set_display_o3\"><button class=\"button button4\">Ozon</button></a>&nbsp;");

						if(displayMode.equals("pm10")) client.println("<a href=\"/set_display_pm10\"><button class=\"button button3\">Pollen</button></a></p>");
						else client.println("<a href=\"/set_display_pm10\"><button class=\"button button4\">Pollen</button></a></p>");
					}
					
					client.println("<p>(derzeit nur IAQ implementiert)</p>");

					client.println("<p><br><a href=\"/settings\"><button class=\"button button4\">Einstellungen</button></a></p><br>");

					// Display WiFi signal strength
					client.println("<br><br><p>WiFi Signalqualität: " + (String)(int)getWifiSignalStrength() + "%</p>");
					// Display MQTT send topic if connected
					if (mqttEnabled && mqttConnected)client.println("<p>Sendet auf MQTT Topic: " + mqttClientName + "/" + mqttSendTopic + "</p><br>");
					// Display firmware Version
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
}

bool readConfigFromFlash(){
	settings.begin("settings", false);

	if(!settings.getBool("configStored", false)) return false;

	//read all settings from Flash

	debugmode = settings.getBool("debugmode", false);
	
	brightness = settings.getInt("brightness", 40);

	netDevName = settings.getString("netDevName", "airquality");

	logBsecToSerial = settings.getBool("logBsecToSerial", true);

	tempOffset = settings.getFloat("tempOffset", -2.6);

	// MQTT settings
	mqttEnabled = settings.getBool("mqttEnabled", true);
	mqttIp = settings.getString("mqttIp", "192.160.0.1");
 	mqttPort = settings.getInt("mqttPort", 1883);
	mqttSendTopic = settings.getString("mqttSendTopic", "myroom");
	mqttClientName = settings.getString("mqttClientName", "ESP32Airquality");
	mqttUserName = settings.getString("mqttUserName", "");
	mqttPassword = settings.getString("mqttPassword", "");
	mqttMetaRoomName = settings.getString("mqttRoomName", "myroom");

	// Openweather settings
	enableOwmData = settings.getBool("enableOwmData", false);
	owmApiKey = settings.getString("owmApiKey" , "");
	longitude = settings.getString("longitude", "0.0");
	latitude = settings.getString("latitude", "0.0");

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
	settings.putBool("logBsecToSerial", logBsecToSerial);
	settings.putFloat("tempOffset", tempOffset);

	// MQTT settings
	settings.putBool("mqttEnabled", mqttEnabled);
	settings.putString("mqttIp", mqttIp);
	settings.putInt("mqttPort", mqttPort);
	settings.putString("mqttSendTopic", mqttSendTopic);
	settings.putString("mqttClientName", mqttClientName);
	settings.putString("mqttUserName", mqttUserName);
	settings.putString("mqttPassword", mqttPassword);
	settings.putString("mqttRoomName", mqttMetaRoomName);

	// Openweather settings
	settings.putBool("enableOwmData", enableOwmData);
	settings.putString("owmApiKey", owmApiKey);
	settings.putString("longitude", longitude);
	settings.putString("latitude", latitude);

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
	//11
	confTmp += "," + mqttMetaRoomName;
	//12
	confTmp += "," + logBsecToSerial;
	//13
	confTmp += "," + String(tempOffset);

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
			//mqtt meta room name
			mqttMetaRoomName = stringPtr;
			break;
		case 12:
			//log bsec output to serial
			if(((String)stringPtr).equals("1")) logBsecToSerial = true;
			else logBsecToSerial = false;
			break;
		case 13:
			//temperature offset
			tempOffset = ((String)stringPtr).toFloat();
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

	if(!bsecInst.run())
		checkBsecStatus(bsecInst);

	if (!mqttClient.connected() && mqttEnabled && wifiConnected) {
		mqttConnected = false;

		// Start reconnect every x seconds
		unsigned long currentMillis = millis();
		if (((unsigned long)(currentMillis - previousMillis) >= reconnect_mqtt_interval)) {
			reconnectMqttClient();
			previousMillis = currentMillis;
		}

	}
	
	if (mqttConnected) mqttClient.loop();

	// Refresh openweathermapdata
	if (enableOwmData && WiFi.status()== WL_CONNECTED){
		unsigned long currentMillis = millis();
		if (((unsigned long)(currentMillis - lastTimeWeatherState) >= refreshWeatherInterval)) {
			refreshOpenweatherData();
			lastTimeWeatherState = currentMillis;
		}
		
	}

	// Example for interval/timer check
	/*
	unsigned long currentMillis = millis();
	if (((unsigned long)(currentMillis - previousMillis) >= refresh_interval)) {

		// Things TODO
		previousMillis = currentMillis;
	}
	*/
}


void setPixelRgb(int Pixel, int red, int green, int blue) {
	// FastLED
	if(Pixel > NUM_LEDS) return;
	else{
		leds[Pixel].g = green;
		leds[Pixel].r = red;
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

void errLeds(void)
{
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
}

void reconnectMqttClient(void) {
  // Log_println("Connection to MQTT broker lost, attempting reconnect...");
  // Attempt to connect
  if (!mqttClient.connected()) {
    if (mqttClient.connect("ESP32Client")) {
      Log_println("Reconnected to broker");
      mqttConnected = true;
    } else {
      // Log_println("Reconnect to broker failed with state: rc=" + mqttClient.state());
    }
  } else {
    mqttConnected = true;
  }
}

void updateBsecState(Bsec& bsec)
{
  static uint16_t stateUpdateCounter = 0;
	bool update = false;
  
	if (stateUpdateCounter == 0) {
    const bsec_output_t* iaq = bsec.getOutput(BSEC_OUTPUT_IAQ);
		/* First state update when IAQ accuracy is >= 3 */
    if (iaq && iaq->accuracy >= 3) {
      update = true;
      stateUpdateCounter++;
    }
	}

	if (update)
    checkBsecStatus(bsec);
}

void bsecCallback(const bme68x_data& input, const BsecOutput& outputs) 
{ 
  if (!outputs.len) 
    return;
    
  if(logBsecToSerial) Serial.println("BSEC outputs:\n\ttimestamp = " + String((int)(outputs.outputs[0].time_stamp / INT64_C(1000000))));

  for (uint8_t i = 0; i < outputs.len; i++) {
    const bsec_output_t& output = outputs.outputs[i];

    switch (output.sensor_id) {
      case BSEC_OUTPUT_IAQ:
        if(logBsecToSerial) Serial.println("\tiaq = " + String(output.signal));
        if(logBsecToSerial) Serial.println("\tiaq accuracy = " + String((int)output.accuracy));
        outputIaq = output.signal;
        outputIaqAcc = (int)output.accuracy;
        break;
      case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
        if(logBsecToSerial) Serial.println("\ttemperature = " + String(output.signal));
        outputTemp = output.signal + tempOffset;
        break;
      case BSEC_OUTPUT_RAW_PRESSURE:
        if(logBsecToSerial) Serial.println("\tpressure = " + String(output.signal));
        outputPressure = output.signal;
        break;
      case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
        if(logBsecToSerial) Serial.println("\thumidity = " + String(output.signal));
        outputHumidity = output.signal;
        break;
      case BSEC_OUTPUT_CO2_EQUIVALENT:
        if(logBsecToSerial) Serial.println("\tCO2 Equivalent = " + String(output.signal));
        outputCo2 = output.signal;
        break;
      case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
        if(logBsecToSerial) Serial.println("\tBreath VOC Equivalent = " + String(output.signal));
        outputVocEquiv = output.signal;
	  case BSEC_OUTPUT_COMPENSATED_GAS:
        if(logBsecToSerial) Serial.println("\tGas resistance = " + String(output.signal));
        outputGasRes = output.signal;
        break;
      default:
        break;
    }
  }

  // Cut input value if it exceeds min/max bundaries
  if (outputIaq > max_input_mapping) outputIaq = max_input_mapping;
  if (outputIaq < min_input_mapping) outputIaq = min_input_mapping;

  // Map IAQ value to HSV color between green and red
  uint8_t hueVal = map((long)outputIaq, min_input_mapping, max_input_mapping, min_color_mapped, max_color_mapped);
  uint8_t satVal = 255;
  uint8_t valVal = 255;

  if(logBsecToSerial) Serial.println("\tCalculated HUE Value = " + String(hueVal));

  // Turn all LEDs off
  fill_solid(leds, 3, CRGB(0,0,0));

  // Check if the value change is big enough to change the state
  // -> Avoid bouncing between two states
  if (abs(hueVal - lastChangeHueValue) >= stateChangeHysteresis) {
    // Determine which LED should be turned on
    if (hueVal >= 75) {
      // State: Good -> Green
      airQualityState = 2;
    }
    else if (hueVal < 75 && hueVal >= 32) {
      // State: Quite good -> Yellow
      airQualityState = 1;
    }
    else {
      // State: Bad -> Red
      airQualityState = 0;
    }

    // Update state change value
    lastChangeHueValue = hueVal;
  }
  
  // Set LED color
  if (ledsEnabled) {
	leds[airQualityState] = CHSV(hueVal, satVal, valVal);
	FastLED.setBrightness(brightness);
	FastLED.show();
  }

  if (mqttConnected)
    sendMqttData();
  
  updateBsecState(bsecInst);
}

void sendMqttData(void)
{
  StaticJsonDocument<1024> doc;

  doc["iaq"] = outputIaq;
  doc["iaq_acc"] = outputIaqAcc;
  doc["temp"] = outputTemp;
  doc["pressure"] = outputPressure;
  doc["humidity"] = outputHumidity;
  doc["co2_eq"] = outputCo2;
  doc["voc_eq"] = outputVocEquiv;
  doc["gas_res"] = outputGasRes;
  doc["room"] = mqttMetaRoomName;
  doc["venting"] = ((String)ventingActive).toInt();
  if (enableOwmData) {
	doc["temp_out"] = temp_out.toFloat();
	doc["humidity_out"] = humidity_out.toFloat();
	doc["windspeed"] = windspeed_out.toFloat();
	doc["aqi_out"] = aqi_out.toFloat();
	doc["no2_out"] = no2_out.toFloat();
	doc["o3_out"] = o3_out.toFloat();
	doc["pm10_out"] = pm10_out.toFloat();
  }

  String jsonPayload;
  serializeJson(doc, jsonPayload);

  String sendTopic = mqttClientName + "/" + mqttSendTopic;

  if(logBsecToSerial) Serial.println("Sending message to MQTT topic " + sendTopic + "...");
  if(logBsecToSerial) Serial.println(jsonPayload);
 
  if (mqttClient.publish(sendTopic.c_str(), (char*) jsonPayload.c_str()) == true) {
    if(logBsecToSerial) Serial.println("Success sending message");
  } else {
    if(logBsecToSerial) Serial.println("Error sending message");
  }
 
  mqttClient.loop();
}

void checkBsecStatus(Bsec& bsec)
{
  int bme68x_status = (int)bsec.getBme68xStatus();
  int bsec_status = (int)bsec.getBsecStatus();
  
  if (bsec_status < BSEC_OK) {
      Serial.println("BSEC error code : " + String(bsec_status));
      for (;;)
        errLeds(); /* Halt in case of failure */
  } else if (bsec_status > BSEC_OK) {
      Serial.println("BSEC warning code : " + String(bsec_status));
  }

  if (bme68x_status < BME68X_OK) {
      Serial.println("BME68X error code : " + String(bme68x_status));
      for (;;)
        errLeds(); /* Halt in case of failure */
  } else if (bme68x_status > BME68X_OK) {
      Serial.println("BME68X warning code : " + String(bme68x_status));
  }
}

String httpGETRequest(const char* serverName) {
  WiFiClient client;
  HTTPClient http;
    
  // Your Domain name with URL path or IP address with path
  http.begin(client, serverName);
  
  // Send HTTP POST request
  int httpResponseCode = http.GET();
  
  String payload = "{}"; 
  
  if (httpResponseCode>0) {
    // Serial.print("HTTP Response code: ");
    // Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("HTTP Client Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}

void refreshOpenweatherData() {
    String apiCallWeather = "http://api.openweathermap.org/data/2.5/weather?lat=" + latitude + "&lon=" + longitude + "&units=metric&appid=" + owmApiKey;
    String apiCallAirquality = "http://api.openweathermap.org/data/2.5/air_pollution?lat=" + latitude + "&lon=" + longitude + "&appid=" + owmApiKey;

	bool parseError = false;

    StaticJsonDocument<1024> jsonDoc;

	String jsonInput = httpGETRequest(apiCallWeather.c_str());
    // Serial.println(jsonInput);
      
    DeserializationError parseErr = deserializeJson(jsonDoc, jsonInput);
	JsonObject root;
  
    if (parseErr) {
    	Serial.println("Parsing input failed!");
    	Serial.println(parseErr.c_str());
		parseError = true;
    }
	else {
		root = jsonDoc.as<JsonObject>();
    
		temp_out = root["main"]["temp"].as<String>();
		// pressure_out = root["main"]["pressure"].as<String>();
		humidity_out = root["main"]["humidity"].as<String>();
		windspeed_out = root["wind"]["speed"].as<String>();
	}

	// Get air pollution info
	jsonInput = httpGETRequest(apiCallAirquality.c_str());
	// Serial.println(jsonInput);
      
	parseErr = deserializeJson(jsonDoc, jsonInput);
  
	if (parseErr) {
		Serial.println("Parsing airquality input failed!");
		Serial.println(parseErr.c_str());
		parseError = true;
	}
	else {
		root = jsonDoc.as<JsonObject>();
		aqi_out = root["list"][0]["main"]["aqi"].as<String>();
		no2_out = root["list"][0]["components"]["no2"].as<String>();
		o3_out = root["list"][0]["components"]["o3"].as<String>();
		pm10_out = root["list"][0]["components"]["pm10"].as<String>();
	}

}