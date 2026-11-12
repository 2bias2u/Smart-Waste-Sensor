#include "LoRaWan_APP.h"
#include "Arduino.h"
#include <EEPROM.h>

/*
 * set LoraWan_RGB to Active,the RGB active in loraWan
 * RGB red means sending;
 * RGB purple means joined done;
 * RGB blue means RxWindow1;
 * RGB yellow means RxWindow2;
 * RGB green means received done;
 *
 * ANGEPASST: Kein GPS-Modul mehr verbaut. Diese Version läuft auf dem
 * Heltec CubeCell AB01 Dev-Board (V2) mit einem DIY-Ultraschallsensor
 * (HC-SR04-Prinzip: Trigger/Echo) zur Füllstandsmessung des Mülleimers.
 * Statt GPS-Koordinaten wird pro Ping ein Zähler (Counter), der gemessene
 * Abstand Sensor->Füllgut in cm sowie die Batteriespannung übertragen.
 * Die Umrechnung von Abstand in Füllstand (%) erfolgt im Backend
 * (n8n/TimescaleDB), da dort die genaue Behälterhöhe bekannt/pflegbar ist.
 */

// --- Ultraschallsensor (HC-SR04-Prinzip) ---
// Vorher für GPS-UART genutzte Pins sind jetzt frei und werden für den
// Sensor verwendet.
#define TRIG_PIN GPIO5
#define ECHO_PIN GPIO3

// Maximal plausibler Messbereich in cm. Werte außerhalb gelten als ungültig
// (z.B. kein Echo, Sensor verdeckt, Störreflexion).
const int MAX_DISTANCE_CM = 400;

// Nur für die serielle Debug-Ausgabe: geschätzte Innenhöhe des Behälters
// vom Sensor bis zum Boden. BITTE AUF EUREN BEHÄLTER ANPASSEN!
const int BIN_HEIGHT_CM = 100;

// Board-ID for internal analysis - PLEASE EDIT THIS VALUE FOR YOUR BOARD!
int boardID = 999; 

// --- Dummy OTAA Keys (Werden vom Compiler zwingend verlangt, auch bei ABP) ---
uint8_t devEui[] = { 0x70, 0xB3, 0xD5, 0x7E, 0xD0, 0x07, 0x84, 0x6A };    //Wert aus dem SWP Chirpstack
uint8_t appEui[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
uint8_t appKey[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };


// --- ABP-Key-Management für zwei Netzwerke ---
// 1. TTN Keys
uint8_t nwkSKey_TTN[] = { 0x2D, 0xA6, 0x5C, 0xE3, 0xBF, 0xC9, 0xE6, 0x11, 0x2D, 0xB2, 0x3C, 0x3F, 0xE6, 0xEE, 0xAC, 0x32 }; 
uint8_t appSKey_TTN[] = { 0x0B, 0xF0, 0xF5, 0x11, 0xD2, 0x79, 0x2A, 0xFA, 0x44, 0x83, 0xB3, 0xE2, 0xA8, 0xDF, 0xFF, 0x3F };
uint32_t devAddr_TTN  = ( uint32_t )0x260B45B8;

// 2. ChirpStack Keys
uint8_t nwkSKey_CS[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB };
uint8_t appSKey_CS[] = { 0xCA, 0xFE, 0xBA, 0xBE, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, 0x11, 0x22, 0x33, 0x44 };
uint32_t devAddr_CS  = ( uint32_t )0x89ABCDEF;


// Aktive Keys, die von der Library genutzt werden (werden in der Loop überschrieben)
uint8_t nwkSKey[16];
uint8_t appSKey[16];
uint32_t devAddr;

bool sendToTTN = true; // Toggle-Variable für den Netzwerkwechsel

// NEU: Zähler damit TTN nur jedes 2. Mal gesendet wird (alle 90s statt 45s)
int cycleCounter = 0;

// Speichern von Pings für Funklöcher:
int eepromAddr = 0; // Aktuelle Schreibadresse
const int MAX_EEPROM_ADDR = 512; // Das AB01 hat ca. 512-1024 Bytes emuliertes EEPROM.

uint8_t deviceStatusBatteryLevel = 0;

/*LoraWan channelsmask, default channels 0-7*/ 
uint16_t userChannelsMask[6]={ 0x00FF,0x0000,0x0000,0x0000,0x0000,0x0000 };

/*LoraWan region, select in arduino IDE tools*/
LoRaMacRegion_t loraWanRegion = ACTIVE_REGION;

/*LoraWan Class, Class A and Class C are supported*/
DeviceClass_t  loraWanClass = LORAWAN_CLASS;

// 45s Intervall - CS sendet jeden Zyklus (alle 45s), TTN jeden 2. Zyklus (= alle 90s)
// Begründung: Duty Cycle (EU 1%) und TTN Fair Use Policy (30s/Tag bei geschätzt 7h Betrieb)
uint32_t appTxDutyCycle = 45000;

/*OTAA or ABP*/
bool overTheAirActivation = false; // Fix auf ABP gesetzt

/*ADR enable*/
bool loraWanAdr = LORAWAN_ADR;

/* MUSS false sein! Da wir den MAC-State alle 45s neu initiieren, würde true den Flash zerstören. */
bool keepNet = false; 

// Unconfirmed Mode - kein Retry, kein ACK-Downlink
// Begründung: Retries würden Duty Cycle und Fair Use Budget sprengen.
bool isTxConfirmed = false;

/* Application port */
uint8_t appPort = 2;
uint8_t confirmedNbTrials = 2;

// Struktur für die History (ANGEPASST: kein Lon/Lat mehr, dafür Ultraschall-Distanz)
struct PingEntry {
  uint16_t counter;
  uint16_t distanceCM; // Abstand Sensor -> Füllgut in cm
  uint16_t battery;
};

// TTN History auf 8 Einträge, CS History auf 11 Einträge
// Payload je Eintrag jetzt 6 Bytes (2 Counter + 2 Distanz + 2 Batterie) statt vorher 10 Bytes (GPS)
// TTN: 1 + (8 × 6) = 49 Bytes
// CS:  1 + (11 × 6) = 67 Bytes
PingEntry historyTTN[8]; 
PingEntry historyCS[11];

// Ping-Counter for internal analysis
int countTTN = 0; 
int countCS = 0; 


// Save every ping in a ring-memory to the EEPROM for Funkloch-Upload
void savePingToEEPROM(uint16_t c, uint16_t distanceCM, uint16_t battery) {
  if (eepromAddr + 7 > 500) {   
    eepromAddr = 0; //Start memory-adress from the beginning, when EEPROM-end is reached 
  }

  EEPROM.write(eepromAddr,     (uint8_t)(c >> 8));
  EEPROM.write(eepromAddr + 1, (uint8_t)(c & 0xFF));
  EEPROM.write(eepromAddr + 2, (uint8_t)(distanceCM >> 8));
  EEPROM.write(eepromAddr + 3, (uint8_t)(distanceCM & 0xFF));
  EEPROM.write(eepromAddr + 4, (uint8_t)(battery >> 8));
  EEPROM.write(eepromAddr + 5, (uint8_t)(battery & 0xFF));
  EEPROM.write(eepromAddr + 6, 0xFF);
  
  delay(50); 
  
  if(EEPROM.commit()) {
    Serial.print("Gespeichert bei Adresse: ");
    Serial.println(eepromAddr);
    eepromAddr += 7;
  } else {
    delay(100);
    if(EEPROM.commit()) {
       Serial.println("Gespeichert im 2. Versuch!");
       eepromAddr += 7;
    } else {
       Serial.println("Flash Hardware-Fehler.");
    }
  }
}


// Misst den Abstand Sensor -> Füllgut mit dem Ultraschallsensor (HC-SR04-Prinzip).
// Gibt 0 zurück, wenn kein gültiges Echo empfangen wurde oder der Messwert
// außerhalb des plausiblen Bereichs liegt.
uint16_t measureDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Timeout 30ms entspricht ca. 5m Reichweite -> vermeidet Blockieren bei fehlendem Echo
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) {
    return 0; // kein Echo empfangen
  }

  long distance = duration * 0.0343 / 2.0; // Schallgeschwindigkeit ~343 m/s

  if (distance <= 0 || distance > MAX_DISTANCE_CM) {
    return 0; // unplausibler Messwert
  }

  return (uint16_t)distance;
}


// Führt mehrere Einzelmessungen durch und bildet den Mittelwert der gültigen
// Messungen, um einzelne Fehlmessungen/Störreflexionen abzufedern.
uint16_t measureDistanceCMFiltered(int samples = 3) {
  uint32_t sum = 0;
  int validCount = 0;

  for (int i = 0; i < samples; i++) {
    uint16_t d = measureDistanceCM();
    if (d > 0) {
      sum += d;
      validCount++;
    }
    delay(60); // kurze Pause zwischen den Einzelmessungen
  }

  if (validCount == 0) {
    return 0; // keine gültige Messung möglich
  }

  return (uint16_t)(sum / validCount);
}


// Misst den Füllstand per Ultraschall und sendet ihn via LoRaWAN
static bool prepareTxFrame( uint8_t port ) 
{
  deviceStatusBatteryLevel = map(getBatteryVoltage(), 3300, 4200, 1, 254);
  uint16_t currentBattery = getBatteryVoltage();
  uint16_t currentDistance = measureDistanceCMFiltered();

  // History-Arrays aktualisieren (neuester Wert vorne)
  if (sendToTTN) {
    for (int i = 7; i > 0; i--) historyTTN[i] = historyTTN[i-1];
    historyTTN[0] = { (uint16_t)countTTN, currentDistance, currentBattery };
  } else {
    for (int i = 10; i > 0; i--) historyCS[i] = historyCS[i-1];
    historyCS[0] = { (uint16_t)countCS, currentDistance, currentBattery };
  }

  // Debugging
  Serial.println();
  if (sendToTTN){
    Serial.println("Sende Daten an TTN...");
    Serial.print("Counter: ");
    Serial.println(countTTN);    
  } else {
    Serial.println("Sende Daten an CS...");
    Serial.print("Counter: ");
    Serial.println(countCS);
  }

  if (currentDistance == 0) {
    Serial.println("Ultraschall: kein gültiger Messwert (Sensor prüfen).");
  } else {
    Serial.print("Abstand Sensor->Füllgut: ");
    Serial.print(currentDistance);
    Serial.println(" cm");

    int fillLevelPercent = (int)(100.0 * (BIN_HEIGHT_CM - currentDistance) / BIN_HEIGHT_CM);
    fillLevelPercent = constrain(fillLevelPercent, 0, 100);
    Serial.print("Geschätzter Füllstand: ");
    Serial.print(fillLevelPercent);
    Serial.println(" % (nur Debug, Behälterhöhe im Code ggf. anpassen)");
  }

  Serial.print("Batteriespannung: ");
  Serial.print(currentBattery / 1000.0, 2);
  Serial.println(" V");

  // Payload-Größen (ANGEPASST: Counter + Distanz + Batterie statt GPS)
  // TTN: 1 + (8 × 6) = 49 Bytes
  // CS:  1 + (11 × 6) = 67 Bytes
  if (sendToTTN) {
    appDataSize = 49;
  } else {
    appDataSize = 67;
  }

  appData[0] = (uint8_t)(boardID & 0xFF);

  if (sendToTTN) {
    for (int i = 0; i < 8; i++) {
      int base = 1 + (i * 6);
      appData[base]     = historyTTN[i].counter >> 8;
      appData[base + 1] = historyTTN[i].counter & 0xFF;
      appData[base + 2] = historyTTN[i].distanceCM >> 8;
      appData[base + 3] = historyTTN[i].distanceCM & 0xFF;
      appData[base + 4] = historyTTN[i].battery >> 8;
      appData[base + 5] = historyTTN[i].battery & 0xFF;
    }
  } else {
    for (int i = 0; i < 11; i++) {
      int base = 1 + (i * 6);
      appData[base]     = historyCS[i].counter >> 8;
      appData[base + 1] = historyCS[i].counter & 0xFF;
      appData[base + 2] = historyCS[i].distanceCM >> 8;
      appData[base + 3] = historyCS[i].distanceCM & 0xFF;
      appData[base + 4] = historyCS[i].battery >> 8;
      appData[base + 5] = historyCS[i].battery & 0xFF;
    }
  }

  // Nur TTN-Pings werden für den Funkloch-Upload im EEPROM gesichert
  if (sendToTTN) {
    savePingToEEPROM((uint16_t)countTTN, currentDistance, currentBattery);
  }

  if (sendToTTN) {
    countTTN = countTTN + 1;
  } else {
    countCS = countCS + 1;
  }
  
  return true;
}


// Function can be called by writing 'd' into the serial monitor in the first 15 seconds of booting the board. 
// dumpEEPROM writes the saved pings out for the Funkloch-Upload
void dumpEEPROM() {
  Serial.println("--- START EEPROM DUMP (CSV) ---");
  Serial.print("BoardID:"); 
  Serial.println(boardID); 
  Serial.println("Counter;AbstandCM;BatterieSpannung(mV)");
  
  for (int i = 0; i < MAX_EEPROM_ADDR - 6; i += 7) {
    uint16_t c = (EEPROM.read(i) << 8) | EEPROM.read(i + 1);
    if (c == 0xFFFF) continue; 

    uint16_t distance = (EEPROM.read(i + 2) << 8) | EEPROM.read(i + 3);
    uint16_t battery = (EEPROM.read(i + 4) << 8) | EEPROM.read(i + 5);

    Serial.print(c); Serial.print(";");
    Serial.print(distance); Serial.print(";");
    Serial.println(battery);
  }
  Serial.println("--- END DUMP ---");
}


// Helper: Loggt den MCPS-Status im Klartext
void logMcpsStatus(const char* netzwerk, LoRaMacStatus_t status) {
  Serial.print("MCPS Status ");
  Serial.print(netzwerk);
  Serial.print(": ");
  Serial.print(status);
  Serial.print(" -> ");
  switch (status) {
    case LORAMAC_STATUS_OK:
      Serial.println("OK, Paket in Sende-Queue.");
      break;
    case LORAMAC_STATUS_BUSY:
      Serial.println("FEHLER: MAC-Stack noch busy.");
      break;
    case LORAMAC_STATUS_DUTYCYCLE_RESTRICTED:
      Serial.println("FEHLER: Duty-Cycle blockiert.");
      break;
    case LORAMAC_STATUS_LENGTH_ERROR:
      Serial.println("FEHLER: Payload zu gross.");
      break;
    default:
      Serial.println("FEHLER: Unbekannter Code.");
      break;
  }
}


// Setup function which is called when the Heltec-Board boots up
void setup() {
  Serial.begin(115200);
  EEPROM.begin(MAX_EEPROM_ADDR);

  Serial.println("Sende 'd' innerhalb von 15s, um den Speicher auszulesen...");
  unsigned long startWait = millis();
  while (millis() - startWait < 15000) {
    if (Serial.available() > 0) {
      char c = Serial.read();
      if (c == 'd') {
        dumpEEPROM();
      }
    }
  }

  // Akkustand beim Booten auslesen
  uint16_t bootVoltage = getBatteryVoltage();
  float voltageV = bootVoltage / 1000.0;
  
  Serial.println("--- Boot Vorgang ---");
  Serial.print("Akkuspannung: ");
  Serial.print(voltageV);
  Serial.println(" V");

  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
  delay(500);

  // Ultraschallsensor initialisieren
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

#if(AT_SUPPORT)
  enableAt();
#endif

  // Initialisiere mit CS Keys für den ersten Durchlauf (CS sendet zuerst)
  memcpy(nwkSKey, nwkSKey_CS, 16);
  memcpy(appSKey, appSKey_CS, 16);
  devAddr = devAddr_CS;

  deviceState = DEVICE_STATE_INIT;
  LoRaWAN.ifskipjoin();
}


void loop()
{
  delay(1000);

  switch( deviceState )
  {
    case DEVICE_STATE_INIT:
    {
#if(LORAWAN_DEVEUI_AUTO)
      LoRaWAN.generateDeveuiByChipID();
#endif
#if(AT_SUPPORT)
      getDevParam();
#endif
      printDevParam();
      LoRaWAN.init(loraWanClass,loraWanRegion);
      deviceState = DEVICE_STATE_JOIN;
      break;
    }
    case DEVICE_STATE_JOIN:
    {
      LoRaWAN.join();

      // Nach erfolgreichem Join: RX2-Fenster auf SF9 setzen (TTN EU868 Standard)
      // und Link Check deaktivieren (TTN V3 unterstützt Link Check nicht —
      // ohne diese Einstellung würde der Stack die Verbindung irgendwann als tot markieren)
      MibRequestConfirm_t mibJoin;
      mibJoin.Type = MIB_NETWORK_JOINED;
      LoRaMacMibGetRequestConfirm(&mibJoin);
      if (mibJoin.Param.IsNetworkJoined) {
        // RX2 auf SF9 (DR_3) — Standard für TTN EU868
        mibJoin.Type = MIB_RX2_DEFAULT_CHANNEL;
        mibJoin.Param.Rx2DefaultChannel.Frequency = 869525000;
        mibJoin.Param.Rx2DefaultChannel.Datarate = DR_3;
        LoRaMacMibSetRequestConfirm(&mibJoin);

        mibJoin.Type = MIB_RX2_CHANNEL;
        mibJoin.Param.Rx2Channel.Frequency = 869525000;
        mibJoin.Param.Rx2Channel.Datarate = DR_3;
        LoRaMacMibSetRequestConfirm(&mibJoin);

        Serial.println("Join: RX2=SF9, Link Check deaktiviert.");
      }
      break;
    }
    case DEVICE_STATE_SEND:
    {
      cycleCounter++;
      bool doSendTTN = (cycleCounter % 2 == 0);

      MibRequestConfirm_t mibReq;

      // =============================================================
      // SCHRITT 1: CS sendet IMMER (jeden Zyklus = alle 45s)
      // =============================================================
      sendToTTN = false;

      // Device Address -> CS
      mibReq.Type = MIB_DEV_ADDR;
      mibReq.Param.DevAddr = devAddr_CS;
      LoRaMacMibSetRequestConfirm(&mibReq);

      // Network Session Key -> CS
      mibReq.Type = MIB_NWK_SKEY;
      mibReq.Param.NwkSKey = nwkSKey_CS;
      LoRaMacMibSetRequestConfirm(&mibReq);

      // Application Session Key -> CS
      mibReq.Type = MIB_APP_SKEY;
      mibReq.Param.AppSKey = appSKey_CS;
      LoRaMacMibSetRequestConfirm(&mibReq);

      // SF9 (DR_3) fest für CS, ADR deaktiviert
      mibReq.Type = MIB_CHANNELS_DATARATE;
      mibReq.Param.ChannelsDatarate = DR_3;
      LoRaMacMibSetRequestConfirm(&mibReq);

      mibReq.Type = MIB_CHANNELS_DEFAULT_DATARATE;
      mibReq.Param.ChannelsDefaultDatarate = DR_3;
      LoRaMacMibSetRequestConfirm(&mibReq);

      mibReq.Type = MIB_ADR;
      mibReq.Param.AdrEnable = false;
      LoRaMacMibSetRequestConfirm(&mibReq);

      Serial.println("\n>> MAPPING: ChirpStack (SF9 fest, kein ADR) <<");

      if (prepareTxFrame(appPort)) {
        McpsReq_t mcpsReq;
        mcpsReq.Type = MCPS_UNCONFIRMED;
        mcpsReq.Req.Unconfirmed.fPort = appPort;
        mcpsReq.Req.Unconfirmed.fBuffer = appData;
        mcpsReq.Req.Unconfirmed.fBufferSize = appDataSize;
        mcpsReq.Req.Unconfirmed.Datarate = DR_3; // SF9 direkt
        LoRaMacStatus_t statusCS = LoRaMacMcpsRequest(&mcpsReq);
        logMcpsStatus("CS", statusCS);
      }

      // =============================================================
      // SCHRITT 2: TTN sendet nur jeden 2. Zyklus (alle 90s)
      // Pause dazwischen, damit RX-Windows vom CS-Paket abgeschlossen sind
      // =============================================================
      if (doSendTTN) {
        // Warte damit der MAC-Stack das CS-Paket abschließen kann
        // SF9 Airtime ~400ms + RX1 @+1s + RX2 @+2s (~400ms) = ~2.8s gesamt
        // 4s als sicherer Puffer statt 3s
        delay(4000);

        sendToTTN = true;

        // Device Address -> TTN
        mibReq.Type = MIB_DEV_ADDR;
        mibReq.Param.DevAddr = devAddr_TTN;
        LoRaMacMibSetRequestConfirm(&mibReq);

        // Network Session Key -> TTN
        mibReq.Type = MIB_NWK_SKEY;
        mibReq.Param.NwkSKey = nwkSKey_TTN;
        LoRaMacMibSetRequestConfirm(&mibReq);

        // Application Session Key -> TTN
        mibReq.Type = MIB_APP_SKEY;
        mibReq.Param.AppSKey = appSKey_TTN;
        LoRaMacMibSetRequestConfirm(&mibReq);

        // SF7 (DR_5) fest für TTN, ADR deaktiviert
        mibReq.Type = MIB_CHANNELS_DATARATE;
        mibReq.Param.ChannelsDatarate = DR_5;
        LoRaMacMibSetRequestConfirm(&mibReq);

        mibReq.Type = MIB_CHANNELS_DEFAULT_DATARATE;
        mibReq.Param.ChannelsDefaultDatarate = DR_5;
        LoRaMacMibSetRequestConfirm(&mibReq);

        mibReq.Type = MIB_ADR;
        mibReq.Param.AdrEnable = false;
        LoRaMacMibSetRequestConfirm(&mibReq);

        Serial.println("\n>> MAPPING: TTN (SF7 fest, kein ADR) <<");

        if (prepareTxFrame(appPort)) {
          McpsReq_t mcpsReq;
          mcpsReq.Type = MCPS_UNCONFIRMED;
          mcpsReq.Req.Unconfirmed.fPort = appPort;
          mcpsReq.Req.Unconfirmed.fBuffer = appData;
          mcpsReq.Req.Unconfirmed.fBufferSize = appDataSize;
          mcpsReq.Req.Unconfirmed.Datarate = DR_5; // SF7 direkt
          LoRaMacStatus_t statusTTN = LoRaMacMcpsRequest(&mcpsReq);
          logMcpsStatus("TTN", statusTTN);
        }
      }

      deviceState = DEVICE_STATE_CYCLE;
      break;
    }
    case DEVICE_STATE_CYCLE:
    {
      txDutyCycleTime = appTxDutyCycle + randr( 0, APP_TX_DUTYCYCLE_RND );
      LoRaWAN.cycle(txDutyCycleTime);
      deviceState = DEVICE_STATE_SLEEP;
      break;
    }
    case DEVICE_STATE_SLEEP:
    {
      LoRaWAN.sleep();
      break;
    }
    default:
    {
      deviceState = DEVICE_STATE_INIT;
      break;
    }
  }
}
