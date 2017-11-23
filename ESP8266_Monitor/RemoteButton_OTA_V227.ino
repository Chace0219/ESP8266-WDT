#include <ESP8266WiFi.h>

#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include "AsyncPing.h"

#include "WebFunc.h"
#include "FBD.h"
#include "FiniteStateMachine.h"
#include "Mqtt.h"
#include <ArduinoJson.h>
#include "ESP8266HTTPUpdateServer.h"

// only when lolin board, if you are using another board, you can only comment this line
#define LOLINBOARD
#define CONFIG_START 0x10

String FirmVer = "2.27";

#ifdef LOLINBOARD
  #define RELAYON HIGH
  #define RELAYOFF LOW
#else
  #define RELAYON LOW
  #define RELAYOFF HIGH
#endif 

const uint8_t RelayPIN = 4;

extern uint32_t nReserveTime;
// soft AP Setting 
const char* softid = "Remote_"; //
const char* softpass = "12345678"; //


/// You can adjust timers, where is issue in timer setting
const uint32_t OffRelayTime = 8000; // for Power off, Relay time, 8sec
const uint32_t DelayTime = 2000; // This is for delay after power off
const uint32_t OnRelayTime = 1000; // For power on relay time
const uint32_t PoweronDelayTime = 100000; // for delay while it is booting.

static uint32_t mqttdiscontime = 0;
const uint32_t MQTTTRETRYINTER = 30000;
// 
const uint32_t PingInterval = 10000;// ping interval 10sec

static uint16_t nErrCount = 0;
// 
ESP8266WebServer webServer(80);
//
ESP8266HTTPUpdateServer httpUpdater;
//
AsyncPing ping;

void powerOff();
void powerOffEnter();

void powerDelay();
void powerDelayEnter();

void powerOn();
void powerOnEnter();

void powerOnDelay();
void powerOnDelayEnter();

void checkWDT();
void checkWDTEnter();


void wifiError();
void standby();
void standbyEnter();

void rebootEnter();
void rebootingUpdate();

void initTempWDTVar();

State Standby = State(standbyEnter, standby, NULL);
State Checking = State(checkWDTEnter, checkWDT, NULL);
State PowerDelay = State(powerDelayEnter, powerDelay, NULL);
State On = State(powerOnEnter, powerOn, NULL);
State OnDelay = State(powerOnDelayEnter, powerOnDelay, NULL);
State Off = State(powerOffEnter, powerOff, NULL);
State WifiError = State(initTempWDTVar, wifiError, NULL);
State Rebooting = State(rebootEnter, rebootingUpdate, NULL);

FSM RemoteDevice = FSM(Standby);

String DeviceID = "";
void initTempWDTVar()
{
	storage.lastPingTime = millis();
}

bool connectToAP(String ssid, String pass)
{
	Serial.println();
	Serial.println("Connecting to WiFi");

	// Disconnect
	WiFi.disconnect();
	delay(100);

	// 
	WiFi.begin(ssid.c_str(), pass.c_str());
	// WiFi.begin("TP-LINK_64FB80", "chunxing151201");
	uint32_t nStartTime = millis();
	const uint32_t nMaxTime = 20000; // 20 secs
	while (WiFi.status() != WL_CONNECTED && (millis() - nStartTime) < nMaxTime)
	{
		delay(500);
		Serial.print(".");
	}

	Serial.println();
	Serial.print("WiFi connected with ip ");
	Serial.println(WiFi.localIP());

	if (WiFi.status() != WL_CONNECTED)
		return false;

	// Transit to Checking Status
	RemoteDevice.transitionTo(Checking);
	return true;
}

void initWebServer()
{
	webServer.on("/", handleRoot);
	webServer.on("/status", statusProc);
	webServer.on("/manual", manualProc);
	webServer.on("/system", systemProc);
	webServer.on("/connect", connectProc);
	webServer.on("/scan", scanProc);
	webServer.on("/wdt", wdtProc);
	webServer.on("/setting", settingProc);

	httpUpdater.setup(&webServer);
	webServer.begin(); //Start the Web server
}

bool loadConfig()
{
	bool bResult = true;
	for (unsigned int t = 0; t < sizeof(storage); t++)
		*((char*)&storage + t) = EEPROM.read(CONFIG_START + t);

	if (storage.version[0] == CONFIG_VERSION[0] &&
		storage.version[1] == CONFIG_VERSION[1] &&
		storage.version[2] == CONFIG_VERSION[2])
	{
		strcpy(storage.ssid, "OMG Office");
		strcpy(storage.pass, "blueflame");
	}
	else
	{
		storage.bWdtEn = false;
		storage.nWdtTime = 300;

		strcpy(storage.serverIP, "None");

		strcpy(storage.ssid, "OMG Office");
		strcpy(storage.pass, "blueflame");
		
		// strcpy(storage.ssid, " ");
		// strcpy(storage.pass, " ");

		bResult = false;
	}

	storage.lastPingTime = millis();

	return bResult;
}

void saveConfig()
{
	storage.version[0] = CONFIG_VERSION[0];
	storage.version[1] = CONFIG_VERSION[1];
	storage.version[2] = CONFIG_VERSION[2];

	for (unsigned int t = 0; t < sizeof(storage); t++)
		EEPROM.write(CONFIG_START + t, *((char*)&storage + t));
	EEPROM.commit();

	String message;
	if (storage.bWdtEn)
		message += "Wdt function enabled,";
	else
		message += "Wdt function disabled,";
	message += storage.serverIP;
	message += ",";
	message += String(storage.nWdtTime);
	message += "(s)";
	//loggingAction("Setting is changed!", message);
}

void setup() {
	// Serial init
	Serial.begin(115200);
	EEPROM.begin(512);

#ifdef LOLINBOARD
	pinMode(RelayPIN, OUTPUT);
#else
	pinMode(RelayPIN, OUTPUT_OPEN_DRAIN);
#endif 

	digitalWrite(RelayPIN, RELAYOFF);
	// 
	WiFi.disconnect();
	delay(1000);

	WiFi.mode(WIFI_STA);
	String softssid = softid + WiFi.softAPmacAddress();
	DeviceID = softssid;
  
  // AP Mode Init
  /*
	WiFi.softAP(softssid.c_str(), softpass);
	*/

	/* callback for each answer/timeout of ping */
	ping.on(true, [](const AsyncPingResponse& response) {
		IPAddress addr(response.addr); //to prevent with no const toString() in 2.3.0
		if (response.answer)
		{
			Serial.printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%d ms\n", response.size, addr.toString().c_str(), response.icmp_seq, response.ttl, response.time);
			storage.lastPingTime = millis();
		}
		else
			Serial.printf("no answer yet for %s icmp_seq=%d\n", addr.toString().c_str(), response.icmp_seq);
		return false; //do not stop
	});

	/* callback for end of ping */
	ping.on(false, [](const AsyncPingResponse& response) {
		IPAddress addr(response.addr); //to prevent with no const toString() in 2.3.0
		Serial.printf("total answer from %s sent %d recevied %d time %d ms\n", addr.toString().c_str(), response.total_sent, response.total_recv, response.total_time);
		if (response.mac)
			Serial.printf("detected eth address " MACSTR "\n", MAC2STR(response.mac->addr));
		return true; //doesn't matter
	});

	// 
	initWebServer();

	// 
	if (loadConfig())
	{
		Serial.println("Load Successful!");
		Serial.println(storage.ssid);
		Serial.println(storage.serverIP);

		if (connectToAP(storage.ssid, storage.pass))
			RemoteDevice.transitionTo(Standby);
		else
			RemoteDevice.transitionTo(WifiError);
	}
	else
	{
		Serial.println("Load Error!");
		//RemoteDevice.transitionTo(WifiError);
		if (connectToAP(storage.ssid, storage.pass))
			RemoteDevice.transitionTo(Standby);
		else
			RemoteDevice.transitionTo(WifiError);
	}

	// mqtt setup
	client.setServer(mqtt_server, 1883);
	client.setCallback(callback);

	if (mqttconnect())
		loggingAction("connect", "");
}

void loop()
{
	static uint32_t nStatusSendTime = millis();

	if(client.connected())
		client.loop();
	RemoteDevice.update();
	webServer.handleClient();

	if (RemoteDevice.isInState(Checking) || RemoteDevice.isInState(Standby))
	{
		if (!client.connected() && WiFi.status() == WL_CONNECTED)
		{
			if (millis() - mqttdiscontime >= MQTTTRETRYINTER)
			{
				if (mqttconnect())
					loggingAction("mqtt", "");
				mqttdiscontime = millis();
			}
		}
		else
			mqttdiscontime = millis();
	}
	else
		mqttdiscontime = millis();
}

void powerOn()
{
	// Relay ON
	digitalWrite(RelayPIN, RELAYON);

	if (RemoteDevice.timeInCurrentState() >= OnRelayTime)
	{
		// Relay Off
		digitalWrite(RelayPIN, RELAYOFF);
		RemoteDevice.transitionTo(OnDelay);
	}
}

void powerOff()
{
	// 
	digitalWrite(RelayPIN, RELAYON);

	if (RemoteDevice.timeInCurrentState() >= OffRelayTime)
	{
		digitalWrite(RelayPIN, RELAYOFF);
		RemoteDevice.transitionTo(PowerDelay);
	}
}

static uint32_t nCheckWDTTime = 0;

void checkWDT()
{
	if (WiFi.status() != WL_CONNECTED)
	{
		RemoteDevice.transitionTo(WifiError);
	}
	else
	{
		if (RemoteDevice.timeInCurrentState() >= PingInterval)
		{
			IPAddress ipAddr;
			ipAddr.fromString(storage.serverIP);
			ping.begin(ipAddr);

			nCheckWDTTime++;
			if (nCheckWDTTime >= 30)
			{
				nCheckWDTTime = 0;
				loggingAction("Checking", "");
			}

			if ((millis() - storage.lastPingTime) >= storage.nWdtTime * 1000)
			{
				RemoteDevice.transitionTo(Off);
				return;
			}
			else
			{
				RemoteDevice.transitionTo(Checking);
			}
		}
	}
}

void wifiError()
{
	if (RemoteDevice.timeInCurrentState() >= 120000)
	{
		if (String(storage.ssid) != "")
		{
			if (connectToAP(storage.ssid, storage.pass))
			{
				RemoteDevice.transitionTo(Standby);
				return;
			}
		}
		RemoteDevice.transitionTo(WifiError);
	}
}

void standby()
{
	if (WiFi.status() != WL_CONNECTED)
	{
		RemoteDevice.transitionTo(WifiError);
	}
	else
	{
		if (storage.bWdtEn)
		{
			RemoteDevice.transitionTo(Checking);
		}
		else
		{
			if (RemoteDevice.timeInCurrentState() >= 300000)
			{
				Serial.println("Activate message");
				RemoteDevice.transitionTo(Standby);
				loggingAction("activated", "");
			}
		}

	}
}

void powerDelay()
{
	if (RemoteDevice.timeInCurrentState() >= DelayTime)
	{
		digitalWrite(RelayPIN, RELAYOFF);
		RemoteDevice.transitionTo(On);
	}
}

void loggingAction(String actiontype, String message)
{
	StaticJsonBuffer<512> jsonBuffer;
	JsonObject& root = jsonBuffer.createObject();
	root["devid"] = DeviceID;
	root["localip"] = WiFi.localIP().toString();
	root["actiontype"] = actiontype;
	root["message"] = message;
	root["wdten"] = storage.bWdtEn;
	root["firmver"] = FirmVer;

	String status;
	if (RemoteDevice.isInState(Standby))
		status = "standby";
	else if (RemoteDevice.isInState(Checking))
		status = "checking WDT";
	else if (RemoteDevice.isInState(PowerDelay))
		status = "delay power off";
	else if (RemoteDevice.isInState(OnDelay))
		status = "booting";
	else if (RemoteDevice.isInState(On))
		status = "Turning on";
	else if (RemoteDevice.isInState(Off))
		status = "Turning off";
	else if (RemoteDevice.isInState(WifiError))
		status = "IP not assinged";
	else if (RemoteDevice.isInState(Rebooting))
		status = "rebooting on webpage";

	root["status"] = status;

	root["serverip"] = storage.serverIP;
	root["wdttime"] = storage.nWdtTime;

	String strJSON;
	root.printTo(strJSON);
	Serial.println(strJSON);
	if (client.connected())
	{
		client.publish("/remotepower/logging", strJSON.c_str());
	}
}

void standbyEnter()
{
	initTempWDTVar();
	loggingAction("Standby", "");
}

void checkWDTEnter()
{
	initTempWDTVar();
	loggingAction("start checking", "");
	nCheckWDTTime = 0;
}

void powerDelayEnter()
{
	initTempWDTVar();
	loggingAction("turned off", "");
}

void powerOnEnter()
{
	initTempWDTVar();
	loggingAction("turned on", "");
}

void powerOffEnter()
{
	initTempWDTVar();
	String message;
	message = storage.serverIP;
	loggingAction("Relay for Power OFF", message);
}

void powerOnDelay()
{
	digitalWrite(RelayPIN, RELAYOFF);
	if (RemoteDevice.timeInCurrentState() >= PoweronDelayTime)
		RemoteDevice.transitionTo(Checking);
}

void powerOnDelayEnter()
{
	initTempWDTVar();
	loggingAction("entered booting", "");

}

void rebootingUpdate()
{
	uint32_t rebootingDelaytime = 60000;
	if (RemoteDevice.timeInCurrentState() >= rebootingDelaytime)
	{
		RemoteDevice.transitionTo(Standby);
	}
}

void rebootEnter()
{
	loggingAction("entered rebooting", "");
}


void wdtSetting(String strSetting)
{
	StaticJsonBuffer<512> jsonBuffer;
	JsonObject& root = jsonBuffer.parseObject(strSetting);
	if (root == JsonObject::invalid())
		return;

	const char *devid = root["devid"];
	String deviceid = devid;
	if (deviceid != DeviceID)
		return;
	strcpy(storage.serverIP, devid);

	storage.bWdtEn = (bool)root["wdten"];
	storage.nWdtTime = (uint32_t)root["wdttime"];
	const char *strip = root["serverip"];
	strcpy(storage.serverIP, strip);

	bool bManual = (bool)root["manual"];
	if (bManual)
	{
		RemoteDevice.transitionTo(Off);
		Serial.println("Manual ");
	}
	else
		RemoteDevice.transitionTo(Standby);
	saveConfig();
}

void rebootingCallback(String strcallback)
{
	StaticJsonBuffer<512> jsonBuffer;
	JsonObject& root = jsonBuffer.parseObject(strcallback);
	if (root == JsonObject::invalid())
		return;

	const char *devid = root["devid"];
	String deviceid = devid;
	if (deviceid != DeviceID)
		return;
	bool bRebooting = (bool)root["rebooting"];
	if (bRebooting)
	{
		RemoteDevice.transitionTo(Rebooting);
		Serial.println("rebooting is started!");
	}
}
