#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Ticker.h>

#include "ConfigurationManager.h"
#include "BatteryProcessor.h"
#include "WifiManager.h"

#include <OneWire.h>
#include <DallasTemperature.h>
const char *BELL_TOPIC_NAME = "devices/doorbell/active";
const char *BATTERY_TOPIC_NAME = "devices/doorbell/battery";
WiFiClient espClient;
PubSubClient client(espClient);
BatteryProcessor battery;

// GPIO where the DS18B20 is connected to
const int oneWireBus = GPIO_PIN;

int wifiStatus = WL_IDLE_STATUS; // the Wifi radio's status
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);
WifiManager wifimanager;

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

void sendMQTTMessage(unsigned long timeBegin)
{
  float rawVoltage = battery.getVolt();
  float temperatureC = sensors.getTempCByIndex(0);
  float temperatureF = sensors.getTempFByIndex(0);

  if (!client.connected())
  {
    reconnect();
  }
  // Get wakeup reason

  unsigned long timeEnd = micros();
  unsigned long duration = timeEnd - timeBegin;
  double averageDuration = (double)duration / 1000.0;

  DynamicJsonDocument doc(1024);
  doc["clientname"] = ConfigurationManager::getInstance()->GetClientName();

  doc["measures"]["celsius"] = temperatureC;
  doc["measures"]["fahrenheit"] = temperatureF;
  doc["measures"]["duration"]= averageDuration;
  doc["voltage"]["rawVoltage"] = rawVoltage;
  doc["voltage"]["adjustedVoltage"] = rawVoltage * ConfigurationManager::getInstance()->GetVoltageMultiplicator();
  doc["settings"]["intervallInMinutes"] = ConfigurationManager::getInstance()->GetSleepTime();
  String json;
  serializeJson(doc, json);

  String topic = ConfigurationManager::getInstance()->GetBaseTopic() + "/" + ConfigurationManager::getInstance()->GetClientName();

  Serial.println("Sending Values");
  Serial.println(json);
  Serial.println(temperatureC);
  client.publish(topic.c_str(), json.c_str());
  client.disconnect();
}


void setup()
{
   unsigned long timeBegin = micros();
#ifdef USE_LED
  pinMode(BUILTIN_LED, OUTPUT);
  ticker.attach(0.5, tick);
#endif

  Serial.begin(115200);
  Serial.print("Reason startup :");
  Serial.println(ESP.getResetReason());

  ConfigurationManager::getInstance()->ReadSettings();
  
  wifimanager.Initialize();
  wifimanager.Connect();

  IPAddress ipAdress;
  ipAdress.fromString(ConfigurationManager::getInstance()->GetMqttServer());
  client.setServer(ipAdress, ConfigurationManager::getInstance()->GetMqttPort());

  sensors.begin();
  // Initial request
  sensors.requestTemperatures();
  sendMQTTMessage(timeBegin);

#ifdef USE_LED
  ticker.detach();
  digitalWrite(BUILTIN_LED, LOW);
#endif

  Serial.println("Go to deep sleep");
  int sleepTime = ConfigurationManager::getInstance()->GetSleepTime();
  Serial.print("Going to sleep for (minutes) ");
  Serial.println(sleepTime);
  // Disable WIFI on Wakeup
  ESP.deepSleep(sleepTime * (1000000 * 60)); 
}

void loop()
{
}
