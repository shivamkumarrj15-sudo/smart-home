#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// ================= USER CONFIGURATION =================
// 1. Aapka WiFi Name aur Password (ESP ko sirf internet chahiye)
const char* wifi_ssid     = "GalaxyA145G";
const char* wifi_password = "12345123456";

// 2. Free Global Cloud MQTT Broker (HiveMQ)
const char* mqtt_server   = "broker.hivemq.com";
const int   mqtt_port     = 1883;

// 3. Unique Cloud Topics (GitHub Pages website se sync)
const char* TOPIC_CMD     = "shivam_home_iot_123/cmd";
const char* TOPIC_STATE   = "shivam_home_iot_123/state";
const char* TOPIC_STATUS  = "shivam_home_iot_123/status";

// 4. Relay Pins Configuration
#define PIN_BULB    D1  // 💡 Bulb -> GPIO5
#define PIN_FAN     D2  // 🌀 Fan -> GPIO4
#define PIN_FAN_ALT D4  // 🌀 Fan Alt -> GPIO2
#define PIN_TV      D5  // 📺 TV -> GPIO14

// Active LOW Relays (LOW = ON, HIGH = OFF)
#define RELAY_ON  LOW
#define RELAY_OFF HIGH
// ======================================================

WiFiClient espClient;
PubSubClient client(espClient);

bool bulbState = false;
bool fanState  = false;
bool tvState   = false;

unsigned long lastHeartbeat = 0;

// 3.3V Leakage-Proof Hardware Switching
void setDeviceState(String device, bool turnOn) {
  if (device == "bulb") {
    bulbState = turnOn;
    if (turnOn) {
      pinMode(PIN_BULB, OUTPUT);
      digitalWrite(PIN_BULB, RELAY_ON);
    } else {
      pinMode(PIN_BULB, INPUT);
    }
    Serial.printf(">>> [ACTION] BULB -> %s <<<\n", turnOn ? "ON" : "OFF");
  }
  else if (device == "fan") {
    fanState = turnOn;
    if (turnOn) {
      pinMode(PIN_FAN, OUTPUT);
      digitalWrite(PIN_FAN, RELAY_ON);
      pinMode(PIN_FAN_ALT, OUTPUT);
      digitalWrite(PIN_FAN_ALT, RELAY_ON);
    } else {
      pinMode(PIN_FAN, INPUT);
      pinMode(PIN_FAN_ALT, INPUT);
    }
    Serial.printf(">>> [ACTION] FAN -> %s <<<\n", turnOn ? "ON" : "OFF");
  }
  else if (device == "tv") {
    tvState = turnOn;
    if (turnOn) {
      pinMode(PIN_TV, OUTPUT);
      digitalWrite(PIN_TV, RELAY_ON);
    } else {
      pinMode(PIN_TV, INPUT);
    }
    Serial.printf(">>> [ACTION] TV -> %s <<<\n", turnOn ? "ON" : "OFF");
  }
}

void setAllDevices(bool turnOn) {
  setDeviceState("bulb", turnOn);
  setDeviceState("fan", turnOn);
  setDeviceState("tv", turnOn);
}

void publishState() {
  String json = "{\"bulb\":" + String(bulbState ? "true" : "false") + 
                ",\"fan\":" + String(fanState ? "true" : "false") + 
                ",\"tv\":" + String(tvState ? "true" : "false") + "}";
  client.publish(TOPIC_STATE, json.c_str(), true);
  Serial.print("Published State: ");
  Serial.println(json);
}

// MQTT Message Receiver (Called when website sends command)
void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  msg.trim();
  Serial.printf("\n[CLOUD COMMAND RECEIVED]: %s\n", msg.c_str());

  if (msg == "bulb:on")       setDeviceState("bulb", true);
  else if (msg == "bulb:off") setDeviceState("bulb", false);
  else if (msg == "fan:on")   setDeviceState("fan", true);
  else if (msg == "fan:off")  setDeviceState("fan", false);
  else if (msg == "tv:on")    setDeviceState("tv", true);
  else if (msg == "tv:off")   setDeviceState("tv", false);
  else if (msg == "all:on")   setAllDevices(true);
  else if (msg == "all:off")  setAllDevices(false);
  else if (msg == "query:status") {
    // Just respond with state
  }

  publishState();
}

void connectWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(wifi_ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 30) {
    delay(400);
    Serial.print(".");
    tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[SUCCESS] WiFi Connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[RETRY] WiFi connecting in background...");
  }
}

void reconnectMQTT() {
  if (client.connected()) return;

  Serial.print("Connecting to HiveMQ Cloud MQTT Broker...");
  String clientId = "ESP8266_Home_" + String(ESP.getChipId(), HEX);

  // Connect with Last-Will-and-Testament: if ESP loses power, website immediately marks it offline
  if (client.connect(clientId.c_str(), TOPIC_STATUS, 1, true, "offline")) {
    Serial.println(" CONNECTED!");
    // Notify cloud website that ESP is ONLINE
    client.publish(TOPIC_STATUS, "online", true);
    client.subscribe(TOPIC_CMD);
    publishState();
  } else {
    Serial.print(" FAILED, rc=");
    Serial.println(client.state());
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n========================================");
  Serial.println("  SMART HOME: GLOBAL CLOUD CONTROLLER   ");
  Serial.println("  GitHub Pages + HiveMQ Cloud MQTT       ");
  Serial.println("========================================");

  setAllDevices(false);

  connectWiFi();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    delay(1000);
    return;
  }

  if (!client.connected()) {
    reconnectMQTT();
    delay(1000);
    return;
  }

  client.loop();

  // Heartbeat every 8 seconds
  if (millis() - lastHeartbeat >= 8000) {
    lastHeartbeat = millis();
    client.publish(TOPIC_STATUS, "online", true);
  }
}
