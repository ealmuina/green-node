#include <MQTT.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <Secrets.h>
#include <Green.h>

// Pins
const int moisture_pin = A0; // Soil Moisture Sensor input at Analog PIN A0
const int relay_pin = 5; // Pump relay output at GPIO 5

// Control values
const String firmware_version = "1";
const double sleep_period = 3600e6; // 1 hour in microseconds
const int pump_interval = 1000; // Pumping will be done in 1 second intervals
const int max_pumping_time = 60000;

// Global variables
WiFiClient net;
MQTTClient client(1024);
DynamicJsonDocument doc(1024);
String node_id = "";
String settings_topic = "";
int moisture_value = 0;
bool is_open = false;

void setup() {
  // Set pins
  pinMode(relay_pin, OUTPUT); // enable relay pin for output

  // Set node_id and node topics
  node_id = String(ESP.getChipId());
  settings_topic = "green/settings/" + node_id;

  // Connect to WiFi and MQTT broker
  connect(net, client, node_id, false);

  // Check for updates and apply them
  //update_firmware(firmware_version);

  // Set callback and subscribe to topics
  client.onMessage(callback);
  client.subscribe(settings_topic, 2);
  delay(500);

  // Read moisture sensor
  update_moisture();

  // Run MQTT loop
  client.loop();
  delay(500);

  if (is_open) {
    irrigate();
  }

  // Get into next deep sleep cycle
  ESP.deepSleep(sleep_period);
}

void callback(String &topic, String &payload) {
  try {
    deserializeJson(doc, payload);

    if (topic == settings_topic) {
      is_open = doc.containsKey("is_open") ? doc["is_open"] : false;
    }

    doc.clear();

  } catch (...) {}
}

void update_moisture() {
  // Get value from sensor
  moisture_value = -analogRead(moisture_pin);

  // Report moisture and node parameters
  doc.clear();

  doc["node_id"] = node_id;
  doc["moisture"] = moisture_value;
  doc["is_open"] = is_open;
  doc["version"] = firmware_version;

  String payload = "";
  serializeJson(doc, payload);
  client.publish("green/record", payload);
  delay(500);
}

void irrigate() {
  // Open valve
  digitalWrite(relay_pin, HIGH);

  // SECURITY MEASURE: Turn the system off if it has been pumping for too long
  int times = 0;
  while (is_open && times * pump_interval < max_pumping_time) {
    client.loop();
    delay(pump_interval);
    times++;
  }

  // Close the valve
  digitalWrite(relay_pin, LOW);
  is_open = false;
}

void loop() {}
