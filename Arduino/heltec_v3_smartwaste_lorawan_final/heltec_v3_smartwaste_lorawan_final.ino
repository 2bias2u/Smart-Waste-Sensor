/***************************************************************************
  Smart-Waste Sensor-Sketch für Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262)

  Sensoren:
    - 2x HC-SR04 Ultraschall (Distanz, Minimum wird verwendet)

  Sendet echtes LoRaWAN (ABP) an The Things Network (TTN).
  Device: lora32-apb in der App smart-trash-ba.
  Eigene, eindeutige NwkSKey/AppSKey (nicht mehr identisch mit ab01-001).

  Pin-Belegung (Heltec WiFi LoRa 32 V3):
    HC-SR04 Sensor 1:
      TRIG -> GPIO6
      ECHO -> GPIO7
    HC-SR04 Sensor 2:
      TRIG -> GPIO19
      ECHO -> GPIO20

  Vom SX1262-Funkchip und OLED-Display bereits belegte Pins wurden
  bewusst NICHT genutzt (NSS=8, RST=12, DIO1=14, BUSY=13, SCK=9, MISO=11,
  MOSI=10, OLED SCL=18/SDA=17/RST=21) - siehe Heltec-V3-GPIO-Guide.
***************************************************************************/

#include <Arduino.h>
#include <heltec_unofficial.h>
#include <RadioLib.h>

// =========================================================================
// TTN ABP Zugangsdaten (aus Device "lora32-apb" in TTN Console)
// =========================================================================
uint32_t devAddr = 0x260BCA14;

// NwkSKey - fuer LoRaWAN 1.0.x als "nwkSEncKey" Parameter verwendet
uint8_t nwkSKey[] = { 0x6F, 0xDF, 0xD9, 0x24, 0xE4, 0x1F, 0xA3, 0x59,
                       0xE6, 0x9C, 0x9B, 0xE9, 0x05, 0xF3, 0x73, 0x52 };

// AppSKey
uint8_t appSKey[] = { 0xD0, 0x3A, 0xC0, 0xF1, 0x74, 0xCE, 0x05, 0x19,
                       0xE1, 0xD4, 0xC3, 0x62, 0xE0, 0x9D, 0x31, 0x57 };

// LoRaWAN Node (nutzt das von heltec_unofficial bereitgestellte "radio"-Objekt)
LoRaWANNode node(&radio, &EU868);

// =========================================================================
// Sensor-Pins
// =========================================================================
// HC-SR04 Sensor 1
const int TRIG1 = 6;
const int ECHO1 = 7;

// HC-SR04 Sensor 2
const int TRIG2 = 19;
const int ECHO2 = 20;

// =========================================================================
// Sensor-Messfunktion
// =========================================================================
float getDistance(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long duration = pulseIn(echo, HIGH, 30000); // Timeout nach 30ms
  if (duration == 0) return 400.0;            // Kein Echo -> Max-Distanz
  return duration * 0.034 / 2;
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println(F("=== SETUP START ==="));

  // Initialisiert das Heltec-Board (Vext, Serial, etc.)
  heltec_setup();
  Serial.println(F("[1] heltec_setup() fertig"));

  // --- HC-SR04 ---
  pinMode(TRIG1, OUTPUT); pinMode(ECHO1, INPUT);
  pinMode(TRIG2, OUTPUT); pinMode(ECHO2, INPUT);
  Serial.println(F("[2] HC-SR04 Pins gesetzt"));

  // --- LoRaWAN ABP Session starten (kein Join noetig) ---
  Serial.println(F("[LoRaWAN] Starte Radio..."));
  int state = radio.begin();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[LoRaWAN] Radio-Start fehlgeschlagen, Code: "));
    Serial.println(state);
    while (true);
  }

  Serial.println(F("[LoRaWAN] Starte ABP-Session..."));
  // LoRaWAN 1.0.x: fNwkSIntKey und sNwkSIntKey = NULL, nwkSEncKey = NwkSKey
  state = node.beginABP(devAddr, NULL, NULL, nwkSKey, appSKey);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[LoRaWAN] beginABP fehlgeschlagen, Code: "));
    Serial.println(state);
  }

  state = node.activateABP();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[LoRaWAN] activateABP fehlgeschlagen, Code: "));
    Serial.println(state);
  } else {
    Serial.println(F("[LoRaWAN] ABP-Session aktiv!"));
  }
}

void loop() {
  heltec_loop();

  // --- Ultraschall messen ---
  float d1 = getDistance(TRIG1, ECHO1);
  delay(50);
  float d2 = getDistance(TRIG2, ECHO2);
  float minDistance = min(d1, d2);

  // --- Serielle Kontrollausgabe ---
  Serial.print("D1: "); Serial.print(d1, 1);
  Serial.print(" cm | D2: "); Serial.print(d2, 1);
  Serial.print(" cm | MIN: "); Serial.println(minDistance, 1);

  // --- Payload zusammenbauen (6 Bytes, Big-Endian) ---
  // Byte 0-1: Distanz 1 (cm), uint16
  // Byte 2-3: Distanz 2 (cm), uint16
  // Byte 4-5: Minimum (cm), uint16
  uint8_t payload[6];
  uint16_t d1_u = (uint16_t)d1;
  uint16_t d2_u = (uint16_t)d2;
  uint16_t min_u = (uint16_t)minDistance;

  payload[0] = d1_u >> 8;   payload[1] = d1_u & 0xFF;
  payload[2] = d2_u >> 8;   payload[3] = d2_u & 0xFF;
  payload[4] = min_u >> 8;  payload[5] = min_u & 0xFF;

  // --- Senden ---
  Serial.println(F("[LoRaWAN] Sende Uplink..."));
  int state = node.sendReceive(payload, sizeof(payload));

  if (state == RADIOLIB_ERR_NONE || state > 0) {
    Serial.println(F("[LoRaWAN] Senden erfolgreich!"));
  } else {
    Serial.print(F("[LoRaWAN] Sende-Fehler, Code: "));
    Serial.println(state);
  }

  // Pause bis zum naechsten Sendezyklus (LoRaWAN Fair-Use: nicht zu oft senden!)
  delay(60000); // 60 Sekunden
}
