#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h> 

// WLAN Zugangsdaten
//const char* ssid = "Steini_IoT";
//const char* password = "Smartesheim23";

const char* ssid = "LSWI";
const char* password = "geeS6ico";

// MQTT Broker Einstellungen
const char* mqtt_server = "192.168.1.137"; // IP-Adresse oder URL deines Brokers
const int mqtt_port = 1883;
const char* mqtt_topic = "sensor/ultraschall"; // Das Topic, auf dem gesendet wird
const char* mqtt_user = "luke";
const char* mqtt_pass = "luke1998";

// Pins Ultraschall 1
const int TRIG1 = 25;
const int ECHO1 = 26;

// Pins Ultraschall 2
const int TRIG2 = 14;
const int ECHO2 = 27;

WiFiClient espClient;
PubSubClient client(espClient);

// Funktion, die prüft, ob echtes Internet vorhanden ist
bool checkInternet() {
  // Falls das WLAN schon offline ist, brauchen wir gar nicht weiter prüfen
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClient testClient;
  // Wir versuchen, eine kurze Verbindung zu Google (Port 80) aufzubauen
  // Das Timeout setzen wir auf 1500ms, damit der Code nicht ewig blockiert
  if (testClient.connect("www.google.com", 80)) {
    testClient.stop();
    return true; // Verbindung erfolgreich -> Internet ist da!
  }
  return false; // Keine Verbindung -> Kein Internet
}

// Funktion für den WLAN-Verbindungsaufbau
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Verbinde mit WLAN: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWLAN verbunden!");
  Serial.print("IP-Adresse: ");
  Serial.println(WiFi.localIP());
}

// Funktion zur Verbindung mit dem MQTT-Broker
void reconnect() {
  while (!client.connected()) {
    // Bevor wir MQTT versuchen, prüfen wir erst, ob das Internet/Netzwerk generell steht
    if (!checkInternet()) {
      Serial.println("Kein Internet verfügbar. Warte 5 Sekunden...");
      delay(5000);
      continue; // Springt an den Anfang der Schleife und prüft erneut
    }

    Serial.print("Versuche MQTT-Verbindung...");
    String clientId = "ESP32Client-";
    clientId += String(random(0, 0xffff), HEX);
    
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("verbunden!");
    } else {
      Serial.print("Fehlgeschlagen, RC=");
      Serial.print(client.state());
      Serial.println(" Nächster Versuch in 5 Sekunden");
      delay(5000);
    }
  }
}

// Funktion zum Messen der Distanz
float getDistance(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long duration = pulseIn(echo, HIGH, 30000); 
  if (duration == 0) return 400.0;           
  return duration * 0.034 / 2;
}

void setup() {
  Serial.begin(115200);
  
  pinMode(TRIG1, OUTPUT); pinMode(ECHO1, INPUT);
  pinMode(TRIG2, OUTPUT); pinMode(ECHO2, INPUT);
  
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

  Serial.println("System bereit!");
}

void loop() {
  // 1. WLAN & Internet-Status prüfen, falls nicht verbunden
  if (WiFi.status() != WL_CONNECTED) {
    setup_wifi();
  }

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // 2. Beide Distanzen messen
  float d1 = getDistance(TRIG1, ECHO1);
  delay(50); 
  float d2 = getDistance(TRIG2, ECHO2);
  float minDistance = min(d1, d2);

  // 3. Ausgabe auf Seriellem Monitor
  Serial.print("S1: "); Serial.print(d1);
  Serial.print("cm | S2: "); Serial.print(d2);
  Serial.print("cm | MIN: "); Serial.print(minDistance);
  Serial.println("cm");

  // 4. Daten via MQTT senden (nur wenn die Verbindung steht)
  if (client.connected()) {
    String payload = "{\"s1\":" + String(d1) + ",\"s2\":" + String(d2) + "}";
    if (client.publish(mqtt_topic, payload.c_str())) {
      Serial.println("MQTT Nachricht erfolgreich gesendet.");
    } else {
      Serial.println("Fehler beim S
  delay(5000); 
}