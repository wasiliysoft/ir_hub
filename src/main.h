#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <EEPROM.h>  // епром
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>

// ------------------- ПАРАМЕТРЫ --------------------
#define HTTP_PORT 80
#define UDP_PORT 55531
#define LED_PIN 2
#define IR_PIN 5
#define BTN_PIN 0

#define SP_AP_NAME "IrHUB"       // название точки
#define SP_AP_IP 192, 168, 1, 1  // IP точки

// ------------------- СТРУКТУРЫ --------------------
struct WiFiCfg {
  char SSID[32] = "";
  char pass[32] = "";
  uint8_t mode = WIFI_STA;  // (1 WIFI_STA, 2 WIFI_AP)
};

// ------------------- ФУНКЦИИ --------------------

void UDPTick();
bool startWiFi();
bool setupLocal();
bool setupAP();
void restartHTTP();
void restartUDP();
/* Запускает память и загружает конфигурацию */
void EE_startup();
void EE_save();

void portalStart();
void portalTick();
void portalBtnTick();

// ------------------- ГЛОБАЬНЫЕ ПЕРЕМЕННЫЕ --------------------
WiFiCfg cfg;
IRsend irsend(IR_PIN);
ESP8266WebServer server(HTTP_PORT);
DNSServer dnsServer;
WiFiUDP udpServer;
char packetBuffer[UDP_TX_PACKET_MAX_SIZE + 1];

bool SP_started = false;

// ------------------- МАКРО --------------------
#define LED(x) digitalWrite(LED_PIN, !x);
