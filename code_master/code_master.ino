#include <Arduino.h>
#include <Wire.h>

#define I2C_DEV_ADDR 0x55

// =====================================
// TABLE DES CODES ERREUR (IDENTIQUE AU SLAVE)
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

int16_t lastErrorCode = -1;
unsigned long lastPollTime = 0;
const unsigned long POLL_INTERVAL = 2000; // Interrogation toutes les 2 secondes

// =====================================
// CORRESPONDANCE CODE / DIAGNOSTIC
// =====================================
const char* getSeverityTag(uint8_t code) {
  switch (code) {
    case ERR_NONE:
      return "[OK]";
    case ERR_TEMP_LIMIT:
    case ERR_LOW_VOLTAGE:
    case ERR_LOW_MEMORY:
      return "[ALERTE]";
    default:
      return "[CRITIQUE]";
  }
}

const char* getErrorMessage(uint8_t code) {
  switch (code) {
    case ERR_NONE:
      return "Fonctionnement nominal : acquisition capteur en cours";
    case ERR_WIFI_LOST:
      return "Impossible de se connecter au Wifi";
    case ERR_SENSOR_DISCONNECT:
      return "Probleme liaison capteur (capteur deconnecte)";
    case ERR_SENSOR_READ:
      return "Probleme liaison capteur (erreur de lecture)";
    case ERR_TEMP_LIMIT:
      return "Alerte thermique : temperature hors limites";
    case ERR_LOW_VOLTAGE:
      return "Defaut alimentation : tension insuffisante";
    case ERR_SERIAL_LOST:
      return "Defaut communication : perte liaison serie";
    case ERR_LOW_MEMORY:
      return "Alerte systeme : memoire insuffisante";
    case ERR_I2C:
      return "Defaut bus interne : erreur communication I2C";
    case ERR_WATCHDOG:
      return "Defaut systeme : Watchdog declenche";
    case ERR_UNKNOWN:
      return "Erreur systeme inconnue";
    default:
      return "Code defaut non repertorie";
  }
}

// =====================================
// INITIALISATION
// =====================================
void setup() {
  Serial.begin(115200);
  Wire.begin(); // SDA = IO21, SCL = IO22 par défaut

  Serial.println("==================================================");
  Serial.println("   VALISE DE DIAGNOSTIC ESP32 - PRETE (I2C 0x55)  ");
  Serial.println("   Tapez un message dans le moniteur pour l'envoyer");
  Serial.println("==================================================");
}

// =====================================
// BOUCLE PRINCIPALE
// =====================================
void loop() {
  // 1. Envoi d'une commande au Nœud Principal si saisie dans le Moniteur Série
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() > 0) {
      Wire.beginTransmission((uint8_t)I2C_DEV_ADDR);
      Wire.write((const uint8_t*)cmd.c_str(), cmd.length());
      uint8_t status = Wire.endTransmission();

      Serial.print(">> Envoi commande au Noeud Principal [");
      Serial.print(cmd);
      Serial.println(status == 0 ? "] : SUCCES" : "] : ECHEC I2C");
    }
  }

  // 2. Interrogation périodique de l'état du Nœud Principal
  unsigned long now = millis();
  if (now - lastPollTime >= POLL_INTERVAL) {
    lastPollTime = now;

    uint8_t bytesReceived = Wire.requestFrom((uint8_t)I2C_DEV_ADDR, (uint8_t)1);

    if (bytesReceived == 1 && Wire.available()) {
      uint8_t receivedCode = Wire.read();

      if (receivedCode != lastErrorCode) {
        Serial.println("\n>>> CHANGEMENT D'ETAT DETECTE <<<");
        lastErrorCode = receivedCode;
      }

      Serial.println("--------------------------------------------------");
      Serial.printf("Statut     : %s (Code 0x%02X)\n", getSeverityTag(receivedCode), receivedCode);
      Serial.print("Diagnostic : ");
      Serial.println(getErrorMessage(receivedCode));
      Serial.println("--------------------------------------------------");
    } else {
      if (lastErrorCode != -2) {
        Serial.println("\n>>> PERTE DE LIAISON DIAGNOSTIC <<<");
        lastErrorCode = -2;
      }
      Serial.println("--------------------------------------------------");
      Serial.println("Statut     : [CRITIQUE]");
      Serial.println("Diagnostic : Aucune reponse du Noeud Principal (verifier bus I2C / alimentation)");
      Serial.println("--------------------------------------------------");
    }
  }
}