#include <MQTT.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <Secrets.h>
#include <Green.h>

// Pins
const int moisture_pin = A0; // Soil Moisture Sensor input at Analog PIN A0
const int relay_pin = 5; // Pump relay output at GPIO 5

// Control values
const String node_type = "0";
const String firmware_version = "1";
const double sleep_period = 3600e6; // 1 hour in microseconds
const int pump_interval = 500; // Pumping will be done in 1 second intervals (this + update_moisture delay)

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
  int max_open_time;
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
  connect(net, client, node_id, false);

  // Check for updates and apply them
  update_firmware(node_type, firmware_version);

  // Set callback and subscribe to topics
  client.onMessage(callback);
  client.subscribe(settings_topic, 2);
  delay(500);

  // Run MQTT loop
  client.loop();
  delay(500);

  // Read moisture sensor
  update_moisture();

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

void callback(String &topic, String &payload) {
  try {
    deserializeJson(doc, payload);

    if (topic == settings_topic) {
      rtc_data.min_moisture = doc.containsKey("min_moisture") ? doc["min_moisture"] : rtc_data.min_moisture;
      rtc_data.max_moisture = doc.containsKey("max_moisture") ? doc["max_moisture"] : rtc_data.max_moisture;
      rtc_data.max_open_time = doc.containsKey("max_open_time") ? doc["max_open_time"] : rtc_data.max_open_time;
    }
  } catch (...) {}
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
    if (times * pump_interval > rtc_data.max_open_time) {
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
      rtc_data.min_moisture = -10001;
      rtc_data.max_moisture = -10000;
      rtc_data.max_open_time = 10000; // in milliseconds
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
  doc["max_open_time"] = rtc_data.max_open_time;
  doc["is_open"] = is_open;
  doc["node_type"] = node_type;
  doc["version"] = firmware_version;

  String payload = "";
  serializeJson(doc, payload);
  client.publish("green/record", payload);
  delay(500);
}

void write_rtc_data() {
  // Update CRC32 of data
  rtc_data.crc32 = 0;
  rtc_data.crc32 = calculate_CRC32((uint8_t*) &rtc_data, sizeof(rtc_data));

  // Write struct to RTC memory
  ESP.rtcUserMemoryWrite(0, (uint32_t*) &rtc_data, sizeof(rtc_data));
}

void loop() {}
