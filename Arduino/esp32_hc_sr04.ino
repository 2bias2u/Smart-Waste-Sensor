// ESP32-S3 + HC-SR04 Ultraschallsensor
// Misst die Entfernung zu Objekten
// 
// ⚠️  WARNUNG: HC-SR04 gibt 5V am ECHO aus, ESP32 mag nur 3.3V
//     Das funktioniert oft, kann aber den GPIO beschädigen!
//     Besser: Spannungsteiler (10kΩ + 16kΩ) verwenden oder
//     HC-SR04 mit 3.3V betreiben

// Pin-Definitionen
const int TRIG_PIN = 4;   // GPIO4 für Trigger
const int ECHO_PIN = 5;   // GPIO5 für Echo

// Konstanten
const float SOUND_SPEED = 0.034;  // cm/µs
const int MAX_DISTANCE = 400;      // Max. Entfernung in cm

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Pin-Modi setzen
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  Serial.println("HC-SR04 Ultraschallsensor - ESP32-S3");
  Serial.println("=====================================");
}

void loop() {
  // Entfernung messen
  long distance = measureDistance();
  
  // Ergebnisse ausgeben
  Serial.print("Entfernung: ");
  Serial.print(distance);
  Serial.println(" cm");
  
  delay(500);  // 500ms zwischen Messungen
}

/**
 * Misst die Entfernung mit HC-SR04
 * Rückgabe: Entfernung in cm (oder -1 bei Fehler)
 */
long measureDistance() {
  // Trigger-Pin für 10µs auf HIGH setzen
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  // Echo-Puls warten (mit Timeout)
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);  // Timeout: 30ms
  
  // Wenn kein Echo empfangen
  if (duration == 0) {
    Serial.println("Fehler: Kein Echo empfangen");
    return -1;
  }
  
  // Entfernung berechnen: Distanz = (Zeit × Schallgeschwindigkeit) / 2
  long distance = (duration * SOUND_SPEED) / 2;
  
  // Plausibilitätsprüfung
  if (distance > MAX_DISTANCE) {
    Serial.println("Warnung: Entfernung > Max. Bereich");
    return -1;
  }
  
  return distance;
}

/**
 * Alternative: Mehrere Messungen und Mittelwert
 * Für stabilere Werte verwenden
 */
long measureDistanceAveraged(int samples = 5) {
  long sum = 0;
  int valid_samples = 0;
  
  for (int i = 0; i < samples; i++) {
    long distance = measureDistance();
    if (distance > 0) {
      sum += distance;
      valid_samples++;
    }
    delay(50);  // Kurze Verzögerung zwischen Samples
  }
  
  if (valid_samples == 0) {
    return -1;
  }
  
  return sum / valid_samples;
}
