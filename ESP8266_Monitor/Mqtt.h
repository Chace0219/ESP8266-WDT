#pragma once

#ifndef MQTT_H
#define MQTT_H

#include <ESP8266WiFi.h>
#include "PubSubClient.h"

extern PubSubClient client;
extern const char* mqtt_server;
bool mqttconnect();
void callback(char* topic, byte* payload, unsigned int length);

extern String DeviceID;
#endif // MQTT_H
