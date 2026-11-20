#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>

// Identifiants WiFi
const char *ssid = "iot";
const char *password = "iotisis;";

// Adresse IP du Broker MQTT
const char *mqtt_server = "172.18.32.46";

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;

const int ledPin = 25;     // LED avec sur GPIO 25
const int buttonPin = 26;  // Bouton poussoir sur GPIO 26

void setup_wifi();
void callback(char *topic, byte *message, unsigned int length);

void setup()
{
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  pinMode(ledPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
}

void setup_wifi()
{
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char *topic, byte *message, unsigned int length)
{
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  for (int i = 0; i < length; i++)
  {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Allume ou éteint la LED selon le message reçu sur esp32/output
  if (String(topic) == "esp32/output")
  {
    Serial.print("Changing output to ");
    if (messageTemp == "on")
    {
      Serial.println("LED on");
      digitalWrite(ledPin, HIGH);
    }
    else if (messageTemp == "off")
    {
      Serial.println("LED off");
      digitalWrite(ledPin, LOW);
    }
  }
}

void reconnect()
{
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Identifiant client unique pour la carte combinée
    if (client.connect("ESP32Client_Led_Btn"))
    {
      Serial.println("connected");
      client.subscribe("esp32/output");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void loop()
{
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 1000)
  {
    lastMsg = now;

    int buttonState = digitalRead(buttonPin);
    if (buttonState == LOW)
    {
      Serial.print("Publish message: ");
      String msg = "down";
      Serial.println(msg);
      client.publish("esp32/button1", msg.c_str());
    }
    else
    {
      Serial.print("Publish message: ");
      String msg = "up";
      Serial.println(msg);
      client.publish("esp32/button1", msg.c_str());
    }
  }
}