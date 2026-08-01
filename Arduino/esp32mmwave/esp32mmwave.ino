#define SENSOR_RX 16
#define SENSOR_TX 17

void setup() {
  Serial.begin(115200);        // Debug
  Serial2.begin(9600, SERIAL_8N1, SENSOR_RX, SENSOR_TX);  // LD2413
  
  Serial.println("LD2413 ESP32 Test");
}

void loop() {
  if (Serial2.available()) {
    byte b = Serial2.read();
    
    // Frame-Sync: 0xAA 0xBB
    if (b == 0xAA) {
      if (Serial2.read() == 0xBB) {
        // Skip 2 Bytes, dann Distanz (Little Endian)
        Serial2.read(); // Byte 3
        Serial2.read(); // Byte 4
        
        byte d_low = Serial2.read();
        byte d_high = Serial2.read();
        
        int distMM = d_low | (d_high << 8);
        int distCM = distMM / 10;
        
        Serial.print("Distance: ");
        Serial.print(distCM);
        Serial.println(" cm");
      }
    }
  }
}