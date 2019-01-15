#include <Arduino.h>
#include <ArduinoJson.h>
#include "OLEDEngine.h"
#include <FS.h>
#include <list>

const char DISP_CONFIG_FILE[] = "/display.json";

    OLEDEngine::OLEDEngine(){

    }
    String OLEDEngine::getDisplayElements(){
        return "\"displayelements\": [ "
            "{\"name\": \"Gen_String\", \"id\": \"de_gnstr\", \"type\": 0, \"formats\": [\"string\"], \"sample\": [\"AP\"]}, "
            "{\"name\": \"Gen_SerialOutput\", \"id\": \"de_gnsrl\", \"type\": 1, \"formats\": [\"string\"], \"sample\": [\"String\"]}, "
            "{\"name\": \"NetS_Mode\", \"id\": \"de_nsmode\", \"type\": 1, \"formats\": [\"string\"], \"sample\": [\"AP\"]}, "
            "{\"name\": \"NetS_SSID\", \"id\": \"de_nsssid\", \"type\": 1, \"formats\": [\"string\"], \"sample\": [\"WifiSSID\"]}, "
            "{\"name\": \"NetS_Hostname\", \"id\": \"de_nshost\", \"type\": 1, \"formats\": [\"string\"], \"sample\": [\"esps-12a345\"]}, "
            "{\"name\": \"NetS_Gateway\", \"id\": \"de_nsgatewy\", \"type\": 1, \"formats\": [\"IP\"], \"sample\": [\"192.168.4.1\"]}, "
            "{\"name\": \"NetS_IP\", \"id\": \"de_nsip\", \"type\": 1, \"formats\": [\"IP\"], \"sample\": [\"192.168.4.101\"]}, "
            "{\"name\": \"NetS_MAC\", \"id\": \"de_nsmac\", \"type\": 1, \"formats\": [\"string\"], \"sample\": [\"12:34:56:78:A9:01\"]}, "
            "{\"name\": \"NetS_RSSI\", \"id\": \"de_nsrssi\", \"type\": 1, \"formats\": [\"rssis\",\"percent\",\"icon\"], \"sample\": [\"-40dBm\", \"60%\", \"^\"]}, "
            "{\"name\": \"NetS_FreeHeap\", \"id\": \"de_nsheap\", \"type\": 1, \"formats\": [\"number\"], \"sample\": [\"24760\"]}, "
            "{\"name\": \"NetS_UpTime\", \"id\": \"de_nsuptime\", \"type\": 1, \"formats\": [\"d hh:mm:ss\"], \"sample\": [\"13d, 22:03:42\"]}, "
            "{\"name\": \"NetS_DataSource\", \"id\": \"de_nsds\", \"type\": 1, \"formats\": [\"string\"], \"sample\": [\"E1.31\"]}, "
            "{\"name\": \"NetS_EffectName\", \"id\": \"de_nseffect\", \"type\": 1, \"formats\": [\"string\"], \"sample\": [\"Rainbow\"]}, "
            "{\"name\": \"E131_UniverseRange\", \"id\": \"de_e1univs\", \"type\": 1, \"formats\": [\"UFrom - UTo\"], \"sample\": [\"1-1\"]}, "
            "{\"name\": \"E131_TotalPackets\", \"id\": \"de_e1totpkt\", \"type\": 1, \"formats\": [\"number\"], \"sample\": [\"0\"]}, "
            "{\"name\": \"E131_SequenceErrors\", \"id\": \"de_e1seqerr\", \"type\": 1, \"formats\": [\"number\"], \"sample\": [\"0\"]}, "
            "{\"name\": \"E131_PacketErrors\", \"id\": \"de_e1pkterr\", \"type\": 1, \"formats\": [\"number\"], \"sample\": [\"0\"]}, "
            "{\"name\": \"E131_SourceIP\", \"id\": \"de_e1srcip\", \"type\": 1, \"formats\": [\"IP\"], \"sample\": [\"192.168.4.100\"]}, "
            "{\"name\": \"E131_LastSeen\", \"id\": \"de_e1lastseen\", \"type\": 1, \"formats\": [\"hours\", \"d hh:mm:ss\"], \"sample\": [\"3hours\",\"13d, 22:03:42\"]}, "
            "{\"name\": \"E131_#Channels\", \"id\": \"de_e1chcount\", \"type\": 1, \"formats\": [\"number\"], \"sample\": [\"510\"]}, "
            "{\"name\": \"E131_#Pixels\", \"id\": \"de_e1pxcount\", \"type\": 1, \"formats\": [\"number\"], \"sample\": [\"170\"]}, "
            "{\"name\": \"UDPS_Source\", \"id\": \"de_udpsrc\", \"type\": 1, \"formats\": [\"string\"], \"sample\": [\"192.168.4.100\"]} ]";
    }
    String OLEDEngine::getDisplayEvents() {

        return "\"displayevents\": ["
            "{\"name\": \"While Booting\", \"id\": \"dee_boot\"},"
            "{\"name\": \"On E1.31 data\", \"id\": \"dee_e131\"},"
            "{\"name\": \"On UDP data\", \"id\": \"dee_udp\"},"
            "{\"name\": \"On MQTT data\", \"id\": \"dee_mqtt\"},"
            "{\"name\": \"When Idle\", \"id\": \"dee_idle\"}"
            "]";
    }
    String OLEDEngine::getDisplayConfig() {
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

    void OLEDEngine::saveDisplayConfig(JsonObject &data) {
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
        
        String jsonString = getDisplayConfig();
        LOG_PORT.println("Deserialize successful");
        File file = SPIFFS.open(DISP_CONFIG_FILE, "w");
        if (!file) {
            LOG_PORT.println(F("*** Error creating configuration file ***"));
            return;
        } else {
            file.println("{" + jsonString + "}");
            LOG_PORT.println(F("* Configuration saved."));
        }        
    }

    void OLEDEngine::readDisplayConfigJson() {
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
                JsonArray &jevents = json.get<JsonArray>("displayconfig");                
                dispconfig.events = new event_t[jevents.size()];
                dispconfig.size = jevents.size();
                for(size_t i = 0; i < jevents.size(); i++)
                {
                    String tmp = jevents[i]["eventid"];
                    JsonObject &joelms = jevents[i];
                    JsonArray &jaelms = joelms.get<JsonArray>("elements");
                    dispconfig.events[i].elements = new element_t[jaelms.size()];

                    dispconfig.events[i].eventid = tmp;
                    dispconfig.events[i].size = jaelms.size();
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
    
