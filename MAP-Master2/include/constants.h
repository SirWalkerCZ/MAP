#ifndef constants_h
#define constants_h

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define SSID "test1"
#define pwd "123456789"

#include <Arduino.h>
uint8_t broadcastAddress[] = {0x50, 0x02, 0x91, 0xE9, 0xFD, 0x75};
typedef struct msg{
  String CFG;
  double air, temp, desired;
  long time;
  byte id;
}msg;


void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus);
void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len);

#endif
