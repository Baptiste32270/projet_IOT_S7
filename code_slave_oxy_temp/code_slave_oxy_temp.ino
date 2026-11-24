#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include "DFRobot_BloodOxygen_S.h"

// =====================================
// 1. CONFIGURATION I2C (SLAVE & MASTER)
// =====================================
// Bus Wire (SDA = IO21, SCL = IO22) -> ESP32 en mode Esclave
#define I2C_SLAVE_ADDR 0x55

// Bus Wire1 (SDA = D3/IO26, SCL = D4/IO27) -> ESP32 en mode Maître pour le MAX30102
#define MAX30102_ADDR  0x57
#define WIRE1_SDA      26
#define WIRE1_SCL      27

DFRobot_BloodOxygen_S_I2C MAX30102(&Wire1, MAX30102_ADDR);
bool max30102Ready = false;

// =====================================
// 2. TABLE DES CODES ERREUR
// =====================================
#define ERR_NONE              0
#define ERR_WIFI_LOST         1
#define ERR_SENSOR_DISCONNECT 2
#define ERR_SENSOR_READ       3
#define ERR_TEMP_LIMIT        4
#define ERR_LOW_VOLTAGE       5
#define ERR_SERIAL_LOST       6
#define ERR_LOW_MEMORY        7
#define ERR_I2C               8
#define ERR_WATCHDOG          9
#define ERR_UNKNOWN           10

volatile uint8_t errorCode = ERR_NONE;

// Drapeaux d'état indépendants pour éviter les conflits d'erreurs
bool flagSensorDisconnect = false;
bool flagSensorReadError  = false;
bool flagWifiLost         = false;
bool flagTempLimit        = false;

void updateErrorCode() {
  if (flagSensorDisconnect) {
    errorCode = ERR_SENSOR_DISCONNECT;
  } else if (flagSensorReadError) {
    errorCode = ERR_SENSOR_READ;
  } else if (flagWifiLost) {
    errorCode = ERR_WIFI_LOST;
  } else if (flagTempLimit) {
    errorCode = ERR_TEMP_LIMIT;
  } else {
    errorCode = ERR_NONE;
  }
}

// =====================================
// 3. CONFIGURATION DHT11, LED, WIFI & MQTT
// =====================================
#define DHTPIN  25 // Broche D2 sur FireBeetle
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

const int ledPin = 4; // Broche DO sur FireBeetle

const char *ssid = "iot";
const char *password = "iotisis;";
const char *mqtt_server = "172.18.32.46";

WiFiClient espClient;
PubSubClient client(espClient);

// Timers non-bloquants
unsigned long lastDhtMsg = 0;
unsigned long lastMaxMsg = 0;
unsigned long lastMqttReconnect = 0;

// =====================================
// CALLBACKS I2C SLAVE (Sur bus Wire)
// =====================================
void onReceive(int len) {
  Serial.print("Commande reçue du Master I2C : ");
  while (Wire.available()) {
    Serial.write(Wire.read());
  }
  Serial.println();
}

void onRequest() {
  Wire.write(errorCode);
  if (errorCode == ERR_NONE) {
    Serial.println("Statut transmis au Master I2C : OK (0)");
  } else {
    Serial.print("Code erreur transmis au Master I2C : ");
    Serial.println(errorCode);
  }
}

// =====================================
// FONCTIONS WIFI & MQTT
// =====================================
void setup_wifi() {
  delay(10);
  Serial.print("Connexion au WiFi ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nWiFi connecté - IP : ");
    Serial.println(WiFi.localIP());
    flagWifiLost = false;
  } else {
    Serial.println("\nÉchec connexion WiFi");
    flagWifiLost = true;
  }
  updateErrorCode();
}

void callback(char *topic, byte *message, unsigned int length) {
  Serial.print("Message MQTT reçu [");
  Serial.print(topic);
  Serial.print("] : ");

  String messageTemp;
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  if (String(topic) == "esp32/output") {
    if (messageTemp == "on" || messageTemp == "ON" || messageTemp == "1") {
      digitalWrite(ledPin, HIGH);
    } else if (messageTemp == "off" || messageTemp == "OFF" || messageTemp == "0") {
      digitalWrite(ledPin, LOW);
    }
  }
}

void reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    flagWifiLost = true;
    updateErrorCode();
    WiFi.reconnect();
    return;
  }

  flagWifiLost = false;
  updateErrorCode();

  Serial.print("Tentative de connexion MQTT...");
  if (client.connect("ESP32Client_Fusion")) {
    Serial.println("connecté");
    client.subscribe("esp32/output");
  } else {
    Serial.print("échec, rc=");
    Serial.println(client.state());
  }
}

// =====================================
// INITIALISATION
// =====================================
void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);

  // 1. Initialisation I2C Esclave (Wire : SDA=IO21, SCL=IO22)
  Wire.onReceive(onReceive);
  Wire.onRequest(onRequest);
  Wire.begin((uint8_t)I2C_SLAVE_ADDR);
  Serial.println("I2C Slave initialisé sur l'adresse 0x55");

  // 2. Initialisation I2C Maître (Wire1 : SDA=D3/IO26, SCL=D4/IO27)
  Wire1.begin(WIRE1_SDA, WIRE1_SCL);
  Wire1.setTimeOut(50); // Évite le blocage si le capteur est débranché en direct
  pinMode(WIRE1_SDA, INPUT_PULLUP);
  pinMode(WIRE1_SCL, INPUT_PULLUP);

  int sensorTries = 0;
  while (!MAX30102.begin() && sensorTries < 5) {
    Serial.println("MAX30102 init fail!");
    delay(500);
    sensorTries++;
  }

  if (sensorTries < 5) {
    Serial.println("MAX30102 init success!");
    MAX30102.sensorStartCollect();
    max30102Ready = true;
    flagSensorDisconnect = false;
  } else {
    Serial.println("MAX30102 non détecté (ERR_SENSOR_DISCONNECT)");
    flagSensorDisconnect = true;
  }
  updateErrorCode();

  // 3. Initialisation DHT11
  dht.begin();

  // 4. Initialisation WiFi & MQTT
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

// =====================================
// BOUCLE PRINCIPALE
// =====================================
void loop() {
  unsigned long now = millis();

  // Surveillance continue de l'état WiFi et MQTT
  if (WiFi.status() != WL_CONNECTED) {
    if (!flagWifiLost) {
      flagWifiLost = true;
      updateErrorCode();
      Serial.println("ALERTE : Connexion WiFi perdue !");
    }
  } else if (flagWifiLost) {
    flagWifiLost = false;
    updateErrorCode();
    Serial.println("WiFi rétabli !");
  }

  if (!client.connected()) {
    if (now - lastMqttReconnect >= 5000) {
      lastMqttReconnect = now;
      reconnectMQTT();
    }
  } else {
    client.loop();
  }

  // Tâche 1 : Lecture DHT11 toutes les 2 secondes
  if (now - lastDhtMsg >= 2000) {
    lastDhtMsg = now;

    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();

    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Erreur de lecture du capteur DHT11 !");
      flagSensorReadError = true;
    } else {
      flagSensorReadError = false;
      flagTempLimit = (temperature > 40.0);

      Serial.printf("DHT11 -> Temp: %.1f °C | Hum: %.1f %%\n", temperature, humidity);

      if (client.connected()) {
        client.publish("esp32/DHT11/temperature", String(temperature).c_str());
        client.publish("esp32/DHT11/humidity", String(humidity).c_str());
      }
    }
    updateErrorCode();
  }

  // Tâche 2 : Détection dynamique + Lecture directe du MAX30102 toutes les 4 secondes
  if (now - lastMaxMsg >= 4000) {
    lastMaxMsg = now;

    Wire1.beginTransmission(MAX30102_ADDR);
    uint8_t i2cStatus = Wire1.endTransmission();

    if (i2cStatus != 0) {
      if (max30102Ready || !flagSensorDisconnect) {
        Serial.println("ALERTE : Oxymètre MAX30102 déconnecté !");
      }
      max30102Ready = false;
      flagSensorDisconnect = true;
    } else {
      if (!max30102Ready) {
        if (MAX30102.begin()) {
          MAX30102.sensorStartCollect();
          max30102Ready = true;
          Serial.println("Oxymètre MAX30102 reconnecté !");
        }
      }

      if (max30102Ready) {
        flagSensorDisconnect = false;

        MAX30102.getHeartbeatSPO2();
        int spo2 = MAX30102._sHeartbeatSPO2.SPO2;
        int heartbeat = MAX30102._sHeartbeatSPO2.Heartbeat;
        float boardTemp = MAX30102.getTemperature_C();

        Serial.printf("MAX30102 -> SPO2: %d %% | BPM: %d | Temp carte: %.1f °C\n",
                      spo2, heartbeat, boardTemp);

        if (client.connected()) {
          if (spo2 > 0 && heartbeat > 0) {
            client.publish("esp32/slave/spo2", String(spo2).c_str());
            client.publish("esp32/slave/bpm", String(heartbeat).c_str());
          }
          client.publish("esp32/slave/temperature", String(boardTemp).c_str());
        }
      }
    }
    updateErrorCode();
  }
}