/*
Nach dem Flashen ein bis zwei Minuten warten, bis sich der HX711 thermisch eingelaufen hat — sonst wandert der Nullpunkt danach noch weg. Dann Waage leer lassen und t senden. Anschließend das bekannte Gewicht auflegen und c mit dem Gewicht in Gramm senden, bei 1 kg also c1000. Ab dem Moment zeigt die Ausgabe Gramm.
Im seriellen Monitor unten „Neue Zeile" oder „NL & CR" einstellen, sonst kommt kein Befehl an.
*/

#include "HX711.h"
#include <Preferences.h>

const int DT_PIN  = 18;
const int SCK_PIN = 19;

HX711 scale;
Preferences prefs;
float faktor = 0.0f;   // 0 = noch nicht kalibriert

void hilfe() {
  Serial.println("---------------------------------------");
  Serial.println(" t        Tarieren (Nullpunkt setzen)");
  Serial.println(" c<g>     Kalibrieren, z.B. c1000");
  Serial.println(" i        Infos anzeigen");
  Serial.println(" x        Kalibrierung loeschen");
  Serial.println("---------------------------------------");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  scale.begin(DT_PIN, SCK_PIN);
  if (!scale.wait_ready_timeout(3000)) {
    Serial.println("HX711 nicht gefunden - Verkabelung pruefen");
    while (1) delay(1000);
  }

  prefs.begin("waage", false);
  faktor      = prefs.getFloat("faktor", 0.0f);
  long offset = prefs.getLong("offset", 0);

  if (faktor != 0.0f) {
    scale.set_scale(faktor);
    scale.set_offset(offset);
    Serial.printf("Kalibrierung geladen: Faktor %.3f\n", faktor);
  } else {
    Serial.println("Noch nicht kalibriert.");
    Serial.println("1) Waage leeren, 't' senden");
    Serial.println("2) Bekanntes Gewicht auflegen, z.B. 'c1000'");
    scale.tare(20);
  }
  hilfe();
}

void befehl(String s) {
  s.trim();
  if (s.length() == 0) return;

  if (s == "t") {
    scale.tare(20);
    prefs.putLong("offset", scale.get_offset());
    Serial.println("Tariert und gespeichert.");
  }
  else if (s.startsWith("c")) {
    float gramm = s.substring(1).toFloat();
    if (gramm <= 0) { Serial.println("Ungueltig, z.B. c1000"); return; }

    float roh = scale.get_value(20);       // Rohwert abzueglich Offset
    if (abs(roh) < 1000) {
      Serial.println("Kaum Signal - liegt das Gewicht wirklich auf?");
      return;
    }
    faktor = roh / gramm;
    scale.set_scale(faktor);
    prefs.putFloat("faktor", faktor);
    Serial.printf("Kalibriert: Faktor %.3f gespeichert.\n", faktor);
  }
  else if (s == "i") {
    Serial.printf("Faktor %.3f | Offset %ld\n", faktor, scale.get_offset());
  }
  else if (s == "x") {
    prefs.clear();
    faktor = 0.0f;
    scale.set_scale();
    Serial.println("Kalibrierung geloescht. Neu starten.");
  }
  else hilfe();
}

void loop() {
  if (Serial.available()) befehl(Serial.readStringUntil('\n'));

  static unsigned long t = 0;
  if (millis() - t > 1000) {
    t = millis();
    if (faktor != 0.0f) Serial.printf("%.0f g\n", scale.get_units(10));
    else                Serial.printf("roh: %.0f\n", scale.get_value(10));
  }
}