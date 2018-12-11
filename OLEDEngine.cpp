#include <Arduino.h>
#include <ArduinoJson.h>
#include "OLEDEngine.h"

const String strings[] = {"string"};
const String rssis[] = {"dBm", "%", "icon"};
const DisplayElementStruct DisplayElements[] = {
    {"NetS_Mode", "de_nsmode", strings, "AP"},
    // {"NetS_SSID", "de_nsssid", {"string"}, "WifiSSID"},
    // {"NetS_Hostname", "de_nshost", {"string"}, {"esps-12a345"}},
    // {"NetS_Gateway", "de_nsgatewy", {"IP"}, {"192.168.4.1"}},
    // {"NetS_IP", "de_nsip", {"IP"}, {"192.168.4.101"}},
    // {"NetS_MAC", "de_nsmac", {"string"}, {"12:34:56:78:A9:01"}},
    {"NetS_RSSI", "de_nsrssi", rssis, "-40dBm"},
    // {"NetS_FreeHeap", "de_nsheap", {"number"}, {"24760"}},
    // {"NetS_UpTime", "de_nsuptime", {"d hh:mm:ss"}, {"13d, 22:03:42"}},
    // {"NetS_DataSource", "de_nsds", {"string"}, {"E1.31"}},
    // {"NetS_EffectName", "de_nseffect", {"string"}, {"Rainbow"}},
    // {"E131_UniverseRange", "de_e1univs", {"UFrom - UTo"}, {"1-1"}},
    // {"E131_TotalPackets", "de_e1totpkt", {"number"}, {"0"}},
    // {"E131_SequenceErrors", "de_e1seqerr", {"number"}, {"0"}},
    // {"E131_PacketErrors", "de_e1pkterr", {"number"}, {"0"}},
    // {"E131_SourceIP", "de_e1srcip", {"IP"}, {"192.168.4.100"}},
    // {"E131_LastSeen", "de_e1lastseen", {"hours", "d hh:mm:ss"}, {"3hours","13d, 22:03:42"}},
    // {"E131_#Channels", "de_e1chcount", {"number"}, {"510"}},
    // {"E131_#Pixels", "de_e1pxcount", {"number"}, {"170"}},
    {"UDPS_", "de_e1srcip", strings, "192.168.4.100"}
};