#include "Secrets.h"
#include "Green.h"

// WiFi
const char *ssid = SECRET_WIFI_SSID;
const char *password = SECRET_WIFI_PASSWORD;

// MQTT Broker
const char *mqtt_broker = SECRET_MQTT_BROKER;
const char *mqtt_username = SECRET_MQTT_USERNAME;
const char *mqtt_password = SECRET_MQTT_PASSWORD;
const int mqtt_port = SECRET_MQTT_PORT;

void connect(WiFiClient &net, MQTTClient &client, String &node_id, bool clean_session) {
  // Connecting to WiFi network
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  // Connecting to a mqtt broker
  client.begin(mqtt_broker, mqtt_port, net);
  client.setCleanSession(clean_session);
  while (!client.connect(node_id.c_str(), mqtt_username, mqtt_password)) {
    delay(2000);
  }
}
