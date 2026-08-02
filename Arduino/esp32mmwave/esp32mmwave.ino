#define SENSOR_RX 16
#define SENSOR_TX 17

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, SENSOR_RX, SENSOR_TX);  // 115200!
  delay(1000);
  Serial.println("LD2413 Hex-Dump:");
}

void loop() {
  if (Serial2.available()) {
    byte b = Serial2.read();
    if (b < 0x10) Serial.print("0");
    Serial.print(b, HEX);
    Serial.print(" ");
  }
}