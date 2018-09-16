///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Arduino.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <lwip/ip_addr.h>
#include <lwip/igmp.h>

#include "ESPixelStick.h"
#include "udpraw.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef ESPS_ENABLE_UDPRAW

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern config_t config;

#if defined(ESPS_MODE_PIXEL)
extern PixelDriver     pixels;         // Pixel object
#define _driver pixels
#elif defined(ESPS_MODE_SERIAL)
extern SerialDriver    serial;         // Serial object
#define _driver serial
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UdpRaw::begin(uint16_t port /*= ESPS_UDP_RAW_DEFAULT_PORT*/)
{
    bool isListening = false;

    if(config.multicast)
    {
        uint16_t universe = config.universe;
        IPAddress address = IPAddress(239, 255, ((universe >> 8) & 0xff), ((universe >> 0) & 0xff));

        isListening = _udp.listenMulticast(address, port);
    }
    else
    {
        isListening = _udp.listen(port);
    }

    if(isListening)
    {
        MDNS.addService("espixelstick-udpraw", "udp", port);

        _udp.onPacket(std::bind(&UdpRaw::onPacket, this, std::placeholders::_1));

        LOG_PORT.print(" - Local UDP RAW Port: ");
        LOG_PORT.println(port);
    }
}

void UdpRaw::onPacket(AsyncUDPPacket &packet) const
{
    // do not disturb effects...
    if(config.ds == DataSource::E131)
    {
        int nread = _min(packet.length(), config.channel_count);
        memcpy(_driver.getData(), packet.data(), nread);

        int nzero = config.channel_count - nread;
        if (nzero > 0)
            memset(_driver.getData() + nread, 0, nzero);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif //#ifdef ESPS_ENABLE_UDPRAW

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
