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

uint32_t calculate_CRC32(const uint8_t *data, size_t length) {
  uint32_t crc = 0xffffffff;
  while (length--) {
    uint8_t c = *data++;
    for (uint32_t i = 0x80; i > 0; i >>= 1) {
      bool bit = crc & 0x80000000;
      if (c & i) {
        bit = !bit;
      }
      crc <<= 1;
      if (bit) {
        crc ^= 0x04c11db7;
      }
    }
  }
  return crc;
}
