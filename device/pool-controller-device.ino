#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"
#include <PubSubClient.h>

#include <SPIFFS.h>

#define MQTT_KEEP_ALIVE (900)
#define MQTT_TIMEOUT    (900)

/// @brief 
struct Configuration {
  char deviceName[32];
  char awsEndpoint[128];
  int awsEndpointPort;
  char wifiSsid[32];
  char wifiPassword[32];
};

Configuration config;
WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);

// Function Declarations
Configuration loadConfig();
void connectWiFi(const char* ssid, const char* password);
bool connectAWS(const char* awsEndpoint, int awsEndpointPort);
void reconnect(const char* deviceName);
void publishMessage();
void callback(char* topic, byte* payload, unsigned int length);

// The MQTT topics that this device should publish/subscribe
#define TOPIC_ACTION_SET_SPEED  "action/speed"
#define AWS_IOT_PUBLISH_TOPIC   "esp32/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/sub"

// Get the device ready
void setup() {
  Serial.begin(115200);
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  config = loadConfig();
  connectWiFi(config.wifiSsid, config.wifiPassword);
  connectAWS(config.awsEndpoint, config.awsEndpointPort);
}

// Do what you do...
void loop() {

  if (!client.connected()) {
    reconnect(config.deviceName);
  }

  client.loop();  
}


Configuration loadConfig() {
  Configuration config;

  // Load config file
  File confFile = SPIFFS.open("/config.json", "r");
  if (!confFile) {
    Serial.println("Failed to open config file ");
    return config;
  }
  else
    Serial.println("Success to open config file");

  StaticJsonDocument<256> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, confFile);
  confFile.close();

  // Test if parsing succeeds.
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return config;
  }

  strlcpy(config.deviceName,
          doc["device-name"] | "unk-device-name",
          sizeof(config.deviceName));

  strlcpy(config.awsEndpoint,
          doc["aws-endpoint"] | "unk-aws-endpoint",
          sizeof(config.awsEndpoint));
  
  config.awsEndpointPort = doc["aws-endpoint-port"] | 8883;

  strlcpy(config.wifiSsid,
          doc["wifi-ssid"] | "unk-wifi-ssid",
          sizeof(config.wifiSsid));

  strlcpy(config.wifiPassword,
          doc["wifi-password"] | "unk-wifi-password",
          sizeof(config.wifiPassword));

  return config;

}

void connectWiFi(const char* ssid, const char* password) {

  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

}

bool connectAWS(const char* awsEndpoint, int awsEndpointPort) {
  String certificate = "/cert.pem";
  String privateKey = "/private.key.pem";
  String caCertificate = "/ca.pem";

  // Load certificate file
  File cert = SPIFFS.open(certificate, "r"); //replace cert.crt eith your uploaded file name
  if (!cert) {
    Serial.println("Failed to open cert file");
    return false;
  }

  if (!net.loadCertificate(cert, cert.size())) {
    Serial.println("cert not loaded");
    return false;
  }
  cert.close();

  // Load private key file
  File private_key = SPIFFS.open(privateKey, "r"); //replace private eith your uploaded file name
  if (!private_key) {
    Serial.println("Failed to open private cert file");
    return false;
  }

  if (!net.loadPrivateKey(private_key, private_key.size())) {
    Serial.println("private key not loaded");
    return false;
  }
  private_key.close();

  // Load CA file
  File ca = SPIFFS.open(caCertificate, "r"); //replace ca eith your uploaded file name
  if (!ca) {
    Serial.println("Failed to open ca ");
    return false;
  }

  if(!net.loadCACert(ca, ca.size())) {
  Serial.println("ca not loaded");
  return false;
  }
  ca.close();

  client.setServer(awsEndpoint, awsEndpointPort);
  client.setCallback(callback);
}

void reconnect(const char* deviceName) {

  // Loop until we're reconnected
  while (!client.connected()) {
    client.setKeepAlive(MQTT_KEEP_ALIVE);
    client.setSocketTimeout(MQTT_TIMEOUT);
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(deviceName)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(AWS_IOT_PUBLISH_TOPIC, "hello world");
      
      // ... and resubscribe
      client.subscribe(TOPIC_ACTION_SET_SPEED);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");

      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void publishMessage()
{
  StaticJsonDocument<200> doc;
  doc["time"] = millis();
  doc["sensor_a0"] = analogRead(0);
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client

  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  if (strcmp(topic, TOPIC_ACTION_SET_SPEED) == 0) {
    Serial.printf("Received %s message!", TOPIC_ACTION_SET_SPEED);
  } else {
    Serial.printf("Received message on topic %s", topic);
  }


  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  StaticJsonDocument<200> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, payload);

  // Test if parsing succeeds.
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  }

  // Fetch values.
  //
  // Most of the time, you can rely on the implicit casts.
  // In other case, you can do doc["time"].as<long>();
  const char* message = doc["message"];

  Serial.print("message --> ");
  Serial.println(message);

}

