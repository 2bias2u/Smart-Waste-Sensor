      /*
  Pololu VL53L3CX + HC-SR04 Ultraschall am ESP32
  Gibt alle 15 s den Durchschnitt der Werte der letzten 15 s aus
  Board: uPesy ESP32 Wroom Dev
  
  Pin Belegung:
  Pololu VL53L3CX  |  ESP32
  VIN              |  3V3
  GND              |  GND
  SDA              |  GPIO 21 (SDA)
  SCL              |  GPIO 22 (SCL)
  XSHUT            |  GPIO 4
  
  HC-SR04          |  ESP32
  VCC              |  VIN (5V)
  GND              |  GND
  TRIG             |  GPIO 23
  ECHO             |  GPIO 25 (über Spannungsteiler: 5V → 3,3V)
                   |  (Spannungsteiler: HC-SR04 ECHO → 1kΩ → GPIO 25 → 1kΩ → GND)
*/

#include <Wire.h>
#include <vl53lx_class.h>

// VL53L3CX Pins
#define SDA_PIN 21
#define SCL_PIN 22
#define XSHUT_PIN 4

// HC-SR04 Pins
#define HC_TRIG_PIN 23
#define HC_ECHO_PIN 25

VL53LX sensor(&Wire, XSHUT_PIN);

const unsigned long AUSGABE_INTERVALL_MS = 15UL * 1000UL; // alle 15 s ausgeben

// VL53L3CX Variablen
unsigned long letzteAusgabe = 0;
unsigned long vl53_summeMM = 0;
unsigned long vl53_anzahlWerte = 0;

// HC-SR04 Variablen
unsigned long hc_summeMM = 0;
unsigned long hc_anzahlWerte = 0;

void setup() {
  Serial.begin(115200);
  delay(500);

  // VL53L3CX Setup
  Wire.setBufferSize(256);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  sensor.begin();
  sensor.VL53LX_Off();
  sensor.InitSensor(0x12);
  sensor.VL53LX_StartMeasurement();

  // HC-SR04 Setup
  pinMode(HC_TRIG_PIN, OUTPUT);
  pinMode(HC_ECHO_PIN, INPUT);
  digitalWrite(HC_TRIG_PIN, LOW);
  delay(100);

  Serial.println("Setup abgeschlossen, sammle Messwerte...");
  letzteAusgabe = millis();
}

// Funktion für HC-SR04 Messung (in mm)
unsigned long messeHC_SR04() {
  // Trigger Puls: 10 µs
  digitalWrite(HC_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(HC_TRIG_PIN, LOW);

  // Echo-Zeit messen (Timeout: 30 ms = max ~5m Entfernung)
  unsigned long pulsDauer = pulseIn(HC_ECHO_PIN, HIGH, 30000);
  
  if (pulsDauer == 0) {
    return 0; // Timeout oder kein Echo
  }

  // Distanz berechnen: (Pulsdauer in µs / 5.8) = mm
  unsigned long distanzMM = pulsDauer / 5.8;

  // Plausibilität prüfen (20 mm bis 4000 mm = 4 m)
  if (distanzMM < 20 || distanzMM > 4000) {
    return 0;
  }

  return distanzMM;
}

void loop() {
  VL53LX_MultiRangingData_t rangingData;
  uint8_t dataReady = 0;
  int status;

  // ===== VL53L3CX auslesen =====
  status = sensor.VL53LX_GetMeasurementDataReady(&dataReady);
  if (!status && dataReady) {
    status = sensor.VL53LX_GetMultiRangingData(&rangingData);
    if (!status && rangingData.NumberOfObjectsFound > 0
        && rangingData.RangeData[0].RangeStatus == 0) {
      vl53_summeMM += rangingData.RangeData[0].RangeMilliMeter;
      vl53_anzahlWerte++;
    }
    sensor.VL53LX_ClearInterruptAndStartMeasurement();
  }

  // ===== HC-SR04 auslesen =====
  unsigned long hc_distanz = messeHC_SR04();
  if (hc_distanz > 0) {
    hc_summeMM += hc_distanz;
    hc_anzahlWerte++;
  }

  // ===== Alle 15 Sekunden: Durchschnitte ausgeben =====
  if (millis() - letzteAusgabe >= AUSGABE_INTERVALL_MS) {
    letzteAusgabe += AUSGABE_INTERVALL_MS;

    // VL53L3CX
    Serial.print("VL53L3CX: ");
    if (vl53_anzahlWerte > 0) {
      float durchschnitt = (float)vl53_summeMM / vl53_anzahlWerte;
      Serial.print("Durchschnitt (");
      Serial.print(vl53_anzahlWerte);
      Serial.print(" Messungen): ");
      Serial.print(durchschnitt, 1);
      Serial.println(" mm");
    } else {
      Serial.println("Keine gültigen Messwerte");
    }

    // HC-SR04
    Serial.print("HC-SR04:   ");
    if (hc_anzahlWerte > 0) {
      float durchschnitt = (float)hc_summeMM / hc_anzahlWerte;
      Serial.print("Durchschnitt (");
      Serial.print(hc_anzahlWerte);
      Serial.print(" Messungen): ");
      Serial.print(durchschnitt, 1);
      Serial.println(" mm");
    } else {
      Serial.println("Keine gültigen Messwerte");
    }
    Serial.println("---");

    // Fenster zurücksetzen
    vl53_summeMM = 0;
    vl53_anzahlWerte = 0;
    hc_summeMM = 0;
    hc_anzahlWerte = 0;
  }
}
