#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include "ESPixelStick.h"

#if defined(ESPS_MODE_PIXEL)
extern PixelDriver     pixels;         /* Pixel object */
#elif defined(ESPS_MODE_SERIAL)
extern  SerialDriver    serial;         /* Serial object */
#else
#error "No valid output mode defined."
#endif

extern  config_t        config;

WiFiUDP             RAWudp;             /* Raw UDP packet Server */
unsigned int        RAWPort = 2801;      // local port to listen for UDP packets
unsigned long       RAW_ctr = 0;
#define             UDPRAW_BUFFER_SIZE  1600
uint8_t             udpraw_Buffer[ UDPRAW_BUFFER_SIZE];

void handle_raw_port() {

  if (!RAWudp) {
    Serial.println("RE-Starting UDP");
    RAWudp.begin(RAWPort);
    MDNS.addService("hyperiond-rgbled", "udp", RAWPort);
    Serial.print("Local RAWport: ");
    Serial.println(RAWudp.localPort());
  }
  if (!RAWudp) {
    Serial.println("RE-Start failed.");
    return;
  }

  int size = RAWudp.parsePacket();
  if (size) {
    // We've received a packet, read the data from it
    RAWudp.read(udpraw_Buffer, UDPRAW_BUFFER_SIZE); // read the packet into the buffer
    RAW_ctr++;

    /* Set the data */
    int i=0;
    for (i = 0; i < _min(size,config.channel_count); i++) {
#if defined(ESPS_MODE_PIXEL)
        pixels.setValue(i, udpraw_Buffer[i]);
#elif defined(ESPS_MODE_SERIAL)
        serial.setValue(i, udpraw_Buffer[i]);
#endif
    }
    /* fill the rest with 0s*/
    for (      ; i < config.channel_count; i++) {
#if defined(ESPS_MODE_PIXEL)
        pixels.setValue(i, 0);
#elif defined(ESPS_MODE_SERIAL)
        serial.setValue(i, 0);
#endif
    }
  }
}


