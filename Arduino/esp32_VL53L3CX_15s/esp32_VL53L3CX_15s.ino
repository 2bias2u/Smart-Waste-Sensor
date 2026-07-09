/*
  Pololu VL53L3CX am ESP32
  Gibt alle 15 s den Durchschnitt der Werte der letzten 15 s aus
  Board: uPesy ESP32 Wroom Dev
  Pin Belegung:
  Pololu VL53L3CX ESP32
  VIN 3V3
  GND GND 
  SDA GPIO 21 (SDA)
  SCL GPIO 22 (SCL)
  XSHUT GPIO 4
  GPIO1 (Interrupt)nicht angeschlossen (nur für Interrupt-Betrieb nötig)
*/

#include <Wire.h>
#include <vl53lx_class.h>

#define SDA_PIN 21
#define SCL_PIN 22
#define XSHUT_PIN 4

VL53LX sensor(&Wire, XSHUT_PIN);

const unsigned long AUSGABE_INTERVALL_MS = 15UL * 1000UL; // alle 15 s ausgeben

unsigned long letzteAusgabe = 0;
unsigned long summeMM = 0;
unsigned long anzahlWerte = 0;

void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.setBufferSize(256);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  sensor.begin();
  sensor.VL53LX_Off();
  sensor.InitSensor(0x12);
  sensor.VL53LX_StartMeasurement();

  Serial.println("Setup abgeschlossen, sammle Messwerte...");
  letzteAusgabe = millis();
}

void loop() {
  VL53LX_MultiRangingData_t rangingData;
  uint8_t dataReady = 0;
  int status;

  // Sensor so oft wie möglich abfragen
  status = sensor.VL53LX_GetMeasurementDataReady(&dataReady);
  if (!status && dataReady) {
    status = sensor.VL53LX_GetMultiRangingData(&rangingData);
    if (!status && rangingData.NumberOfObjectsFound > 0
        && rangingData.RangeData[0].RangeStatus == 0) {
      // gültige Messung in die laufende Summe einrechnen
      summeMM += rangingData.RangeData[0].RangeMilliMeter;
      anzahlWerte++;
    }
    sensor.VL53LX_ClearInterruptAndStartMeasurement();
  }

  // Alle 15 Sekunden: Durchschnitt der Werte aus diesem Fenster ausgeben und zurücksetzen
  if (millis() - letzteAusgabe >= AUSGABE_INTERVALL_MS) {
    letzteAusgabe += AUSGABE_INTERVALL_MS;

    if (anzahlWerte > 0) {
      float durchschnitt = (float)summeMM / anzahlWerte;
      Serial.print("Durchschnitt (");
      Serial.print(anzahlWerte);
      Serial.print(" Messungen): ");
      Serial.print(durchschnitt, 1);
      Serial.println(" mm");
    } else {
      Serial.println("Keine gültigen Messwerte in den letzten 15 s.");
    }

    // Fenster zurücksetzen für die nächsten 15 s
    summeMM = 0;
    anzahlWerte = 0;
  }
}