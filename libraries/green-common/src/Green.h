#ifndef Green_h
#define Green_h

#include <MQTT.h>
#include <ESP8266WiFi.h>

void connect(WiFiClient &net, MQTTClient &client, String &node_id, bool clean_session);

#endif
