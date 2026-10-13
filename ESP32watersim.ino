#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_BMP085.h>


#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);


int waterPin = 34;
int ldrPin = 35;

Adafruit_BMP085 bmp;

const char* ssid = "";
const char* password = "";
const char* mqtt_server = "broker.emqx.io";
WiFiClient espClient;
PubSubClient client(espClient);

void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (int i = 0; i < length; i++) msg += (char)payload[i];

  if (msg == "ON") digitalWrite(LED_BUILTIN, HIGH);
  if (msg == "OFF") digitalWrite(LED_BUILTIN, LOW);
}


void setup_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  int counter = 0;

  while (WiFi.status() != WL_CONNECTED && counter < 20) {
    delay(500);
    Serial.print(".");
    counter++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connect timeout");
  }
}


void reconnect() {
  while (!client.connected()) {
    String cid = "ESP32_" + String(random(0xffff), HEX);

    if (client.connect(cid.c_str())) {
      Serial.println("MQTT Connected!");
      client.subscribe("esp32/command");
    } else {
      Serial.print("MQTT Failed, rc=");
      Serial.print(client.state());
      Serial.println(" retry in 2 seconds");
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  dht.begin();

  if (!bmp.begin()) {
    Serial.println("BMP180 Not Found!");
  } else {
    Serial.println("BMP180 Ready");
  }

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  randomSeed(micros());
}

void loop() {

  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi LOST... reconnecting");

    WiFi.disconnect(true);
    delay(500);

    WiFi.begin(ssid, password);
    delay(2000);

    return;
  }
 

 
  if (!client.connected()) {
    Serial.println("MQTT reconnecting...");
    reconnect();
  }

  client.loop();

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("DHT ERROR");
    return;
  }

  float t_added = t + 2; 
  float pressure = bmp.readPressure() / 100.0;
  float altitude = bmp.readAltitude();

  Serial.print("Pressure = ");
  Serial.println(pressure);

  Serial.print("Altitude = ");
  Serial.println(altitude);

 
  int waterValue = analogRead(waterPin);
  int ldrValue = analogRead(ldrPin);

  String wetLevel = (waterValue > 1500) ? "Rainning" : "No Rain";
  String lightLevel = (ldrValue > 2500) ? "Night" : "Morning";

  
  String payload = "{";
  payload += "\"temp\":" + String(t_added, 1) + ",";
  payload += "\"humidity\":" + String(h, 1) + ",";
  payload += "\"water\":" + String(waterValue) + ",";
  payload += "\"wet\":\"" + wetLevel + "\",";
  payload += "\"ldr\":" + String(ldrValue) + ",";
  payload += "\"light\":\"" + lightLevel + "\",";
  payload += "\"pressure\":" + String(pressure, 1) + ",";
  payload += "\"altitude\":" + String(altitude, 1);
  payload += "}";

  Serial.println("JSON:");
  Serial.println(payload);

  client.publish("esp32/sensors", payload.c_str());

  delay(2000);
}
