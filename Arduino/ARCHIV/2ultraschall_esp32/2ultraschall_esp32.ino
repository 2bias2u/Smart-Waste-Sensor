#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h> 

// Pins Ultraschall 1
const int TRIG1 = 25;
const int ECHO1 = 26;

// Pins Ultraschall 2
const int TRIG2 = 14;
const int ECHO2 = 27;

// Funktion zum Messen der Distanz
float getDistance(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long duration = pulseIn(echo, HIGH, 30000); // Timeout nach 30ms
  if (duration == 0) return 400.0;           // Falls kein Echo, max Distanz
  return duration * 0.034 / 2;
}

void setup() {
  Serial.begin(115200);
  
  pinMode(TRIG1, OUTPUT); pinMode(ECHO1, INPUT);
  pinMode(TRIG2, OUTPUT); pinMode(ECHO2, INPUT);
  
  Serial.println("System mit 2 Ultraschall-Sensoren bereit!");
}

void loop() {
  // 1. Beide Distanzen messen
  float d1 = getDistance(TRIG1, ECHO1);
  delay(50); // Kleiner Abstand, damit sich die Schallwellen nicht stören
  float d2 = getDistance(TRIG2, ECHO2);

  // 2. Minimum ermitteln
  float minDistance = min(d1, d2);

  // 3. Ausgabe
  Serial.print("S1: "); Serial.print(d1);
  Serial.print("cm | S2: "); Serial.print(d2);
  Serial.print("cm | MIN: "); Serial.print(minDistance);
  Serial.println("cm");

  delay(1000);
}