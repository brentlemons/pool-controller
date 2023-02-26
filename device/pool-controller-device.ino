#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include <SPIFFS.h>

// Date and time functions using a DS3231 RTC connected via I2C and Wire lib
#include <RTClib.h>

RTC_DS3231 rtc;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};


#define MQTT_KEEP_ALIVE (900)
#define MQTT_TIMEOUT    (900)
#define NUMBER_OF_SPEEDS (8)

/// @brief 
struct Configuration {
  char deviceName[32];
  char awsEndpoint[128];
  int awsEndpointPort;
  char wifiSsid[32];
  char wifiPassword[32];
};

typedef struct Speed {
  char name[6];
  bool inp1;
  bool inp2;
  bool inp3;
} Speed;

// Globals
Configuration config;
WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);
Speed currentSpeed;
bool currentPower;

const int INP1 = 19;
const int INP2 = 16;
const int INP3 = 17;
const int INP4 = 21;

Speed speeds[NUMBER_OF_SPEEDS] = { // based on Hayward IS3206VSP3 Rev-C Instruction Manual
  { "ONE",   false, false, false }, // INP1 off, INP2 off, INP3 off
  { "TWO",   true,  false, false }, // INP1 on,  INP2 off, INP3 off
  { "THREE", false, true,  false }, // INP1 off, INP2 on,  INP3 off
  { "FOUR",  true,  true,  false }, // INP1 on,  INP2 on,  INP3 off
  { "FIVE",  false, false, true },  // INP1 off, INP2 off, INP3 on
  { "SIX",   true,  false, true },  // INP1 on,  INP2 off, INP3 on
  { "SEVEN", false, true,  true },  // INP1 off, INP2 on,  INP3 on
  { "EIGHT", true,  true,  true }   // INP1 on,  INP2 on,  INP3 on
};

// Function Declarations
Configuration loadConfig();
void connectWiFi(const char* ssid, const char* password);
bool connectAWS(const char* awsEndpoint, int awsEndpointPort);
void reconnect(const char* deviceName);
void publishMessage();
void callback(char* topic, byte* payload, unsigned int length);
void initInp(int inp);
bool getInpState(int inp);
void setInpState(int inp, bool state);
void setSpeed(Speed speed);
void setPower(bool power);
void flipInp(int inp);
void flipOnInp(int inp);
void flipOffInp(int inp);

// The MQTT topics that this device should publish/subscribe
#define TOPIC_ACTION_POWER_ON   "action/power/on"
#define TOPIC_ACTION_POWER_OFF  "action/power/off"
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

  // if (! rtc.begin()) {
  //   Serial.println("Couldn't find RTC");
  //   Serial.flush();
  //   while (1) delay(10);
  // }

  // if (rtc.lostPower()) {
  //   Serial.println("RTC lost power, let's set the time!");
  //   // When time needs to be set on a new device, or after a power loss, the
  //   // following line sets the RTC to the date & time this sketch was compiled
  //   rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  //   // This line sets the RTC with an explicit date & time, for example to set
  //   // January 21, 2014 at 3am you would call:
  //   // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  // }

  // Initialize the relay switches
  initInp(INP1);
  initInp(INP2);
  initInp(INP3);
  initInp(INP4);

  currentPower = false;
  currentSpeed = speeds[0];

  setSpeed(currentSpeed);
  setPower(currentPower);

  config = loadConfig();
  connectWiFi(config.wifiSsid, config.wifiPassword);
  connectAWS(config.awsEndpoint, config.awsEndpointPort);
  delay(1500);
}

// Do what you do...
void loop() {

  if (!client.connected()) {
    Serial.println("connecting...");
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
      // client.publish(AWS_IOT_PUBLISH_TOPIC, "hello world");
      // ... and resubscribe
      client.subscribe(TOPIC_ACTION_POWER_ON);
      client.subscribe(TOPIC_ACTION_POWER_OFF);
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

void publishMessage() {
  Serial.println("publishing...");
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

  StaticJsonDocument<200> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, payload);

  // Test if parsing succeeds.
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  }

  if (strcmp(topic, TOPIC_ACTION_SET_SPEED) == 0) {
    Serial.printf("Received %s message!", TOPIC_ACTION_SET_SPEED);
    if (doc["speed"]) {
      const char* speed = doc["speed"];
      Serial.printf("speed: %s\n", speed);
      for (int i = 0; i < NUMBER_OF_SPEEDS; i++) {
        if (strcmp(speed, speeds[i].name) == 0) {
          Serial.printf("FOUND SPEED %s: INP1 - %d, INP2 - %d, INP3 - %d\n", 
            speeds[i].name,
            speeds[i].inp1, 
            speeds[i].inp2, 
            speeds[i].inp3
          );
          setSpeed(speeds[i]);
          break;
        }
      }
    } else {
      Serial.println("No speed");
    }
  } else if (strcmp(topic, TOPIC_ACTION_POWER_ON) == 0) {
    Serial.printf("Received %s message!", TOPIC_ACTION_POWER_ON);
    if (doc["timestamp"]) {
      const char* speed = doc["timestamp"];
      setPower(true);
    }
  } else if (strcmp(topic, TOPIC_ACTION_POWER_OFF) == 0) {
    Serial.printf("Received %s message!", TOPIC_ACTION_POWER_OFF);
    if (doc["timestamp"]) {
      const char* speed = doc["timestamp"];
      setPower(false);
    }
  } else {
    Serial.printf("Received message on topic %s", topic);
  }

}

void initInp(int inp) {
  pinMode(inp, OUTPUT);
  digitalWrite(inp, HIGH);
}

bool getInpState(int inp) {
  digitalRead(inp);
}

void setInpState(int inp, bool state) {
  digitalWrite(inp, !state);
}

void setSpeed(Speed speed) {
  setInpState(INP1, speed.inp1);
  setInpState(INP2, speed.inp2);
  setInpState(INP3, speed.inp3);
}

void setPower(bool power) {
  setInpState(INP4, power);
}

void flipInp(int inp) {
  digitalWrite(inp, !getInpState(inp));
}

void flipOnInp(int inp) {
  digitalWrite(inp, LOW);
}

void flipOffInp(int inp) {
  digitalWrite(inp, HIGH);
}