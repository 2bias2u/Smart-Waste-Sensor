#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h> 
#include "HX711.h"



// Pins Waage
const int LOADCELL_DOUT_PIN = 32;
const int LOADCELL_SCK_PIN = 33;

// Pins Ultraschall 1
const int TRIG1 = 25;
const int ECHO1 = 26;

// Pins Ultraschall 2
const int TRIG2 = 14;
const int ECHO2 = 27;

HX711 scale;

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

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  // scale.set_scale(DEIN_FAKTOR); // Hier später deinen Kalibrierfaktor rein!
  scale.tare();
  
  Serial.println("System mit 2 Ultraschall-Sensoren bereit!");
}

void loop() {
  // 1. Beide Distanzen messen
  float d1 = getDistance(TRIG1, ECHO1);
  delay(50); // Kleiner Abstand, damit sich die Schallwellen nicht stören
  float d2 = getDistance(TRIG2, ECHO2);

  // 2. Minimum ermitteln
  float minDistance = min(d1, d2);

  // 3. Gewicht messen
  long weight = 0;
  if (scale.is_ready()) {
    weight = scale.get_units(5);
  }

  // 4. Ausgabe
  Serial.print("S1: "); Serial.print(d1);
  Serial.print("cm | S2: "); Serial.print(d2);
  Serial.print("cm | MIN: "); Serial.print(minDistance);
  Serial.print("cm | Gewicht: "); Serial.println(weight);

  delay(1000);
}