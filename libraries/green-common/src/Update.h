#ifndef Update_h
#define Update_h

#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecure.h>
#include <CertStoreBearSSL.h>
#include <time.h>

void set_clock();

void update_firmware(String node_type, String firmware_version);

#endif
