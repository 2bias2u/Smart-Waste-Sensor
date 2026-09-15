#include "HX711.h"
#include <Preferences.h>

const int DT_PIN   = 18;
const int SCK_PIN  = 19;
const unsigned long INTERVALL = 2000;   // 2 s, spaeter hochsetzen
const int MITTELUNG = 10;

HX711 scale;
Preferences prefs;
float faktor = 0.0f;                    // Counts pro Gramm

long refRoh = 0;
bool refGesetzt = false;
float startwert = 0, minwert = 0, maxwert = 0;
unsigned long t0 = 0, tLetzte = 0;
bool ersteMessung = true;

void neuStarten() {
  ersteMessung = true;
  t0 = millis();
  tLetzte = millis() - INTERVALL;
  Serial.println("\n Min |     kg |   Delta |     Min |     Max");
  Serial.println("-----+--------+---------+---------+--------");
}

void hilfe() {
  Serial.println("  t        tarieren (Nullpunkt)");
  Serial.println("  m        Referenz merken, dann Gewicht zulegen");
  Serial.println("  c<g>     kalibrieren, z.B. c10000");
  Serial.println("  i        Infos      r  Log neu starten");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  scale.begin(DT_PIN, SCK_PIN);
  if (!scale.wait_ready_timeout(3000)) {
    Serial.println("HX711 nicht gefunden");
    while (1) delay(1000);
  }

  prefs.begin("waage2", false);
  faktor = prefs.getFloat("faktor", 0.0f);
  scale.set_offset(prefs.getLong("offset", 0));
  if (faktor != 0.0f) scale.set_scale(faktor);

  Serial.printf("\nFaktor %.4f\n", faktor);
  hilfe();
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
  else if (s == "m") {
    refRoh = scale.read_average(20);
    refGesetzt = true;
    Serial.printf("Referenz: %ld -- jetzt Gewicht zulegen, dann c<gramm>\n", refRoh);
  }
  else if (s.startsWith("c")) {
    float gramm = s.substring(1).toFloat();
    if (gramm <= 0) { Serial.println("Ungueltig, z.B. c10000"); return; }

    long diff;
    if (refGesetzt) diff = scale.read_average(20) - refRoh;
    else            diff = (long)scale.get_value(20);

    if (abs(diff) < 200) { Serial.println("Kaum Aenderung - Gewicht aufgelegt?"); return; }

    faktor = (float)diff / gramm;
    scale.set_scale(faktor);
    prefs.putFloat("faktor", faktor);
    Serial.printf("Kalibriert: %ld Counts / %.0f g -> Faktor %.4f\n", diff, gramm, faktor);
    refGesetzt = false;
    neuStarten();
  }
  else if (s == "i") {
    Serial.printf("Faktor %.4f | Offset %ld\n", faktor, scale.get_offset());
  }
  else if (s == "r") neuStarten();
  else hilfe();
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

  Serial.printf("%4lu | %6.2f | %+7.2f | %7.2f | %7.2f\n",
                (millis() - t0) / 60000, kg, kg - startwert, minwert, maxwert);
}