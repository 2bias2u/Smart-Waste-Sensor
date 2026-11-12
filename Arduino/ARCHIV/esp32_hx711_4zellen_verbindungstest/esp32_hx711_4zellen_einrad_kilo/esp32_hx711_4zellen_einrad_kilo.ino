#include "HX711.h"
#include <Preferences.h>

const int DT_PIN   = 18;
const int SCK_PIN  = 19;
const unsigned long INTERVALL = 30000;   // 30 s
const int  MITTELUNG = 20;
const float HOCHRECHNUNG = 4.0f;         // Anzahl Raeder

HX711 scale;
Preferences prefs;
float faktor = 0.0f;                     // Counts pro GRAMM

float startwert = 0, minwert = 0, maxwert = 0;
unsigned long t0 = 0, tLetzte = 0;
bool ersteMessung = true;

void neuStarten() {
  ersteMessung = true;
  t0 = millis();
  tLetzte = millis() - INTERVALL;
  Serial.println("\n Min |    Rad |  gesamt |   Delta |     Min |     Max   [kg]");
  Serial.println("-----+--------+---------+---------+---------+--------");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  scale.begin(DT_PIN, SCK_PIN);
  if (!scale.wait_ready_timeout(3000)) {
    Serial.println("HX711 nicht gefunden");
    while (1) delay(1000);
  }

  prefs.begin("waage", false);
  faktor = prefs.getFloat("faktor", 0.0f);
  scale.set_offset(prefs.getLong("offset", 0));
  if (faktor != 0.0f) scale.set_scale(faktor);

  Serial.printf("Faktor %.3f | t=tarieren  c<g>=kalibrieren  r=neu\n", faktor);
  neuStarten();
}

void befehl(String s) {
  s.trim();
  if (s.length() == 0) return;

  if (s == "t") {
    scale.tare(20);
    prefs.putLong("offset", scale.get_offset());
    Serial.println("Tariert.");
    neuStarten();
  }
  else if (s.startsWith("c")) {
    float gramm = s.substring(1).toFloat();
    if (gramm <= 0) { Serial.println("Ungueltig, z.B. c1750"); return; }
    float roh = scale.get_value(20);
    if (abs(roh) < 1000) { Serial.println("Kaum Signal - Gewicht aufgelegt?"); return; }
    faktor = roh / gramm;
    scale.set_scale(faktor);
    prefs.putFloat("faktor", faktor);
    Serial.printf("Kalibriert: Faktor %.3f\n", faktor);
    neuStarten();
  }
  else if (s == "r") neuStarten();
  else Serial.println("t / c<gramm> / r");
}

void loop() {
  static String puffer = "";
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (puffer.length() > 0) { befehl(puffer); puffer = ""; }
    } else puffer += c;
  }

  if (millis() - tLetzte < INTERVALL) return;
  tLetzte = millis();

  if (faktor == 0.0f) {
    Serial.printf("roh: %.0f  (nicht kalibriert)\n", scale.get_value(10));
    return;
  }

  float kg = scale.get_units(MITTELUNG) / 1000.0f;

  if (ersteMessung) { startwert = minwert = maxwert = kg; ersteMessung = false; }
  if (kg < minwert) minwert = kg;
  if (kg > maxwert) maxwert = kg;

  Serial.printf("%4lu | %6.2f | %7.2f | %+7.2f | %7.2f | %7.2f\n",
                (millis() - t0) / 60000, kg, kg * HOCHRECHNUNG,
                kg - startwert, minwert, maxwert);
}