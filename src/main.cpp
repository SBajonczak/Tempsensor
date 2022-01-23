#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Ticker.h>

#include "ConfigurationManager.h"
#include "BatteryProcessor.h"

#include <OneWire.h>
#include <DallasTemperature.h>

const char *BELL_TOPIC_NAME = "devices/doorbell/active";
const char *BATTERY_TOPIC_NAME = "devices/doorbell/battery";
WiFiClient espClient;
PubSubClient client(espClient);
BatteryProcessor battery;

const char *ssid = "Ponyhof";
const char *password = "Tumalonga2411";
const char *mqtt_server = "mediacenter";

// GPIO where the DS18B20 is connected to
const int oneWireBus = GPIO_PIN;

int wifiStatus = WL_IDLE_STATUS; // the Wifi radio's status
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);

boolean reconnect()
{
  if (client.connect(ConfigurationManager::getInstance()->GetClientName().c_str(),
                     ConfigurationManager::getInstance()->GetMqttUser().c_str(),
                     ConfigurationManager::getInstance()->GetMqttPassword().c_str()))
  {
    Serial.println("Connected to server");
    return client.connected();
  }
  Serial.println("There is no connection!");
  return 0;
}

void sendMQTTMessage()
{

  float temperatureC = sensors.getTempCByIndex(0);
  float temperatureF = sensors.getTempFByIndex(0);

  char deviceId[13]; // need to define the static variable
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(deviceId, 13, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  if (!client.connected())
  {
    reconnect();
  }
  // Get wakeup reason
 
  DynamicJsonDocument doc(1024);
  doc["clientname"] = ConfigurationManager::getInstance()->GetClientName();
  doc["measures"]["voltage"] = temperatureC;
  doc["measures"]["celsius"] = temperatureC;
  doc["measures"]["fahrenheit"] = temperatureF;
  doc["settings"]["intervallInMinutes"] = ConfigurationManager::getInstance()->GetSleepTime();
  String json;
  serializeJson(doc, json);

  String topic = ConfigurationManager::getInstance()->GetBaseTopic() + "/" + ConfigurationManager::getInstance()->GetClientName();

  Serial.println("Sending Values");
  Serial.println(json);
  Serial.println(temperatureC);
  client.publish(topic.c_str(), json.c_str());
  client.disconnect();
  delay(1000);
}

void connectWifi()
{
  while (wifiStatus != WL_CONNECTED)
  {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network:
    wifiStatus = WiFi.begin(ssid, password);
    // wait 10 seconds for connection:
    delay(1000);
  }
}

void setup()
{
  int start = millis();

#ifdef USE_LED
  pinMode(BUILTIN_LED, OUTPUT);
  ticker.attach(0.5, tick);
#endif

  Serial.begin(115200);
  Serial.print("Reason startup :");
  Serial.println(ESP.getResetReason());

  ConfigurationManager::getInstance()->ReadSettings();
  Serial.println(ConfigurationManager::getInstance()->GetWifiSsid());
  WiFi.begin(ConfigurationManager::getInstance()->GetWifiSsid(), ConfigurationManager::getInstance()->GetWifiPassword());
  Serial.println("Connecting to WiFi.");
  int _try = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
    _try++;
    if (_try >= WIFI_CONNECT_TRY_COUNTER)
    {
      Serial.println("Impossible to connect WiFi network, go to deep sleep");
      ESP.deepSleep(0);
    }
  }
  Serial.println("Connected to the WiFi network");
  IPAddress ipAdress;
  ipAdress.fromString(ConfigurationManager::getInstance()->GetMqttServer());
  client.setServer(ipAdress, ConfigurationManager::getInstance()->GetMqttPort());

  sensors.begin();
  // Initial request
  sensors.requestTemperatures();
  sendMQTTMessage();

#ifdef USE_LED
  ticker.detach();
  digitalWrite(BUILTIN_LED, LOW);
#endif

  Serial.println("Go to deep sleep");
  int sleepTime = ConfigurationManager::getInstance()->GetSleepTime();
  Serial.print("Going to sleep for (minutes) ");
  Serial.println(sleepTime);
  ESP.deepSleep((sleepTime * 1000000) * 60); //71 minutes
}

void loop()
{
}
