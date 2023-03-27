#include <Arduino.h>
#include "painlessMesh.h"
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DS18B20.h>
#include <Servo.h>

//  MESH Details
#define MESH_PREFIX "RNTMESH"        // name for your MESH
#define MESH_PASSWORD "MESHpassword" // password for your MESH
#define MESH_PORT 5555               // default port

// Number for this node
int nodeNumber = 1;
Servo myservo;

// String to send to other nodes with sensor readings
String readings;
OneWire oneWire1(4);
DS18B20 pin(&oneWire1);

Scheduler userScheduler; // to control your personal task
painlessMesh mesh;

// User stub
void sendMessage();   // Prototype so PlatformIO doesn't complain
String getReadings(); // Prototype for sending sensor readings

// Create tasks: to send messages and get readings;
Task taskSendMessage(TASK_SECOND * 15, TASK_FOREVER, &sendMessage);

String getReadings()
{
  readings = "";
  DynamicJsonDocument jsonReadings(64);
  // do{pin.requestTemperatures();} while (pin.getTempC()==85); //waits for the sensor to properly init
  pin.requestTemperatures();
  jsonReadings["node"] = nodeNumber;
  int val = pin.getTempC() * 100; // converting to integer,saves bandwith
  jsonReadings["temp"] = val;
  jsonReadings["pos"] = myservo.read();
  serializeJson(jsonReadings, readings);
  Serial.println(readings);
  return readings;
}

void sendMessage()
{
  String msg = getReadings();
  mesh.sendBroadcast(msg);
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
}

// Init
void initSens()
{
  pin.begin();
  pin.setResolution(12);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
}

// Needed for painless library
void receivedCallback(uint32_t from, String &msg)
{
  Serial.printf("Received from %u msg=%s\n", from, msg.c_str());
  DynamicJsonDocument myObject(256);
  deserializeJson(myObject, msg);
  byte node = myObject["node"];
  if (node == 255)
  {
    Serial.println("Received message from bridge");
    uint16_t pos = myObject["pos"];
    Serial.printf("\nMoving servo to position: %i\n", pos);
    myservo.write(pos);
    bool b = myObject["temp"];
    Serial.println(b);
    digitalWrite(LED_BUILTIN, b);
    return;
  }

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
  Serial.begin(115200);
  myservo.attach(14);
  initSens();

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
  // it will run the user scheduler as well

  mesh.update();
}