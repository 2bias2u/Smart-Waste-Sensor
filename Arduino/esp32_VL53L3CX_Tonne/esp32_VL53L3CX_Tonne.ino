/*
  ============================================================================
  Pololu VL53L3CX am ESP32 - Füllstandsmessung bei schräger Montage
  ============================================================================
  Szenario: Sensor sitzt seitlich OBEN in der Tonne und misst schräg nach
  unten auf die Müll-/Abfalloberfläche.

  Der Sensor liefert die SCHRÄGE (diagonale) Distanz zum Zielobjekt.
  Wir rechnen daraus die SENKRECHTE Distanz (= relevante Füllhöhe) um.

  Benötigte Bibliothek: "STM32duino VL53L3CX" von STMicroelectronics
  ============================================================================
*/

#include <Wire.h>
#include <vl53lx_class.h>

// ---------------------------------------------------------------------------
// I2C- und Steuer-Pins (ESP32)
// ---------------------------------------------------------------------------
#define SDA_PIN   21
#define SCL_PIN   22
#define XSHUT_PIN 4

VL53LX sensor(&Wire, XSHUT_PIN);

// ---------------------------------------------------------------------------
// EINSTELLUNGEN ZUM ANPASSEN - hier unbedingt an deine Tonne/Montage anpassen!
// ---------------------------------------------------------------------------

const float NEIGUNGSWINKEL_GRAD = 45.0;

const float TONNEN_INNENHOEHE_MM = 1000.0;  // TODO: an deine Tonne anpassen

const uint16_t MIN_PLAUSIBLE_MM = 50;
const uint16_t MAX_PLAUSIBLE_MM = 1600;

const unsigned long AUSGABE_INTERVALL_MS = 15UL * 1000UL; // alle 15s Durchschnitt ausgeben

// NEU: Debug-Ausgaben werden nicht mehr bei jeder Messung gedruckt, sondern
// nur alle X Sekunden. Hier die gewünschte Sekundenzahl eintragen.
// -> 0 bedeutet: Debug-Ausgabe komplett aus (unabhängig von DEBUG_ROHWERTE).
const unsigned long DEBUG_INTERVALL_SEKUNDEN = 3;
const unsigned long DEBUG_INTERVALL_MS = DEBUG_INTERVALL_SEKUNDEN * 1000UL;

// Debug-Ausgaben grundsätzlich an/aus. Bei false gibt es NIE Debug-Zeilen,
// egal was DEBUG_INTERVALL_SEKUNDEN sagt.
const bool DEBUG_ROHWERTE = true;

// ---------------------------------------------------------------------------
// Interne Variablen - hier normalerweise nichts ändern
// ---------------------------------------------------------------------------
unsigned long letzteAusgabe = 0;
unsigned long letzteDebugAusgabe = 0;
unsigned long summeHoeheMM = 0;
unsigned long anzahlWerte = 0;

float cosNeigungswinkel;

void setup() {
  Serial.begin(115200);
  delay(500);

  cosNeigungswinkel = cos(radians(NEIGUNGSWINKEL_GRAD));

  Wire.setBufferSize(256);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  int status;

  status = sensor.begin();
  Serial.print("begin(): "); Serial.println(status);

  sensor.VL53LX_Off();

  status = sensor.InitSensor(0x12);
  Serial.print("InitSensor(): "); Serial.println(status);

  // OPTIONAL: ROI-Block, siehe frühere Erklärung - bleibt hier auskommentiert
  /*
  VL53LX_UserRoi_t roi;
  roi.TopLeftX  = 6;
  roi.TopLeftY  = 9;
  roi.BotRightX = 9;
  roi.BotRightY = 6;
  int roiStatus = sensor.VL53LX_SetUserROI(&roi);
  Serial.print("ROI gesetzt: "); Serial.println(roiStatus);
  */

  status = sensor.VL53LX_StartMeasurement();
  Serial.print("StartMeasurement(): "); Serial.println(status);

  Serial.println("Setup abgeschlossen, sammle Messwerte...");
  Serial.print("cos(Neigungswinkel) = "); Serial.println(cosNeigungswinkel, 4);

  letzteAusgabe = millis();
  letzteDebugAusgabe = millis();
}

void loop() {
  VL53LX_MultiRangingData_t rangingData;
  uint8_t dataReady = 0;
  int status;

  status = sensor.VL53LX_GetMeasurementDataReady(&dataReady);
  if (!status && dataReady) {

    status = sensor.VL53LX_GetMultiRangingData(&rangingData);
    if (!status) {

      int objektAnzahl = rangingData.NumberOfObjectsFound;

      // Prüfen, ob gerade ein Debug-Ausgabe-Zeitpunkt erreicht ist
      bool jetztDebugAusgeben = DEBUG_ROHWERTE
                                 && DEBUG_INTERVALL_MS > 0
                                 && (millis() - letzteDebugAusgabe >= DEBUG_INTERVALL_MS);

      if (jetztDebugAusgeben) {
        letzteDebugAusgabe = millis();

        Serial.print("[Debug] ");
        Serial.print(objektAnzahl);
        Serial.print(" Objekt(e): ");
        for (int i = 0; i < objektAnzahl; i++) {
          Serial.print(" | #");
          Serial.print(i);
          Serial.print(" Status=");
          Serial.print(rangingData.RangeData[i].RangeStatus);
          Serial.print(" Dist=");
          Serial.print(rangingData.RangeData[i].RangeMilliMeter);
          Serial.print("mm");
        }
        Serial.println();
      }

      int16_t besteDistanzMM = -1;

      for (int i = 0; i < objektAnzahl; i++) {
        uint8_t rangeStatus = rangingData.RangeData[i].RangeStatus;
        int16_t distanzMM   = rangingData.RangeData[i].RangeMilliMeter;

        if (rangeStatus != 0) continue;
        if (distanzMM < MIN_PLAUSIBLE_MM || distanzMM > MAX_PLAUSIBLE_MM) continue;

        if (distanzMM > besteDistanzMM) {
          besteDistanzMM = distanzMM;
        }
      }

      if (besteDistanzMM > 0) {
        float senkrechteHoeheMM = besteDistanzMM * cosNeigungswinkel;

        // Diese Zeile hängt an dieselbe Drossel wie der Objekt-Debug oben,
        // damit beide zusammen erscheinen (oder beide nicht).
        if (jetztDebugAusgeben) {
          Serial.print("[Debug] gewaehlt: ");
          Serial.print(besteDistanzMM);
          Serial.print(" mm schraeg -> ");
          Serial.print(senkrechteHoeheMM, 1);
          Serial.println(" mm senkrecht");
        }

        summeHoeheMM += (unsigned long)senkrechteHoeheMM;
        anzahlWerte++;
      }
    }

    sensor.VL53LX_ClearInterruptAndStartMeasurement();
  }

  if (millis() - letzteAusgabe >= AUSGABE_INTERVALL_MS) {
    letzteAusgabe += AUSGABE_INTERVALL_MS;

    if (anzahlWerte > 0) {
      float durchschnittHoeheMM = (float)summeHoeheMM / anzahlWerte;

      float fuellstandProzent = 100.0 * (TONNEN_INNENHOEHE_MM - durchschnittHoeheMM)
                                        / TONNEN_INNENHOEHE_MM;
      if (fuellstandProzent < 0)   fuellstandProzent = 0;
      if (fuellstandProzent > 100) fuellstandProzent = 100;

      Serial.print(">>> Ergebnis (");
      Serial.print(anzahlWerte);
      Serial.print(" Messungen): senkrechte Hoehe = ");
      Serial.print(durchschnittHoeheMM, 1);
      Serial.print(" mm  |  Fuellstand = ");
      Serial.print(fuellstandProzent, 1);
      Serial.println(" %");
    } else {
      Serial.println(">>> Keine gueltigen/plausiblen Messwerte in den letzten 15 s.");
    }

    summeHoeheMM = 0;
    anzahlWerte = 0;
  }
}