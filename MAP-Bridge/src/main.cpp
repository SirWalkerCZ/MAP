#include <Arduino.h>
#include "painlessMesh.h"
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DS18B20.h>
#include <espnow.h>
#include <ESP8266WiFi.h>
#include <ESP32Time.h>

ESP32Time rtc;
#define MESH_PREFIX "RNTMESH"        // name for your MESH
#define MESH_PASSWORD "MESHpassword" // password for your MESH
#define MESH_PORT 5555               // default port

// Number for this node
int nodeNumber = 255; // 255 FOR BRIDGE
String readings;
OneWire oneWire1(4);
DS18B20 pin(&oneWire1);

Scheduler userScheduler; // to control your personal task
painlessMesh mesh;
bool pulse = true;
byte servoPos = 90;
// User stub
void sendMessage();   // Prototype so PlatformIO doesn't complain
void OnDataRecv(uint8_t *mac_addr, uint8_t *data, uint8_t data_len);

// Create tasks: to send messages and get readings;
Task taskSendMessage(TASK_SECOND * 5, TASK_FOREVER, &sendMessage);

uint8_t broadcastAddress[] = {0x50, 0x02, 0x91, 0xE9, 0xFD, 0x80};
typedef struct msg
{
  double air, temp, desired;
  long time;
  byte id;
} msg;
double airTemp = 0;
double desired = 10;
msg incomingReadings;
msg outgoing;

void sendMessage()
{                                                     // sends message with desired position
  if (desired < airTemp + 1 || desired > airTemp - 1) // trochu plus minus
  {
    Serial.println("měním teplotu");
    if (desired > airTemp)
    {
      Serial.println("Přitápím");
      if (servoPos != 180)
      {
        servoPos += 10;
      }      
    }
    else if (desired < airTemp)
    {
      Serial.println("Ubírám");
      if (servoPos != 0)
      {
        servoPos -= 10;
      }   
    }
    else
    {
      Serial.println("Teplota je OK");
    }
    pulse = !pulse;
    digitalWrite(LED_BUILTIN, pulse);
    DynamicJsonDocument jsonReadings(64);
    jsonReadings["node"] = nodeNumber;  
    jsonReadings["temp"] = pulse;
    jsonReadings["pos"] = servoPos;
    serializeJson(jsonReadings, readings);
    Serial.println(readings);
    mesh.sendBroadcast(readings);
    readings = "";
  }
}

void OnDataRecv(uint8_t *mac_addr, uint8_t *data, uint8_t data_len) //esp now callback
{
  memcpy(&incomingReadings, data, sizeof(incomingReadings));
  if (incomingReadings.time > 1000)
  {
    rtc.setTime(incomingReadings.time);
    Serial.println(rtc.getDateTime());
  }
  incomingReadings.desired = desired;
}

// Needed for painless library
void receivedCallback(uint32_t from, String &msg) 
{
  Serial.printf("Received from %u msg=%s\n", from, msg.c_str());
  DynamicJsonDocument myObject(256);
  deserializeJson(myObject, msg);
  outgoing.id = myObject["node"];
  outgoing.temp = myObject["temp"];
  pin.requestTemperatures();
  outgoing.air = pin.getTempC();
  outgoing.time = rtc.getEpoch();
  esp_now_send(broadcastAddress, (uint8_t *)&outgoing, sizeof(outgoing));
}
void newConnectionCallback(uint32_t nodeId)
{
  Serial.printf("New Connection, nodeId = %u\n", nodeId);
}

void changedConnectionCallback()
{
  Serial.printf("Changed connections\n");
}

void nodeTimeAdjustedCallback(int32_t offset)
{
  Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(), offset);
}

void setup()
{
  Serial.begin(74880);
  WiFi.mode(WIFI_STA);
  Serial.println(WiFi.macAddress());

  pin.begin();
  pin.setResolution(12);

  // Init ESP-NOW
  esp_now_init();
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_COMBO, 6, 0, 0);

  // Register for a callback function that will be called when data is
  esp_now_register_recv_cb(OnDataRecv);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
  mesh.setDebugMsgTypes(ERROR | STARTUP); // set before init() so that you can see startup messages

  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

  userScheduler.addTask(taskSendMessage);
  taskSendMessage.enable();
}

void loop()
{
  mesh.update();
}