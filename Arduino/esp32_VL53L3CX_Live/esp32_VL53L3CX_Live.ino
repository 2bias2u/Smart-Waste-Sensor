/*
  Pololu VL53L3CX (bis 500 cm) am ESP32 auslesen
  Benötigte Bibliothek: "STM32duino VL53L3CX" von STMicroelectronics
*/

#include <Wire.h>
#include <vl53lx_class.h>

#define SDA_PIN 21
#define SCL_PIN 22
#define XSHUT_PIN 4

VL53LX sensor(&Wire, XSHUT_PIN);

void setup() {
  Serial.begin(115200);
  delay(500);

  // WICHTIG für ESP32: I2C-Puffer vergrößern, sonst schlägt InitSensor() lautlos fehl
  Wire.setBufferSize(256);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  int status;

  status = sensor.begin();
  Serial.print("begin(): ");
  Serial.println(status);

  sensor.VL53LX_Off();

  status = sensor.InitSensor(0x12);
  Serial.print("InitSensor(): ");
  Serial.println(status); // 0 = OK

  status = sensor.VL53LX_StartMeasurement();
  Serial.print("StartMeasurement(): ");
  Serial.println(status); // 0 = OK

  Serial.println("Setup abgeschlossen, starte Messung...");
}

void loop() {
  VL53LX_MultiRangingData_t rangingData;
  uint8_t dataReady = 0;
  int status;

  unsigned long startWait = millis();
  do {
    status = sensor.VL53LX_GetMeasurementDataReady(&dataReady);
    if (millis() - startWait > 2000) {
      Serial.println("Timeout: keine Messdaten bereit (Status=" + String(status) + ")");
      return; // loop() erneut aufrufen statt endlos zu blockieren
    }
  } while (!dataReady);

  if (!status) {
    status = sensor.VL53LX_GetMultiRangingData(&rangingData);
    int objectCount = rangingData.NumberOfObjectsFound;

    for (int i = 0; i < objectCount; i++) {
      Serial.print("Objekt ");
      Serial.print(i);
      Serial.print(" | Status: ");
      Serial.print(rangingData.RangeData[i].RangeStatus); // 0 = gültige Messung
      Serial.print(" | Distanz: ");
      Serial.print(rangingData.RangeData[i].RangeMilliMeter);
      Serial.println(" mm");
    }

    if (status == 0) {
      sensor.VL53LX_ClearInterruptAndStartMeasurement();
    }
  }

  delay(100);
}