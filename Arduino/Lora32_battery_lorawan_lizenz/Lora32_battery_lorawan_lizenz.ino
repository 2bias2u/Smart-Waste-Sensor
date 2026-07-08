/***************************************************************************
  Battery-Reporting-Sketch (LoRaWAN, ABP/OTAA)

  Sendet nur den Akkustand (Batteriespannung in mV) per LoRaWAN.
  Alle Sensor-Teile (BME280, CCS811, MQ7) wurden entfernt.

  Plattform: Heltec CubeCell (Mcu.begin(HELTEC_BOARD, ...))
  Batteriespannung wird über die eingebaute getBatteryVoltage()-Funktion
  der CubeCell-BSP gelesen.
***************************************************************************/

#include "LoRaWan_APP.h"
#include <Arduino.h>

/* Heltec-Lizenz für dieses Board (gebunden an die Chip-ID).
   Unter http://www.heltec.cn/search/ mit der ChipID aus dem Serial Monitor
   abrufen und hier die vier Werte eintragen (Reihenfolge wie erhalten). 
   ins Terminal mit "New Line": AT+CDKEY=5F7096A1FE6E00E89C39EBD2190D58C3 ENTER*/

uint32_t license[4] = { 0x5F7096A1,0xFE6E00E8,0x9C39EBD2,0x190D58C3 };

/* OTAA para*/
uint8_t devEui[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };    //hier Ihre devEUI einfügen
uint8_t appEui[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };    //hier Ihre appEUI einfügen
uint8_t appKey[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };  //hier Ihre appKey einfügen

/* ABP para*/
uint8_t nwkSKey[] = { 0xC7, 0x98, 0x07, 0x5C, 0xF1, 0x88, 0xDC, 0x38, 0x8B, 0xD8, 0xB8, 0x5B, 0x49, 0x2F, 0xA2, 0xA3 };
uint8_t appSKey[] = { 0xCD, 0xF2, 0x58, 0xB4, 0xD8, 0x94, 0xE6, 0xFF, 0x4E, 0x2D, 0x38, 0x10, 0x39, 0xD8, 0x10, 0x64 };
uint32_t devAddr =  ( uint32_t )0x260BCA14;

/*LoraWan channelsmask, default channels 0-7*/
uint16_t userChannelsMask[6]={ 0x00FF,0x0000,0x0000,0x0000,0x0000,0x0000 };

/*LoraWan region, select in arduino IDE tools*/
LoRaMacRegion_t loraWanRegion = ACTIVE_REGION;

/*LoraWan Class, Class A and Class C are supported*/
DeviceClass_t  loraWanClass = CLASS_A;

/*the application data transmission duty cycle.  value in [ms].*/
uint32_t appTxDutyCycle = 15000;

/*OTAA or ABP*/
bool overTheAirActivation = false;

/*ADR enable*/
bool loraWanAdr = true;

/* Indicates if the node is sending confirmed or unconfirmed messages */
bool isTxConfirmed = true;

/* Application port */
uint8_t appPort = 2;

/*!
* Number of trials to transmit the frame, if the LoRaMAC layer did not
* receive an acknowledgment.
*/
uint8_t confirmedNbTrials = 4;

//////// Messdaten-Platzhalter ////////
uint16_t batteryVoltage_mV = 0;

static void prepareTxFrame( uint8_t port )
{
  /*appData size is LORAWAN_APP_DATA_MAX_SIZE which is defined in "commissioning.h".
  *appDataSize max value is LORAWAN_APP_DATA_MAX_SIZE.
  *if enabled AT, don't modify LORAWAN_APP_DATA_MAX_SIZE, it may cause system hanging or failure.
  *if disabled AT, LORAWAN_APP_DATA_MAX_SIZE can be modified.
  */

  // Batteriespannung lesen (in mV)
  batteryVoltage_mV = BoardGetBatteryVoltage();

  // Payload: 2 Bytes, Big-Endian uint16 (mV)
  appDataSize = 2;
  appData[0] = (uint8_t)(batteryVoltage_mV >> 8);
  appData[1] = (uint8_t)(batteryVoltage_mV & 0xFF);
}

//////////////////////
// Setup            //
//////////////////////
void setup() {
  Serial.begin(115200);
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  delay(100);

  Serial.println("Battery-Reporting-Setup gestartet...");
  Serial.println("Setup abgeschlossen.");
}

//////////////////////
// Loop             //
//////////////////////
void loop() {

  switch( deviceState )
  {
    case DEVICE_STATE_INIT:
    {
#if(LORAWAN_DEVEUI_AUTO)
      LoRaWAN.generateDeveuiByChipID();
#endif
      LoRaWAN.init(loraWanClass,loraWanRegion);
      //both set join DR and DR when ADR off
      LoRaWAN.setDefaultDR(3);
      break;
    }
    case DEVICE_STATE_JOIN:
    {
      LoRaWAN.join();
      break;
    }
    case DEVICE_STATE_SEND:
    {
      prepareTxFrame( appPort );

      Serial.println("==== Battery-Daten ====");
      Serial.print("Batteriespannung: ");
      Serial.print(batteryVoltage_mV);
      Serial.println(" mV");
      Serial.println();

      LoRaWAN.send();
      deviceState = DEVICE_STATE_CYCLE;
      break;
    }
    case DEVICE_STATE_CYCLE:
    {
      // Schedule next packet transmission
      txDutyCycleTime = appTxDutyCycle + randr( -APP_TX_DUTYCYCLE_RND, APP_TX_DUTYCYCLE_RND );
      LoRaWAN.cycle(txDutyCycleTime);
      deviceState = DEVICE_STATE_SLEEP;
      break;
    }
    case DEVICE_STATE_SLEEP:
    {
      LoRaWAN.sleep(loraWanClass);
      break;
    }
    default:
    {
      deviceState = DEVICE_STATE_INIT;
      break;
    }
  }
}
