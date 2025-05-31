#include <WiFiS3.h>
#include <ArduinoMqttClient.h>
#include <WiFiClient.h>
#include <HttpClient.h>

// WiFi
const char ssid[] = "TelstraA4F1D3";
const char pass[] = "b29t6urac8";

// MQTT (Adafruit IO)
const char* broker = "io.adafruit.com";
const int port = 1883;
const char* username = "Thilakarathne";
const char* aioKey = "aio_ZTqo33yvRPEKNsnH5MOH5BfQj9tQ";
const char* topicManual = "Thilakarathne/feeds/manual_water";
const char* topicMoisture = "Thilakarathne/feeds/moisture_data";

// IFTTT
const char* iftttHost = "maker.ifttt.com";
const char* iftttKey = "j5_3DTKaNLGAJwCDTKcFb_lw2mrGK6GQCuF1EfurPby";

// REST
const char* restHost = "io.adafruit.com";
const int restPort = 80;

// Pin Config
const int relayPin = 6;
const int trigPin = 3;
const int echoPin = 2;

// Thresholds
const int dryThreshold = 800;
const int wetThreshold = 900;
const int stoppoint = 850;
const int waterLevelThreshold = 7;

WiFiClient wifiClient;
HttpClient httpClient(wifiClient, iftttHost, 80);
MqttClient mqttClient(wifiClient);

int moistureValue = 0;
bool isWatering = false;
bool receivedMqttData = false;

void setup() {
  Serial.begin(115200);
  pinMode(relayPin, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  digitalWrite(relayPin, LOW);

  connectToWiFi();
  connectToMQTT();
}

void loop() {
  if (!mqttClient.connected()) {
    connectToMQTT();
  }

  mqttClient.poll();  // This must be called often to receive MQTT messages

  if (!receivedMqttData) {
    fetchMoistureViaHTTP();  // fallback
  }

  checkMoisture();

  receivedMqttData = false;  // reset for next loop
  delay(10000);
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println(" connected!");
}

void connectToMQTT() {
  Serial.print("Connecting to MQTT...");
  mqttClient.setId("UnoClient");
  mqttClient.setUsernamePassword(username, aioKey);
  mqttClient.onMessage(onMqttMessage);  // register callback

  while (!mqttClient.connect(broker, port)) {
    Serial.print(".");
    delay(1000);
  }

  mqttClient.subscribe(topicManual);
  mqttClient.subscribe(topicMoisture);
  Serial.println(" MQTT connected and subscribed.");
}

void onMqttMessage(int messageSize) {
  String topic = mqttClient.messageTopic();
  String message = mqttClient.readString();
  message.trim();

  Serial.print("Received topic: ");
  Serial.println(topic);
  Serial.print("Payload: ");
  Serial.println(message);

  if (topic == topicMoisture) {
    moistureValue = message.toInt();
    receivedMqttData = true;
    Serial.print("Moisture received: ");
    Serial.println(moistureValue);
  }

  if (topic == topicManual) {
    message.toUpperCase();
    Serial.println("Manual command received: " + message);

    int level = getWaterLevel();
    if (message == "ON" && moistureValue < wetThreshold && level < waterLevelThreshold) {
      startWatering();
      unsigned long start = millis();
      while ((millis() - start < 30000) && moistureValue < wetThreshold && getWaterLevel() < waterLevelThreshold) {
        delay(5000);
      }
      stopWatering();
    } else {
      Serial.println("Manual watering blocked.");
    }
  }
}

void fetchMoistureViaHTTP() {
  Serial.println("Fetching moisture from Adafruit REST API...");

  WiFiClient restClientWiFi;
  HttpClient restClient(restClientWiFi, restHost, restPort);

 String url = "/api/v2/Thilakarathne/feeds/moisture-data/data/last";


  restClient.beginRequest();
  restClient.get(url);
  restClient.sendHeader("Host", restHost);
  restClient.sendHeader("X-AIO-Key", aioKey);
  restClient.sendHeader("Connection", "close");
  restClient.endRequest();

  int status = restClient.responseStatusCode();
  if (status == 200) {
    String response = restClient.responseBody();
    Serial.print("API Response: ");
    Serial.println(response);

    int index = response.indexOf("\"value\":\"");
    if (index != -1) {
      int start = index + 9;
      int end = response.indexOf("\"", start);
      String valueStr = response.substring(start, end);
      moistureValue = valueStr.toInt();
      Serial.print("Fetched moisture via HTTP: ");
      Serial.println(moistureValue);
    } else {
      Serial.println("Error parsing value from response.");
    }
  } else {
    Serial.print("HTTP error: ");
    Serial.println(status);
  }
}

void checkMoisture() {
  int level = getWaterLevel();

  Serial.print("Moisture: ");
  Serial.println(moistureValue);
  Serial.print("Water Level: ");
  Serial.print(level);
  Serial.println(" cm");

  if (moistureValue < dryThreshold && level < waterLevelThreshold) {
    sendIFTTTEvent("moisture_low");
    startWatering();

    unsigned long start = millis();
    while ((millis() - start < 30000) && moistureValue < stoppoint && getWaterLevel() < waterLevelThreshold) {
      delay(5000);
    }

    stopWatering();
  }

  if (level >= waterLevelThreshold) {
    sendIFTTTEvent("water_low");
    stopWatering();
  }
}

int getWaterLevel() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  int cm = duration * 0.034 / 2;
  return cm;
}

void startWatering() {
  if (!isWatering) {
    digitalWrite(relayPin, HIGH);
    isWatering = true;
    Serial.println("Pump ON");
  }
}

void stopWatering() {
  if (isWatering) {
    digitalWrite(relayPin, LOW);
    isWatering = false;
    Serial.println("Pump OFF");
  }
}

void sendIFTTTEvent(const char* event) {
  String url = "/trigger/";
  url += event;
  url += "/with/key/";
  url += iftttKey;

  httpClient.get(url);
  int statusCode = httpClient.responseStatusCode();
  String response = httpClient.responseBody();

  Serial.print("IFTTT ");
  Serial.print(event);
  Serial.print(" status: ");
  Serial.println(statusCode);
}
