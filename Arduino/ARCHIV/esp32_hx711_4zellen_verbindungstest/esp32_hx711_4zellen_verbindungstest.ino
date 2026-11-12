#include "HX711.h"

const int DT_PIN  = 18;
const int SCK_PIN = 19;

HX711 scale;
long nullwert = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== HX711 Verkabelungstest ===");

  scale.begin(DT_PIN, SCK_PIN);

  if (!scale.wait_ready_timeout(3000)) {
    Serial.println("FEHLER: keine Antwort vom HX711.");
    Serial.println("-> VCC an 3V3? GND verbunden?");
    Serial.println("-> VCC und VDD auf dem Modul gebrueckt?");
    Serial.println("-> DT an GPIO18, SCK an GPIO19 (nicht vertauscht)?");
    while (1) delay(1000);
  }
  Serial.println("HX711 antwortet. OK.");
  Serial.println("Waage jetzt NICHT beruehren...");
  delay(2000);

  nullwert = scale.read_average(20);
  Serial.printf("Nullwert: %ld\n", nullwert);

  if (nullwert == 0 || abs(nullwert) > 8000000L) {
    Serial.println("WARNUNG: Wert am Anschlag -> Bruecke falsch verschaltet");
    Serial.println("oder eine Ader lose. Zellenfarben nachmessen.");
  } else {
    Serial.println("Nullwert plausibel.");
  }

  Serial.println("\nJetzt auf die Zellen druecken.");
  Serial.println("Erwartet: deutliche Aenderung, danach zurueck auf ~0.\n");
}

void loop() {
  long roh = scale.read_average(5);
  long diff = roh - nullwert;

  Serial.printf("roh: %8ld   diff: %+8ld   ", roh, diff);

  if (abs(diff) < 500)         Serial.println("- keine Last");
  else if (abs(diff) < 5000)   Serial.println("- leichtes Signal");
  else                         Serial.println("- SIGNAL OK");

  delay(300);
}