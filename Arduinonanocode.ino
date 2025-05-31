#include <WiFiNINA.h>
#include <ArduinoMqttClient.h>

// WiFi Credentials
const char ssid[] = "TelstraA4F1D3";
const char pass[] = "b29t6urac8";

// Adafruit IO MQTT
const char* broker = "io.adafruit.com";
const int port = 1883;
const char* username = "Thilakarathne";
const char* aioKey = "aio_ZTqo33yvRPEKNsnH5MOH5BfQj9tQ";
const char* topic = "Thilakarathne/feeds/moisture_data";

// Sensor pin
const int moisturePin = A0;

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.print("Connecting to WiFi...");
  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println(" connected!");

  mqttClient.setId("NanoMoistureSensor");
  mqttClient.setUsernamePassword(username, aioKey);

  Serial.print("Connecting to MQTT...");
  while (!mqttClient.connect(broker, port)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println(" connected!");
}

void loop() {
  int moisture = analogRead(moisturePin);
  Serial.print("Moisture: ");
  Serial.println(moisture);

  mqttClient.beginMessage(topic, true);  // true = retain
  mqttClient.print(moisture);
  mqttClient.endMessage();
  Serial.println("Moisture published (retained).");

  delay(10000);  // every 10 sec
}
