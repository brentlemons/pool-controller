#include "secrets.h"
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"
#include <PubSubClient.h>

#include <SPIFFS.h>

#define MQTT_KEEP_ALIVE (900)
#define MQTT_TIMEOUT    (900)

// The MQTT topics that this device should publish/subscribe
#define AWS_IOT_PUBLISH_TOPIC   "esp32/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/sub"

const String AWSThingId = "pool-controller"; // unique thing identifier for AWS IoT

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
//  StaticJsonDocument<200> doc;
//  deserializeJson(doc, payload);
//  const char* message = doc["message"];
}

WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(AWS_IOT_ENDPOINT, 8883, callback, net); //set  MQTT port number to 8883 as per //standard


void configureWiFi() {

  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

}

void reconnect() {

  // Loop until we're reconnected
  while (!client.connected()) {
    client.setKeepAlive(MQTT_KEEP_ALIVE);
    client.setSocketTimeout(MQTT_TIMEOUT);
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(AWSThingId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(AWS_IOT_PUBLISH_TOPIC, "hello world");
      // ... and resubscribe
      client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");

      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void connectAWS()
{

  String certificate = "/cert.pem";
  String privateKey = "/private.key.pem";
  String caCertificate = "/ca.pem";

  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  // Load certificate file
  File cert = SPIFFS.open(certificate, "r"); //replace cert.crt eith your uploaded file name
  if (!cert) {
    Serial.println("Failed to open cert file");
  }
  else
    Serial.print("Success to open cert file: ");
    Serial.println(cert.size());

  delay(1000);

  if (net.loadCertificate(cert, cert.size()))
    Serial.println("cert loaded");
  else
    Serial.println("cert not loaded");

  // Load private key file
  File private_key = SPIFFS.open(privateKey, "r"); //replace private eith your uploaded file name
  if (!private_key) {
    Serial.println("Failed to open private cert file");
  }
  else
    Serial.println("Success to open private cert file");

  delay(1000);

  if (net.loadPrivateKey(private_key, private_key.size()))
    Serial.println("private key loaded");
  else
    Serial.println("private key not loaded");



    // Load CA file
    File ca = SPIFFS.open(caCertificate, "r"); //replace ca eith your uploaded file name
    if (!ca) {
      Serial.println("Failed to open ca ");
    }
    else
    Serial.println("Success to open ca");

    delay(1000);

    if(net.loadCACert(ca, ca.size()))
    Serial.println("ca loaded");
    else
    Serial.println("ca failed");

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

void setup() {
  Serial.begin(115200);
  configureWiFi();
  connectAWS();
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }

  client.loop();
  
//  delay(1000);
}

//char *fileLoad(Stream& stream, size_t size) {
//  char *dest = (char*)malloc(size+1);
//  if (!dest) {
//    return nullptr;
//  }
//  if (size != stream.readBytes(dest, size)) {
//    free(dest);
//    dest = nullptr;
//    return nullptr;
//  }
//  dest[size] = '\0';
//  return dest;
//}
