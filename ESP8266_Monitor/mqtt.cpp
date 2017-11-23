
#include "Mqtt.h"
#include <ArduinoJson.h>
#include "FiniteStateMachine.h"

// mqtt server address.
const char* mqtt_server = "10.0.5.2"; // current your local server, perhaps, we have to implement soem ideas 
// for more functionalities.
// const char* mqtt_server = "test.mosquitto.org";

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
static char msg[1024];
int value = 0;

extern void wdtSetting(String strSetting);
extern void rebootingCallback(String strcallback);
extern String FirmVer;

bool mqttconnect() 
{
	bool bResult = true;
	// Loop until we're reconnected
	if (!client.connected() && WiFi.status() == WL_CONNECTED) 
	{
		Serial.print("Attempting MQTT connection...");
		// Create a random client ID
		String clientId = "ESP8266Client-";
		clientId += String(random(0xffff), HEX);
		
		// Attempt to connect
		StaticJsonBuffer<512> jsonBuffer;
		JsonObject& root = jsonBuffer.createObject();

		root["devid"] = DeviceID;
		root["localip"] = WiFi.localIP().toString();
		root["actiontype"] = "disconnect";
		root["status"] = "unreachable";
		root["message"] = "device is disconnected from mqtt broker!";
		root["firmver"] = FirmVer;

		String lwtMessage; 
		root.printTo(lwtMessage);

		//if (client.connect(clientId.c_str(), "/remotepower/logging", 0, false, lwtMessage.c_str()))
		if (client.connect(clientId.c_str()))
		{
			Serial.println("Mqtt connected!");
			// ... and resubscribe
			client.subscribe("/remotepower/setting");
		}
		else 
		{
			Serial.print("failed, rc=");
			Serial.print(client.state());
			bResult = false;
			delay(2000);
		}
	}
	return bResult;
}

void callback(char* topic, byte* payload, unsigned int length) 
{
	Serial.print("Message arrived [");
	Serial.print(topic);
	Serial.print("] ");

	String packetJson = "";
	for (uint8_t idx = 0; idx < length; idx++)
		packetJson += (char)payload[idx];
	rebootingCallback(packetJson);
}
