#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiAP.h>
#include <ESPAsyncWebServer.h>
#include <espnow.h>
#include <LittleFS.h>
#include "constants.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
int potRead;
double desiredTemp;
double airTemp;
byte webTemp = 0;
byte pot = 0;
uint32_t tojm =0;
msg myData;
uint32_t TIMER, TIMER_ESP_NOW;
AsyncWebServer server(80);
msg recData;
String temp;
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("Last Packet Send Status: ");
  if (sendStatus == 0){
    Serial.println("Delivery success");
  }
  else{
    Serial.println("Delivery fail");
  }
}

void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {
  memcpy(&recData, incomingData, sizeof(recData));
  airTemp = recData.air;
  tojm = recData.time;
  Serial.printf("Vzduch:%f, Teplota:%f, Čas: %i, ID:%i\n", recData.air, recData.temp, recData.time, recData.id);
  File file = LittleFS.open("/buffer.csv", "w");
  file.print(String(recData.air) + "," + String(recData.temp) + "," + String(recData.time) + "," + String(recData.id) + "\n");
  file.close();
}

void setup() {
  Serial.begin(74880);
  Serial.print("ESP8266 Board MAC Address:  ");
  Serial.println(WiFi.macAddress());

  if(!LittleFS.begin()){
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }

  WiFi.begin();
  WiFi.softAP(SSID, pwd);
  WiFi.softAPConfig(IPAddress(192, 168, 5, 1), IPAddress(192, 168, 5, 1), IPAddress(255, 255, 255, 0));

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/index.html", "text/html"); });
  server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/buffer.csv", "text/html", true);
            Serial.println("uživatel stahuje"); });
  server.on("/clear", HTTP_GET, [](AsyncWebServerRequest *request)
            { 
              LittleFS.remove("/buffer.csv");
              File file = LittleFS.open("/buffer.csv", "w");
              file.print("Vzduch,Teplota,Čas,ID\n");
              file.close();
              request->send(200);
            });
  server.on("/post", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
      {
        temp = (char *)data; // převádění udajů do použitelné podoby, přes magické přetypovací řešení
        temp = temp.substring(0, 10);
        Serial.println(temp.toInt());
        myData.CFG = temp;
        esp_now_send(broadcastAddress,(uint8_t *) &myData, sizeof(myData));
        myData.CFG = "";
      });
  server.on("/macAddress", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", WiFi.softAPmacAddress().c_str()); });
  server.on("/runtime", HTTP_GET, [](AsyncWebServerRequest *request)
            { temp=millis()/1000; request->send_P(200, "text/plain", temp.c_str()); });
  server.on("/pot", HTTP_GET, [](AsyncWebServerRequest *request)
            { temp=millis()/1000; request->send_P(200, "text/plain", String(pot).c_str()); });
  server.on("/air", HTTP_GET, [](AsyncWebServerRequest *request)
          { temp=millis()/1000; request->send_P(200, "text/plain", String(airTemp).c_str()); });
  server.on("/date", HTTP_GET, [](AsyncWebServerRequest *request)
          { temp=millis()/1000; request->send_P(200, "text/plain", String(tojm).c_str()); });

  server.begin();

  esp_now_init();
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_add_peer(broadcastAddress,ESP_NOW_ROLE_COMBO,6,0,0);
  esp_now_register_recv_cb(OnDataRecv);
   
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    ESP.restart();
  }
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.println("Booting...");
  display.display();
  delay(2000);
}
void loop() {
  if (TIMER<millis())
  {
    display.clearDisplay();
    display.setCursor(0, 10);
    potRead = analogRead(A0);
    desiredTemp = map(potRead,0,1024,1000,3000);
    pot = map(potRead,0,1024,0,180);
    webTemp = map(airTemp,0,1024,0,180);
    desiredTemp = desiredTemp/100;
    String macmacmac =WiFi.macAddress();
    display.printf("SSID: %s\nPwd: %s\nIP:%s\nTeplota: %.2f C\nCilova teplota: %.2f", SSID, pwd, macmacmac, airTemp, desiredTemp);
    display.display();
    TIMER= millis()+100UL;
  }
  if (TIMER_ESP_NOW<millis())
  {
    myData.desired = desiredTemp;
    myData.CFG = "";
    esp_now_send(broadcastAddress,(uint8_t *) &myData, sizeof(myData));
    TIMER_ESP_NOW = millis()+10000UL;
  }
}