#include <MQTT.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <Secrets.h>

// WiFi
const char *ssid = SECRET_WIFI_SSID;
const char *password = SECRET_WIFI_PASSWORD;

// MQTT Broker
const char *mqtt_broker = SECRET_MQTT_BROKER;
const char *mqtt_username = SECRET_MQTT_USERNAME;
const char *mqtt_password = SECRET_MQTT_PASSWORD;
const int mqtt_port = SECRET_MQTT_PORT;

// Pins
const int moisture_pin = A0; // Soil Moisture Sensor input at Analog PIN A0
const int relay_pin = 5; // Pump relay output at GPIO 5

// Control values
const String firmware_version = "1";
const double sleep_period = 3600e6; // 1 hour in microseconds
const int pump_interval = 1000; // Pumping will be done in 1 second intervals

// Global variables
WiFiClient net;
MQTTClient client(1024);
DynamicJsonDocument doc(1024);
String node_id = "";
String settings_topic = "";
int moisture_value = 0;
bool is_open = false;

// Structure which will be stored in RTC memory.
// First field is CRC32, which is calculated based on the
// rest of structure contents.
struct {
  uint32_t crc32;
  int min_moisture;
  int max_moisture;
  int max_pumping_time;
} rtc_data;

void setup() {
  // Load rtc_data from RTC memory
  read_rtc_data();

  // Set pins
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(relay_pin, OUTPUT); // enable relay pin for output

  // Turn LED off
  digitalWrite(LED_BUILTIN, HIGH);

  // Set node_id and node topics
  node_id = String(ESP.getChipId());
  settings_topic = "green/settings/" + node_id;

  // Connect to WiFi and MQTT broker
  connect();

  // Check for updates and apply them
  //update_firmware(firmware_version);

  // Subscribe to topics
  client.subscribe(settings_topic, 2);
  delay(500);

  // Run MQTT loop
  client.loop();
  delay(500);

  // Read moisture sensor
  update_moisture();
  delay(500);

  // Soil is dry. Pump water!
  if (moisture_value < rtc_data.min_moisture) {
    pump();
  }

  // Save rtc_data into RTC memory
  write_rtc_data();

  // Unsubscribe from topics for getting retained messages next time
  client.unsubscribe(settings_topic);

  // Get into next deep sleep cycle
  ESP.deepSleep(sleep_period);
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

void callback(String &topic, String &payload) {
  try {
    deserializeJson(doc, payload);

    if (topic == settings_topic) {
      rtc_data.min_moisture = doc.containsKey("min_moisture") ? doc["min_moisture"] : rtc_data.min_moisture;
      rtc_data.max_moisture = doc.containsKey("max_moisture") ? doc["max_moisture"] : rtc_data.max_moisture;
      rtc_data.max_pumping_time = doc.containsKey("max_pumping_time") ? doc["max_pumping_time"] : rtc_data.max_pumping_time;
    }
  } catch (...) {}
}

void connect() {
  // Connecting to WiFi network
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  // Connecting to a mqtt broker
  client.begin(mqtt_broker, mqtt_port, net);
  client.onMessage(callback);
  client.setCleanSession(false);
  while (!client.connect(node_id.c_str(), mqtt_username, mqtt_password)) {
    delay(2000);
  }
}

void pump() {
  // Turn the pump on
  digitalWrite(relay_pin, HIGH);

  // Set status to pumping
  is_open = true;

  // Wait until soil is wet enough
  int times = 0;
  while (moisture_value < rtc_data.max_moisture) {
    client.loop();
    delay(pump_interval);
    update_moisture();
    times++;

    // SECURITY MEASURE: Turn the system off if it has been pumping for too long
    if (times * pump_interval > rtc_data.max_pumping_time * 1000) { // comparison is done in milliseconds
      // Set status to idle
      is_open = false;
      update_moisture(); // Just to send new status
      ESP.deepSleep(0); // Sleep forever
    }
  }

  // Turn the pump off
  digitalWrite(relay_pin, LOW);

  // Set status to idle
  is_open = false;
}

void read_rtc_data() {
  if (ESP.rtcUserMemoryRead(0, (uint32_t*) &rtc_data, sizeof(rtc_data))) {
    uint32_t expected_crc = rtc_data.crc32;
    rtc_data.crc32 = 0;
    uint32_t crc_of_data = calculate_CRC32((uint8_t*) &rtc_data, sizeof(rtc_data));

    if (crc_of_data != expected_crc) {
      // CRC32 in RTC memory doesn't match CRC32 of data. Data is probably invalid!
      // Set default values
      rtc_data.min_moisture = -480;
      rtc_data.max_moisture = -285;
      rtc_data.max_pumping_time = 30; // in seconds
    }
  }
}

void update_moisture() {
  // Get value from sensor
  moisture_value = -analogRead(moisture_pin);

  // Report moisture and node parameters
  doc.clear();

  doc["node_id"] = node_id;
  doc["moisture"] = moisture_value;
  doc["min_moisture"] = rtc_data.min_moisture;
  doc["max_moisture"] = rtc_data.max_moisture;
  doc["max_pumping_time"] = rtc_data.max_pumping_time;
  doc["is_open"] = is_open;
  doc["version"] = firmware_version;

  String payload = "";
  serializeJson(doc, payload);
  client.publish("green/record", payload);
}

void write_rtc_data() {
  // Update CRC32 of data
  rtc_data.crc32 = 0;
  rtc_data.crc32 = calculate_CRC32((uint8_t*) &rtc_data, sizeof(rtc_data));

  // Write struct to RTC memory
  ESP.rtcUserMemoryWrite(0, (uint32_t*) &rtc_data, sizeof(rtc_data));
}

void loop() {}
