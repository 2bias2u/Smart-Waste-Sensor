/*
  ============================================================================
  ESP32-CAM Füllstand-Test (nur Konsolen-Ausgabe, kein LoRaWAN)
  ============================================================================
  Zweck: Erstmal nur die Bildauswertung isoliert testen und den berechneten
  Füllstand + Debug-Werte im Serial Monitor beobachten, bevor irgendwas mit
  LoRaWAN gemacht wird.

  Ablauf:
  - alle paar Sekunden ein Graustufen-Bild aufnehmen
  - vertikale Spalte in der Bildmitte scannen, größten Helligkeitssprung suchen
  - Füllstand in % + Debug-Infos (Helligkeitsprofil) über Serial ausgeben

  Board: AI-Thinker ESP32-CAM
  Arduino IDE Einstellungen: Board "AI Thinker ESP32-CAM", Partition Scheme
  "Huge APP (3MB No OTA/1MB SPIFFS)", Upload Speed 115200

  Zum Flashen: GPIO0 auf GND, Reset drücken, hochladen, GPIO0 wieder lösen,
  Reset drücken -> dann läuft der Sketch normal.
  ============================================================================
*/

#include "esp_camera.h"

// ---------------------------------------------------------------------------
// Kamera-Pinbelegung AI-Thinker ESP32-CAM
// ---------------------------------------------------------------------------
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ---------------------------------------------------------------------------
// Einstellungen zum Anpassen
// ---------------------------------------------------------------------------
const unsigned long MESS_INTERVALL_MS = 5000; // alle 5s eine neue Messung

// framesize FRAMESIZE_QVGA -> 320x240 Pixel
const int FRAME_WIDTH   = 320;
const int FRAME_HEIGHT  = 240;
const int SCAN_X        = FRAME_WIDTH / 2;   // mittlere Spalte, TODO: ggf. anpassen
const int SCAN_Y_START  = 10;
const int SCAN_Y_END    = FRAME_HEIGHT - 10;

// Schwelle für Kontrastsprung - erstmal niedrig ansetzen und anhand der
// gedruckten Helligkeitswerte iterativ anpassen
const int KONTRAST_SCHWELLE = 15;

// Bei true wird das komplette Helligkeitsprofil der Scan-Spalte ausgegeben
// (viel Text, aber gut zum Kalibrieren). Bei false nur das Endergebnis.
const bool DEBUG_PROFIL_AUSGEBEN = true;

unsigned long letzteMessung = 0;

// ---------------------------------------------------------------------------
// Kamera initialisieren
// ---------------------------------------------------------------------------
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_GRAYSCALE;
  config.frame_size   = FRAMESIZE_QVGA;
  config.fb_count     = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Kamera-Init fehlgeschlagen, Fehlercode: 0x%x\n", err);
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Füllstand berechnen + optional Debug-Profil ausgeben
// ---------------------------------------------------------------------------
void berechneUndZeigeFuellstand(camera_fb_t *fb) {
  int besteZeile = -1;
  int groessterAbfall = 0;

  if (DEBUG_PROFIL_AUSGEBEN) {
    Serial.println("--- Helligkeitsprofil (Zeile: Wert) ---");
  }

  for (int y = SCAN_Y_START; y < SCAN_Y_END - 1; y++) {
    int idx1 = y * FRAME_WIDTH + SCAN_X;
    int idx2 = (y + 1) * FRAME_WIDTH + SCAN_X;

    int helligkeit1 = fb->buf[idx1];
    int helligkeit2 = fb->buf[idx2];
    int abfall = helligkeit1 - helligkeit2;

    if (DEBUG_PROFIL_AUSGEBEN && (y % 5 == 0)) {
      Serial.printf("  y=%3d  helligkeit=%3d\n", y, helligkeit1);
    }

    if (abfall > groessterAbfall) {
      groessterAbfall = abfall;
      besteZeile = y;
    }
  }

  Serial.println("--- Ergebnis ---");
  Serial.printf("Groesster Helligkeitssprung: %d an Zeile %d\n", groessterAbfall, besteZeile);

  if (besteZeile < 0 || groessterAbfall < KONTRAST_SCHWELLE) {
    Serial.println(">>> Kein klarer Kontrastsprung gefunden (Schwelle nicht erreicht).");
    Serial.println(">>> Fuellstand: unbekannt (0%)");
    return;
  }

  float relativePosition = (float)(besteZeile - SCAN_Y_START) / (float)(SCAN_Y_END - SCAN_Y_START);
  int fillLevelPercent = (int)constrain(relativePosition * 100.0, 0, 100);

  Serial.printf(">>> Fuellstand: %d%%  (Konfidenz/Kontrast: %d)\n", fillLevelPercent, groessterAbfall);
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nStarte ESP32-CAM Fuellstand-Test...");

  if (!initCamera()) {
    Serial.println("Abbruch: Kamera konnte nicht initialisiert werden.");
    while (true) delay(1000);
  }

  Serial.println("Kamera bereit. Messung startet...\n");
  letzteMessung = millis();
}

void loop() {
  if (millis() - letzteMessung >= MESS_INTERVALL_MS) {
    letzteMessung = millis();

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Bild konnte nicht aufgenommen werden!");
      return;
    }

    berechneUndZeigeFuellstand(fb);

    esp_camera_fb_return(fb);
  }
}
