/*
 * Konfiguracijski fajl za ESP32 Smart Irrigation Monitor
 * 
 * UPUTE:
 * 1. Kopiraj ovaj fajl i preimenuj ga u "config.h"
 * 2. Popuni svoje podatke ispod
 */

#ifndef CONFIG_H
#define CONFIG_H

// WiFi kredencijali - zamijeni sa svojim podacima
#define WIFI_SSID "ime_tvoje_wifi_mreze"
#define WIFI_PASSWORD "lozinka_wifi_mreze"

// Firebase konfiguracija - zamijeni sa URL-om tvoje Firebase baze
#define FIREBASE_HOST "tvoj-projekt-default-rtdb.europe-west1.firebasedatabase.app"

// Device ID - jedinstveni identifikator za tvoj ESP32
#define DEVICE_ID "ESP32-001"

#endif
