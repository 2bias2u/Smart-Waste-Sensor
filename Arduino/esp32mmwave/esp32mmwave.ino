#define SENSOR_RX 16
#define SENSOR_TX 17

byte buf[11];
int idx = 0;

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, SENSOR_RX, SENSOR_TX);
  delay(500);
  Serial.println("LD2413 Parser gestartet");
}

void loop() {
  while (Serial2.available()) {
    byte b = Serial2.read();

    // Header-Sync: F4 F3 F2 F1
    if (idx == 0 && b != 0xF4) continue;
    if (idx == 1 && b != 0xF3) { idx = 0; continue; }
    if (idx == 2 && b != 0xF2) { idx = 0; continue; }
    if (idx == 3 && b != 0xF1) { idx = 0; continue; }

    buf[idx++] = b;

    if (idx == 11) {
      idx = 0;

      // Footer prüfen
      if (buf[7+0] != 0xF8) continue;  // buf[7..10] = F8 F7 F6 F5

      // Distanz = Float aus buf[6..9]
      float distMM;
      memcpy(&distMM, &buf[6], 4);

      Serial.print("Distanz: ");
      Serial.print(distMM, 1);
      Serial.println(" mm");
    }
  }
}