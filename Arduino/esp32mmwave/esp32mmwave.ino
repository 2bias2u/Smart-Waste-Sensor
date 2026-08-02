#define SENSOR_RX 16
#define SENSOR_TX 17

byte buf[11];
int idx = 0;

unsigned long lastPrint = 0;
const unsigned long INTERVAL = 10000;  // 10 s

// letzte gültige Distanz zwischenspeichern
float lastDist = -1;

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, SENSOR_RX, SENSOR_TX);
  delay(500);
  Serial.println("LD2413 Parser - Ausgabe alle 10s");
}

void loop() {
  // --- Frames im Hintergrund einlesen ---
  while (Serial2.available()) {
    byte b = Serial2.read();

    if (idx == 0 && b != 0xF4) continue;
    if (idx == 1 && b != 0xF3) { idx = 0; continue; }
    if (idx == 2 && b != 0xF2) { idx = 0; continue; }
    if (idx == 3 && b != 0xF1) { idx = 0; continue; }

    buf[idx++] = b;

    if (idx == 11) {
      idx = 0;
      if (buf[10] != 0xF8) continue;   // Footer-Check

      float distMM;
      memcpy(&distMM, &buf[6], 4);
      if (distMM > 0) lastDist = distMM;   // 0 = kein Ziel, ignorieren
    }
  }

  // --- alle 10 s ausgeben ---
  if (millis() - lastPrint >= INTERVAL) {
    lastPrint = millis();
    if (lastDist > 0) {
      Serial.print("Distanz: ");
      Serial.print(lastDist, 1);
      Serial.print(" mm  (");
      Serial.print(lastDist / 10.0, 1);
      Serial.println(" cm)");
    } else {
      Serial.println("Kein gueltiger Wert");
    }
  }
}