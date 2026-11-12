#include <Arduino.h>
#include <heltec_unofficial.h>
#include "HX711.h"
#include <time.h> // Eingebaute Zeit-Bibliothek

// Frequenz für Europa: 868.0 MHz
const float FREQUENCY = 868.0; 

// =========================================================================
// DEINE SENSOR-PINS (Sicher auf Heltec V3)
// =========================================================================
// Pins Waage (HX711)
const int LOADCELL_DOUT_PIN = 4;  
const int LOADCELL_SCK_PIN = 5;   

// Pins Ultraschall 1
const int TRIG1 = 6;              
const int ECHO1 = 7;              

// Pins Ultraschall 2
const int TRIG2 = 19;             
const int ECHO2 = 20;             

HX711 scale;

// Funktion zum Messen der Distanz
float getDistance(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long duration = pulseIn(echo, HIGH, 30000); // Timeout nach 30ms
  if (duration == 0) return 400.0;           
  return duration * 0.034 / 2;
}

// Hilfsfunktion, um die aktuelle Uhrzeit als String zu erhalten
String getFormattedTime() {
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  
  char timeString[9];
  sprintf(timeString, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  return String(timeString);
}

void setup() {
  // Initialisiert das Heltec-Board und schaltet Vext ein
  heltec_setup();
  
  // --- UHRZEIT MANUELL STELLEN ---
  // Da kein WLAN vorhanden ist, stellen wir die Uhr beim Start auf 16:00:00 (Beispielwert)
  struct tm tm;
  tm.tm_year = 2026 - 1900; // Jahr seit 1900
  tm.tm_mon = 4;            // Mai (0 = Januar)
  tm.tm_mday = 19;
  tm.tm_hour = 16;
  tm.tm_min = 0;
  tm.tm_sec = 0;
  time_t t = mktime(&tm);
  struct timeval now = { .tv_sec = t };
  settimeofday(&now, NULL);

  // Sensoren initialisieren
  pinMode(TRIG1, OUTPUT); pinMode(ECHO1, INPUT);
  pinMode(TRIG2, OUTPUT); pinMode(ECHO2, INPUT);

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  // scale.set_scale(DEIN_FAKTOR); 
  scale.tare();
  
  // --- LoRa über die Heltec-Bibliothek starten ---
  Serial.print(F("[LoRa] Initialisiere... "));
  int state = radio.begin(FREQUENCY);
  
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("Erfolgreich!"));
    radio.setOutputPower(17); 
  } else {
    Serial.print(F("Fehlgeschlagen, Code: "));
    Serial.println(state);
    while (true); 
  }

  Serial.println("System bereit. Sende via Heltec LoRa V3...");
}

void loop() {
  // Das Board im Loop am Leben halten
  heltec_loop();

  // 1. Uhrzeit holen
  String currentTime = getFormattedTime();

  // 2. Beide Distanzen messen
  float d1 = getDistance(TRIG1, ECHO1);
  delay(50); 
  float d2 = getDistance(TRIG2, ECHO2);

  // 3. Minimum ermitteln
  float minDistance = min(d1, d2);

  // 4. Gewicht messen
  long weight = 0;
  if (scale.is_ready()) {
    weight = scale.get_units(5);
  }

  // 5. Daten inkl. Uhrzeit als Text-Nachricht zusammenbauen
  // Format: [16:00:05] D1,D2,MIN,Gewicht
  String loraMessage = "[" + currentTime + "] " + String(d1, 1) + "," + String(d2, 1) + "," + String(minDistance, 1) + "," + String(weight);

  // 6. Serielle Ausgabe zur Kontrolle
  Serial.print("Sende LoRa-Paket: ");
  Serial.println(loraMessage);

  // 7. LoRa Paket absenden
  int state = radio.transmit(loraMessage.c_str());

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("Senden erfolgreich!"));
  } else {
    Serial.print(F("Sende-Fehler, Code: "));
    Serial.println(state);
  }

  // 5 Sekunden Pause bis zur nächsten Messung
  delay(5000);
}