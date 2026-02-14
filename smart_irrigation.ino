/*
 * ESP32 Smart Irrigation Monitor
 */

#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

// Učitaj konfiguraciju iz eksternog fajla
#include "config.h"

// Pinovi
#define MOISTURE_PIN 34
#define SERVO_PIN 15
#define LED_GREEN 18
#define LED_RED 19

// Pragovi
const int DRY_THRESHOLD = 20;
const int WET_THRESHOLD = 30;

// Objekti
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo myServo;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Globalne varijable
int moisturePercent = 0;
String currentState = "LOADING";
int currentServoPos = 90;
bool wateringActive = false;

// Kontrola
bool manualMode = false;
bool manualWaterOn = false;

// Timeri
unsigned long lastFirebaseSend = 0;
unsigned long lastSensorRead = 0;
unsigned long lastCommandCheck = 0;
unsigned long lastLEDBlink = 0;
bool ledState = false;
int blinkInterval = 500;


void setup() {
  Serial.begin(9600);
  delay(100);
  
  Serial.println("\n=== ESP32 Irrigation Monitor ===\n");
  
  // LED
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED, LOW);
  
  // Servo
  myServo.attach(SERVO_PIN);
  myServo.write(90);
  currentServoPos = 90;
  
  // LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Pokretanje...");
  delay(2000);
  
  // WiFi
  Serial.print("WiFi spajanje...");
  lcd.clear();
  lcd.print("WiFi...");
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi spojen!");
    Serial.println(WiFi.localIP());
    lcd.clear();
    lcd.print("WiFi OK!");
    delay(1000);
  }
  
  // Firebase
  Serial.println("Firebase...");
  lcd.clear();
  lcd.print("Firebase...");
  
  config.database_url = FIREBASE_HOST;
  config.signer.test_mode = true;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  delay(2000);
  
  if (Firebase.ready()) {
    Serial.println("Firebase OK!");
    lcd.clear();
    lcd.print("Firebase OK!");
    Firebase.RTDB.setString(&fbdo, "/devices/" + String(DEVICE_ID) + "/status", "online");
  }
  
  delay(1000);
  Serial.println("\nSistem spreman!\n");
}


void loop() {
  handleLEDBlink();
  
  // Ocitaj senzor svaki 2s
  if (millis() - lastSensorRead >= 2000) {
    readSensor();
    lastSensorRead = millis();
  }
  
  // Provjeri Firebase komande svaki 2s
  if (millis() - lastCommandCheck >= 2000) {
    checkFirebaseCommands();
    lastCommandCheck = millis();
  }
  
  // Salji u Firebase svaki 2s
  if (Firebase.ready() && millis() - lastFirebaseSend >= 2000) {
    sendToFirebase();
    lastFirebaseSend = millis();
  }
}


void readSensor() {
  int rawValue = analogRead(MOISTURE_PIN);
  moisturePercent = map(rawValue, 4095, 0, 0, 100);
  moisturePercent = constrain(moisturePercent, 0, 100);
  
  Serial.print("Vlaznost: ");
  Serial.print(moisturePercent);
  Serial.println("%");
  
  // LCD prikaz
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Vlaga: ");
  lcd.print(moisturePercent);
  lcd.print("%");
  
  // Odluka prema modu
  if (manualMode) {
    handleManualMode();
  } else {
    handleAutoMode();
  }
}


void handleAutoMode() {
  Serial.println("Mode: AUTO");
  
  if (moisturePercent < DRY_THRESHOLD) {
    currentState = "DRY";
    blinkInterval = 200;
    wateringActive = true;
    
    lcd.setCursor(0, 1);
    lcd.print("AUTO: SUHO    ");
    
    moveServo(180);
    
  } else if (moisturePercent <= WET_THRESHOLD) {
    currentState = "OPTIMAL";
    blinkInterval = 500;
    wateringActive = false;
    
    lcd.setCursor(0, 1);
    lcd.print("AUTO: OPTIMAL ");
    
    moveServo(90);
    
  } else {
    currentState = "WET";
    blinkInterval = 800;
    wateringActive = false;
    
    lcd.setCursor(0, 1);
    lcd.print("AUTO: VLAZNO  ");
    
    moveServo(0);
  }
}


void handleManualMode() {
  Serial.println("Mode: MANUAL");
  currentState = "MANUAL";
  blinkInterval = 300;
  
  // Safety check
  if (manualWaterOn && moisturePercent > 80) {
    Serial.println("UPOZORENJE: Previse vlazno!");
    manualWaterOn = false;
    Firebase.RTDB.setBool(&fbdo, "/devices/" + String(DEVICE_ID) + "/commands/manualWater", false);
    Firebase.RTDB.setString(&fbdo, "/devices/" + String(DEVICE_ID) + "/warning", "Vlaznost previsoka!");
  }
  
  if (manualWaterOn) {
    wateringActive = true;
    lcd.setCursor(0, 1);
    lcd.print("MANUAL: ON    ");
    moveServo(180);
  } else {
    wateringActive = false;
    lcd.setCursor(0, 1);
    lcd.print("MANUAL: OFF   ");
    moveServo(90);
  }
}


void checkFirebaseCommands() {
  if (!Firebase.ready()) return;
  
  String path = "/devices/" + String(DEVICE_ID) + "/commands";
  
  // Citaj manual mode
  if (Firebase.RTDB.getBool(&fbdo, path + "/manualMode")) {
    manualMode = fbdo.boolData();
  }
  
  // Citaj manual water
  if (Firebase.RTDB.getBool(&fbdo, path + "/manualWater")) {
    manualWaterOn = fbdo.boolData();
  }
}


void moveServo(int target) {
  if (currentServoPos == target) return;
  
  if (currentServoPos < target) {
    for (int pos = currentServoPos; pos <= target; pos += 3) {
      myServo.write(pos);
      delay(30);
    }
  } else {
    for (int pos = currentServoPos; pos >= target; pos -= 3) {
      myServo.write(pos);
      delay(30);
    }
  }
  
  myServo.write(target);
  currentServoPos = target;
}


void handleLEDBlink() {
  if (millis() - lastLEDBlink >= blinkInterval) {
    ledState = !ledState;
    
    if (currentState == "DRY" || (manualMode && manualWaterOn)) {
      digitalWrite(LED_RED, ledState ? HIGH : LOW);
      digitalWrite(LED_GREEN, LOW);
    } else {
      digitalWrite(LED_GREEN, ledState ? HIGH : LOW);
      digitalWrite(LED_RED, LOW);
    }
    
    lastLEDBlink = millis();
  }
}


void sendToFirebase() {
  if (!Firebase.ready()) return;
  
  String path = "/devices/" + String(DEVICE_ID);
  
  Firebase.RTDB.setInt(&fbdo, path + "/currentData/moisture", moisturePercent);
  Firebase.RTDB.setString(&fbdo, path + "/currentData/state", currentState);
  Firebase.RTDB.setInt(&fbdo, path + "/currentData/servoPosition", currentServoPos);
  Firebase.RTDB.setBool(&fbdo, path + "/currentData/wateringActive", wateringActive);
  Firebase.RTDB.setBool(&fbdo, path + "/currentData/manualMode", manualMode);
  Firebase.RTDB.setString(&fbdo, path + "/currentData/timestamp", String(millis()));
  Firebase.RTDB.setString(&fbdo, path + "/status", "online");
  
  // History
  String histPath = path + "/history/" + String(millis());
  Firebase.RTDB.setInt(&fbdo, histPath + "/moisture", moisturePercent);
  Firebase.RTDB.setString(&fbdo, histPath + "/state", currentState);
  Firebase.RTDB.setInt(&fbdo, histPath + "/servoPosition", currentServoPos);
  
  Serial.println("Firebase azuriran");
}
