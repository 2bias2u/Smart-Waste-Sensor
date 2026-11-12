#include "HX711.h"

/*
Sensordrähte an den HX711:
Sensor 1 Weiß → E+
Sensor 2 Weiß → E-
Sensor 1 Rot → A-
Sensor 2 Rot → A+
*/

/*
 * Wiegezellen-Sketch für den ESP32 (WROOM-32) - rein lokal, ohne WLAN/LoRaWAN
 *
 * Funktionsweise:
 * 1. Beim Start: Tara wird automatisch gesetzt (kein Gewicht auflegen)
 * 2. Danach: du legst EINMAL ein bekanntes Referenzgewicht auf
 * 3. Der Code berechnet den Kalibrierungsfaktor SELBST daraus
 * 4. Ab dann werden die echten Gewichtswerte in Gramm laufend im
 *    Serial Monitor angezeigt
 *
 * WICHTIG: Die Kalibrierung läuft bei JEDEM Neustart des ESP32 erneut ab.
 * Das heißt: nach jedem Reset/Einschalten musst du kurz das Referenz-
 * gewicht auflegen, wenn die Aufforderung im Serial Monitor erscheint.
 */

// ==== HX711 Pin-Belegung (unverändert, wie verkabelt) ====
const int HX711_DT  = 33;
const int HX711_SCK = 25;

// ========================================================
// >>> HIER dein bekanntes Referenzgewicht eintragen (in Gramm) <<<
// Das ist das Gewicht, das du beim Start auf die Wiegezelle legst,
// damit der Code sich selbst kalibrieren kann.
// ========================================================
float KNOWN_WEIGHT_GRAMS = 500.0; // <-- ANPASSEN, z.B. auf 1000.0 für 1kg

// ========================================================
// >>> HIER das Gewicht einer VOLLEN Tonne eintragen (in Gramm) <<<
// Wird genutzt, um aus dem gemessenen Gewicht einen Füllstand
// in Prozent zu berechnen (Gewicht / Vollgewicht * 100).
// ========================================================
float MAX_CAPACITY_GRAMS = 10000.0; // <-- ANPASSEN, z.B. auf euer erwartetes Vollgewicht

HX711 scale;
bool calibrated = false;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("=== Wiegezellen-Sketch mit Selbstkalibrierung (lokal, ohne WLAN) ===");

  scale.begin(HX711_DT, HX711_SCK);

  Serial.println("Warte auf HX711...");
  unsigned long start = millis();
  while (!scale.is_ready()) {
    delay(100);
    if (millis() - start > 5000) {
      Serial.println("FEHLER: HX711 antwortet nicht. Verkabelung pruefen!");
      break;
    }
  }

  if (scale.is_ready()) {
    // Schritt 1: Tara (Nullpunkt) setzen
    Serial.println();
    Serial.println(">>> Bitte JETZT KEIN Gewicht auflegen - Tara wird in 3 Sekunden gesetzt <<<");
    delay(3000);
    scale.set_scale(); // Faktor 1, fuer rohe Messung
    scale.tare(20);
    Serial.println("Tara gesetzt.");

    // Schritt 2: Referenzgewicht auflegen und automatisch kalibrieren
    Serial.println();
    Serial.print(">>> Bitte jetzt dein Referenzgewicht (");
    Serial.print(KNOWN_WEIGHT_GRAMS, 1);
    Serial.println(" g) auflegen. Kalibrierung startet in 5 Sekunden... <<<");
    delay(5000);

    long rawValue = scale.get_value(20); // gemittelter Rohwert MIT Gewicht drauf
    float factor = rawValue / KNOWN_WEIGHT_GRAMS;
    scale.set_scale(factor);

    Serial.print("Kalibrierung fertig. Berechneter Faktor: ");
    Serial.println(factor, 4);
    Serial.println("Du kannst das Referenzgewicht jetzt wieder herunternehmen.");
    Serial.println();
    calibrated = true;
  }
}

void loop() {
  if (calibrated && scale.is_ready()) {
    float weight = scale.get_units(10); // gemittelter, kalibrierter Messwert in Gramm
    if (weight < 0) weight = 0; // negative Werte (z.B. durch Drift) abfangen

    float fillLevelPercent = (weight / MAX_CAPACITY_GRAMS) * 100.0;
    if (fillLevelPercent > 100.0) fillLevelPercent = 100.0;

    Serial.print("Gewicht: ");
    Serial.print(weight, 1);
    Serial.print(" g  |  Fuellstand: ");
    Serial.print(fillLevelPercent, 1);
    Serial.println(" %");
  } else {
    Serial.println("Noch nicht kalibriert oder HX711 nicht bereit.");
  }

  delay(1000);
}
