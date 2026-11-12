#include <Arduino.h>
#include "HX711.h"

// Pins für die HX711 Wiegezelle (hier anpassen, falls du andere Pins nutzt)
const int LOADCELL_DOUT_PIN = 23; 
const int LOADCELL_SCK_PIN = 22;

HX711 scale;

// Kalibrierfaktor (Diesen Wert musst du durch Kalibrierung ermitteln!)
// Wenn das Gewicht in Gramm angezeigt werden soll, teile den Rohwert durch das bekannte Gewicht.
float calibration_factor = 0.0; 

void setup() {
  Serial.begin(115200);
  delay(10);
  
  Serial.println("Initialisiere die Wiegezelle...");

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  
  // Setzt den Kalibrierfaktor
  scale.set_scale(calibration_factor); 
  
  // Setzt die Waage auf Null (Tara). 
  // WICHTIG: Beim Starten darf kein Gewicht auf der Waage liegen!
  scale.tare(); 

  Serial.println("Wiegezelle bereit!");
}

void loop() {
  // Prüfen, ob der HX711-Sensor bereit ist
  if (scale.is_ready()) {
    // get_units(5) nimmt 5 Messungen und bildet den Durchschnitt für einen stabileren Wert
    long weight = scale.get_units(5);
    
    // Ausgabe auf dem Seriellen Monitor
    Serial.print("Gewicht: ");
    Serial.print(weight);
    Serial.println(" g"); // Einheit je nach Kalibrierfaktor (z.B. g oder kg)
  } else {
    Serial.println("HX711 nicht gefunden. Verkabelung prüfen!");
  }

  delay(1000); // Jede Sekunde eine Messung
}