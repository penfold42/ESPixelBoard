#include <Arduino.h>
#include "OLEDEngine.h"
#include "ESPixelStick.h"
#include <ArduinoJson.h>
#include <FS.h>
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Wire.h" // legacy include: `#include "SSD1306.h"`
#include <ESPAsyncE131.h>

#if defined(ESPS_SUPPORT_OLED) 

#if defined(ESPS_ENABLE_UDPRAW)
#include "udpraw.h"
extern UdpRaw       udpraw;
#endif

extern  config_t    config;         // Current configuration
extern EffectEngine effects;    // EffectEngine for test modes
extern ESPAsyncE131 e131;       // ESPAsyncE131 with X buffers
extern uint32_t     *seqError;  // Sequence error tracking for each universe
extern uint16_t     uniLast;    // Last Universe to listen for
extern unsigned long       mqtt_last_seen;         // millis() timestamp of last message
extern uint32_t            mqtt_num_packets;       // count of message rcvd

dispconf_t  dispconfig;
SSD1306Wire  display(0x3c, 4, 5);

void initDisplay () {
  LOG_PORT.println("Init Display called:");
    display.init();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);

    DynamicJsonBuffer jsonBuffer;
    File file = SPIFFS.open(DISP_CONFIG_FILE, "r");
    if (file) {
        // Parse CONFIG_FILE json
        size_t size = file.size();
        char buffer[size];
        file.readBytes(buffer, size);

        JsonObject &json = jsonBuffer.parseObject(buffer);
        
        if (!json.success()) {
            LOG_PORT.println(F("*** Display Configuration File Format Error ***"));
        } else {
          LOG_PORT.println("Found Display config");
            JsonArray &jevents = json.get<JsonArray>("displayconfig");                
            dispconfig.events = new event_t[jevents.size()];
            dispconfig.size = jevents.size();
            LOG_PORT.println("# of Events " + String(dispconfig.size));
            for(size_t i = 0; i < jevents.size(); i++)
            {
                String tmp = jevents[i]["eventid"];
                JsonObject &joelms = jevents[i];
                JsonArray &jaelms = joelms.get<JsonArray>("elements");
                dispconfig.events[i].elements = new element_t[jaelms.size()];

                dispconfig.events[i].eventid = tmp;
                dispconfig.events[i].size = jaelms.size();
                LOG_PORT.println(dispconfig.events[i].eventid + " has # of elements " + String(dispconfig.events[i].size));
                for(size_t j = 0; j < jaelms.size(); j++)
                {
                    JsonObject &joelm = jaelms.get<JsonObject>(j);
                    element_t elm;
                    elm.enabled = joelm.get<int16_t>("enabled");
                    elm.elementid = joelm.get<String>("elementid");
                    elm.px = joelm.get<int16_t>("px");
                    elm.py = joelm.get<int16_t>("py");
                    elm.format = joelm.get<int16_t>("format");
                    if (elm.elementid == "de_gnstr") {
                        elm.text  = joelm.get<String>("text");
                    }
                    dispconfig.events[i].elements[j] = elm;
                }
            }
        }
    } else {
        LOG_PORT.println(F("*** Display Configuration File does not exist ***"));
    }
}

void showDisplay(const String event, const String data) {
    display.clear();
    for(size_t i = 0; i < dispconfig.size; i++)
    {
        if (event == dispconfig.events[i].eventid){
          for(size_t j = 0; j < dispconfig.events[i].size; j++)
          {
              if (dispconfig.events[i].elements[j].enabled) {
                const element_t elm = dispconfig.events[i].elements[j];
                showElementValue(elm, data);
              }
          }
          display.display();
          break;
        }
    }
}
String getDisplayElements() {
    return "\"displayelements\": [ "
        "{\"name\": \"Label\", \"id\": \"de_gnstr\", \"type\": 0, \"formats\": [\"string\"], \"sample\": [\"AP\"]}, "
        "{\"name\": \"CustomString\", \"id\": \"de_gnsrl\", \"type\": 1, \"formats\": [\"string\"], \"sample\": [\"String\"]}, "
        "{\"name\": \"FW_Version\", \"id\": \"de_ver\", \"type\": 1, \"formats\": [\"string\"], \"sample\": [\"String\"]}, "
        "{\"name\": \"NetWorkMode\", \"id\": \"de_nsmode\", \"type\": 1, \"formats\": [\"string\"], \"sample\": [\"AP\"]}, "
        "{\"name\": \"SSID\", \"id\": \"de_nsssid\", \"type\": 1, \"formats\": [\"string\"], \"sample\": [\"WifiSSID\"]}, "
        "{\"name\": \"Hostname\", \"id\": \"de_nshost\", \"type\": 1, \"formats\": [\"string\"], \"sample\": [\"esps-12a345\"]}, "
        "{\"name\": \"Gateway\", \"id\": \"de_nsgatewy\", \"type\": 1, \"formats\": [\"IP\"], \"sample\": [\"192.168.4.1\"]}, "
        "{\"name\": \"IP\", \"id\": \"de_nsip\", \"type\": 1, \"formats\": [\"IP\"], \"sample\": [\"192.168.4.101\"]}, "
        "{\"name\": \"MAC\", \"id\": \"de_nsmac\", \"type\": 1, \"formats\": [\"string\"], \"sample\": [\"12:34:56:78:A9:01\"]}, "
        "{\"name\": \"RSSI\", \"id\": \"de_nsrssi\", \"type\": 1, \"formats\": [\"rssis\",\"percent\",\"icon\"], \"sample\": [\"-40dBm\", \"60%\", \"^\"]}, "
        "{\"name\": \"FreeHeap\", \"id\": \"de_nsheap\", \"type\": 1, \"formats\": [\"number\"], \"sample\": [\"24760\"]}, "
        "{\"name\": \"UpTime\", \"id\": \"de_nsuptime\", \"type\": 1, \"formats\": [\"d hh:mm:ss\"], \"sample\": [\"13d, 22:03:42\"]}, "
        "{\"name\": \"DataSource\", \"id\": \"de_nsds\", \"type\": 1, \"formats\": [\"string\"], \"sample\": [\"E1.31\"]}, "
        "{\"name\": \"EffectName\", \"id\": \"de_nseffect\", \"type\": 1, \"formats\": [\"string\"], \"sample\": [\"Rainbow\"]}, "
        "{\"name\": \"E131_UniverseRange\", \"id\": \"de_e1univs\", \"type\": 1, \"formats\": [\"UFrom - UTo\"], \"sample\": [\"1-1\"]}, "
        "{\"name\": \"E131_TotalPackets\", \"id\": \"de_e1totpkt\", \"type\": 1, \"formats\": [\"number\"], \"sample\": [\"0\"]}, "
        "{\"name\": \"E131_SequenceErrors\", \"id\": \"de_e1seqerr\", \"type\": 1, \"formats\": [\"number\"], \"sample\": [\"0\"]}, "
        "{\"name\": \"E131_PacketErrors\", \"id\": \"de_e1pkterr\", \"type\": 1, \"formats\": [\"number\"], \"sample\": [\"0\"]}, "
        "{\"name\": \"E131_SourceIP\", \"id\": \"de_e1srcip\", \"type\": 1, \"formats\": [\"IP\"], \"sample\": [\"192.168.4.100\"]}, "
        "{\"name\": \"E131_LastSeen\", \"id\": \"de_e1lastseen\", \"type\": 1, \"formats\": [\"d hh:mm:ss\"], \"sample\": [\"13d, 22:03:42\"]}, "
        "{\"name\": \"E131_#Channels\", \"id\": \"de_e1chcount\", \"type\": 1, \"formats\": [\"number\"], \"sample\": [\"510\"]}, "
        "{\"name\": \"MQTT_#Packets\", \"id\": \"de_mqtpkt\", \"type\": 1, \"formats\": [\"number\"], \"sample\": [\"512\"]}, "
        "{\"name\": \"MQTT_LastSeen\", \"id\": \"de_mqtls\", \"type\": 1, \"formats\": [\"d hh:mm:ss\"], \"sample\": [\"13d, 22:03:42\"]}, "
        "{\"name\": \"UDP_#Packets\", \"id\": \"de_udppkt\", \"type\": 1, \"formats\": [\"number\"], \"sample\": [\"512\"]}, "
        "{\"name\": \"UDP_ShortPackets\", \"id\": \"de_udpspkt\", \"type\": 1, \"formats\": [\"number\"], \"sample\": [\"512\"]}, "
        "{\"name\": \"UDP_LongPackets\", \"id\": \"de_udplpkts\", \"type\": 1, \"formats\": [\"number\"], \"sample\": [\"512\"]}, "
        "{\"name\": \"UDP_LastSeen\", \"id\": \"de_udpls\", \"type\": 1, \"formats\": [\"d hh:mm:ss\"], \"sample\": [\"13d, 22:03:42\"]}, "
        "{\"name\": \"UDP_ClientIP\", \"id\": \"de_udpsrc\", \"type\": 1, \"formats\": [\"IP\"], \"sample\": [\"192.168.4.100\"]} ]";

}
String getDisplayConfig() {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    JsonArray& events = jsonBuffer.createArray();
    
    for(size_t i = 0; i < dispconfig.size; i++)
    {
        JsonObject& event = events.createNestedObject();
        event["eventid"] = dispconfig.events[i].eventid;   
        JsonArray& elements = event.createNestedArray("elements");
        for(size_t j = 0; j < dispconfig.events[i].size; j++)
        {
            JsonObject& element = elements.createNestedObject(); 
            element["enabled"] = dispconfig.events[i].elements[j].enabled;
            element["elementid"] = dispconfig.events[i].elements[j].elementid;
            element["px"] = dispconfig.events[i].elements[j].px;
            element["py"] = dispconfig.events[i].elements[j].py;
            element["format"] = dispconfig.events[i].elements[j].format;
            if (element["elementid"] == "de_gnstr"){
                element["text"] = dispconfig.events[i].elements[j].text;
            }
        }
        event["elements"] = elements;
    }
    root["displayconfig"] = events;
    String response;
    root.printTo(response);
    response = response.substring(1, response.length() - 1);
    
    return response;
}
String getDisplayEvents() {
    return "\"displayevents\": ["
        "{\"name\": \"While Booting\", \"id\": \"dee_boot\"},"
        "{\"name\": \"On E1.31 data\", \"id\": \"dee_e131\"},"
        "{\"name\": \"On MQTT data\", \"id\": \"dee_mqtt\"},"
        "{\"name\": \"When Idle\", \"id\": \"dee_idle\"}"
        "]";
}
void saveDisplayConfig(JsonObject &data) {
//Update config var
    for(size_t i = 0; i < dispconfig.size; i++)
    {
        if (data["eventid"] == dispconfig.events[i].eventid) {
            JsonArray &jaes = data.get<JsonArray>("elements");
            dispconfig.events[i].elements = new element_t[jaes.size()];
            dispconfig.events[i].size = jaes.size();
            for(size_t j = 0; j < jaes.size(); j++)
            {
                JsonObject &joelm = jaes.get<JsonObject>(j);
                element_t elm;
                elm.enabled = joelm.get<int16_t>("enabled");
                elm.elementid = joelm.get<String>("elementid");
                elm.px = joelm.get<int16_t>("px");
                elm.py = joelm.get<int16_t>("py");
                elm.format = joelm.get<int16_t>("format");
                if (elm.elementid == "de_gnstr") {
                    elm.text  = joelm.get<String>("text");
                }
                dispconfig.events[i].elements[j] = elm;
            }
            break;
        }
    }
    // Parse config var
    String jsonString = getDisplayConfig();
    LOG_PORT.println("Deserialize successful");
    // Save data to file.
    File file = SPIFFS.open(DISP_CONFIG_FILE, "w");
    if (!file) {
        LOG_PORT.println(F("*** Error creating configuration file ***"));
        return;
    } else {
        file.println("{" + jsonString + "}");
        LOG_PORT.println(F("* Configuration saved."));
    }  
}
void showElementValue(const element_t elem, const String custData) {
    String disptext = "";
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    if(elem.elementid == "de_gnsrl"){
        display.setFont(ArialMT_Plain_10);
        display.drawStringMaxWidth(elem.px, elem.py, 128, custData);
    } else if (elem.elementid == "de_gnstr"){
        disptext = elem.text;
    } else if (elem.elementid == "de_ver"){
        disptext = (String)VERSION;
    } else if (elem.elementid == "de_nsssid"){
        String cssid = config.ssid;
        if (WiFi.localIP()[0] == 0) {
            cssid = WiFi.softAPSSID();
        }
        display.setFont(ArialMT_Plain_10);
        display.drawString(elem.px, elem.py, cssid);
    } else if (elem.elementid == "de_nshost"){
        disptext = (String)WiFi.hostname();
    } else if (elem.elementid == "de_nsgatewy"){
        disptext = IPAddress(config.gateway[0], config.gateway[1], config.gateway[2], config.gateway[3]).toString();
    } else if (elem.elementid == "de_nsip"){
        if  (WiFi.localIP()[0] == 0) {
            disptext = WiFi.softAPIP().toString();
        } else {
            disptext = WiFi.localIP().toString();
        }
    } else if (elem.elementid == "de_nsmac"){
        disptext = WiFi.macAddress();
    } else if (elem.elementid == "de_nsrssi"){
        display.setTextAlignment(TEXT_ALIGN_RIGHT);
        int32_t rssi = WiFi.RSSI();
        if (elem.format == 0){ //format rssis
            disptext = ((String)rssi) + "dbm";
        } else if (elem.format == 1 ) { // format percent
            if (rssi <= -100){
                disptext = "0%";
            } else if (rssi >= -50){
                disptext = "100%";
            } else {
                int32_t quality = 2 * (rssi + 100);
                disptext = ((String)quality) + "%";
            }
        } else if (elem.format == 2 ){ //format icon
            int32_t quality = 2 * (rssi + 100);
            if (quality > 80) {
                //Wifi_1 bitmap 
                display.drawXbm(elem.px, elem.py, 16, 16, wifi1);
            } else if (quality > 60) {
                //Wifi_2 bitmap
                display.drawXbm(elem.px, elem.py, 16, 16, wifi2);
            } else if (quality > 40) {
                //Wifi_3 bitmap
                display.drawXbm(elem.px, elem.py, 16, 16, wifi3);
            } else if (quality > 20) {
                //Wifi_4 itmap
                display.drawXbm(elem.px, elem.py, 16, 16, wifi4);
            } else {
                //Do not render anything
            }
        }
    } else if (elem.elementid == "de_nsheap"){
        disptext = (String)ESP.getFreeHeap();
    } else if (elem.elementid == "de_nsuptime"){
        disptext = (String)millis();
    } else if (elem.elementid == "de_nsds"){
        switch (config.ds) {
            case DataSource::E131:
                disptext = "E131";
                break;
            case DataSource::MQTT:
                disptext = "MQTT";
                break;
            case DataSource::WEB:
                disptext = "web";
                break;
            case DataSource::IDLEWEB:
                disptext = "Idle";
                break;
            default:
                disptext = "None";
                break;
        }
    } else if (elem.elementid == "de_nseffect"){
        disptext = (String)effects.getEffect();
    } else if (elem.elementid == "de_e1univs"){
        disptext = (String)config.universe + " - " + (String)uniLast;
    } else if (elem.elementid == "de_e1totpkt"){
        disptext = (String)e131.stats.num_packets;
    } else if (elem.elementid == "de_e1seqerr"){
        int32_t seqErrors = 0;
        for (int i = 0; i < ((uniLast + 1) - config.universe); i++)
            seqErrors =+ seqError[i];
        disptext = (String)seqErrors;
    } else if (elem.elementid == "de_e1pkterr"){
        disptext = (String)e131.stats.packet_errors;
    } else if (elem.elementid == "de_e1srcip"){
        disptext = e131.stats.last_clientIP.toString();
    } else if (elem.elementid == "de_e1lastseen"){
        disptext = e131.stats.last_seen ? (String) (millis() - e131.stats.last_seen) : "never";
    } else if (elem.elementid == "de_e1chcount"){
        disptext = config.channel_count;
    } else if (elem.elementid == "de_mqtpkt"){
        disptext = (String)mqtt_num_packets;
    } else if (elem.elementid == "de_mqtls"){
        disptext = mqtt_last_seen ? (String) (millis() - mqtt_last_seen) : "never";
    } else if (elem.elementid == "de_udppkt"){
        disptext = (String)udpraw.stats.num_packets;
    } else if (elem.elementid == "de_udpspkt"){
        disptext = (String)udpraw.stats.short_packets;
    } else if (elem.elementid == "de_udplpkts"){
        disptext = (String)udpraw.stats.long_packets;
    } else if (elem.elementid == "de_udpls"){
        disptext = udpraw.stats.last_seen ? (String) (millis() - udpraw.stats.last_seen) : "never";
    } else if (elem.elementid == "de_udpsrc"){
        disptext = udpraw.stats.last_clientIP.toString();
    }
    if (disptext != ""){
        display.setFont(ArialMT_Plain_16);
        display.drawString(elem.px, elem.py, disptext);
    }
}
#endif // ESPS_SUPPORT_OLED