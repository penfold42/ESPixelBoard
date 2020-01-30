/*
* ESPixelStick.ino
*
* Project: ESPixelStick - An ESP8266 and E1.31 based pixel driver
* Copyright (c) 2016 Shelby Merrick
* http://www.forkineye.com
*
*  This program is provided free for you to use in any way that you wish,
*  subject to the laws and regulations where you are using it.  Due diligence
*  is strongly suggested before using this code.  Please give credit where due.
*
*  The Author makes no warranty of any kind, express or implied, with regard
*  to this program or the documentation contained in this document.  The
*  Author shall not be liable in any event for incidental or consequential
*  damages in connection with, or arising out of, the furnishing, performance
*  or use of these programs.
*
*/

/*****************************************/
/*        BEGIN - Configuration          */
/*****************************************/

/* Fallback configuration if config.json is empty or fails */
const char ssid[] = "ENTER_SSID_HERE";
const char passphrase[] = "ENTER_PASSPHRASE_HERE";

/*****************************************/
/*         END - Configuration           */
/*****************************************/

#include <ESPAsyncE131.h>
#include <Hash.h>
#include <SPI.h>
#include "ESPixelStick.h"
#include "EFUpdate.h"
#include "wshandler.h"
#include "gamma.h"
#include "udpraw.h"
#include "pwm.h"
#include "gpio.h"
#include "buttons.h"

extern "C" {
#include <user_interface.h>
}

// Debugging support
#if defined(DEBUG)
extern "C" void system_set_os_print(uint8 onoff);
extern "C" void ets_install_putc1(void* routine);

static void _u0_putc(char c){
  while(((U0S >> USTXC) & 0x7F) == 0x7F);
  U0F = c;
}
#endif

/////////////////////////////////////////////////////////
//
//  Globals
//
/////////////////////////////////////////////////////////

// old style MQTT State
const char MQTT_LIGHT_STATE_TOPIC[] = "/light/status";
const char MQTT_LIGHT_COMMAND_TOPIC[] = "/light/switch";

 // MQTT Brightness
const char MQTT_LIGHT_BRIGHTNESS_STATE_TOPIC[] = "/brightness/status";
const char MQTT_LIGHT_BRIGHTNESS_COMMAND_TOPIC[] = "/brightness/set";

 // MQTT Colors (rgb)
const char MQTT_LIGHT_RGB_STATE_TOPIC[] = "/rgb/status";
const char MQTT_LIGHT_RGB_COMMAND_TOPIC[] = "/rgb/set";

// MQTT State
const char MQTT_SET_COMMAND_TOPIC[] = "/set";

// MQTT Payloads by default (on/off)
const char LIGHT_ON[] = "ON";
const char LIGHT_OFF[] = "OFF";

// MQTT buffer used to send / receive data
#define MSG_BUFFER_SIZE 20
char m_msg_buffer[MSG_BUFFER_SIZE];

// Configuration file
const char CONFIG_FILE[] = "/config.json";

ESPAsyncE131        e131(10);       // ESPAsyncE131 with X buffers
config_t            config;         // Current configuration
uint32_t            *seqError;      // Sequence error tracking for each universe
uint16_t            uniLast = 1;    // Last Universe to listen for
bool                reboot = false; // Reboot flag
AsyncWebServer      web(HTTP_PORT); // Web Server
AsyncWebSocket      ws("/ws");      // Web Socket Plugin
uint8_t             *seqTracker;    // Current sequence numbers for each Universe */
uint32_t            lastUpdate;     // Update timeout tracker
WiFiEventHandler    wifiConnectHandler;     // WiFi connect handler
WiFiEventHandler    wifiDisconnectHandler;  // WiFi disconnect handler
Ticker              wifiTicker;     // Ticker to handle WiFi
Ticker              idleTicker;     // Ticker for effect on idle
AsyncMqttClient     mqtt;           // MQTT object
Ticker              mqttTicker;     // Ticker to handle MQTT
unsigned long       mqtt_last_seen;         // millis() timestamp of last message
uint32_t            mqtt_num_packets;       // count of message rcvd
EffectEngine        effects;        // Effects Engine
Ticker              sendTimer;
UdpRaw              udpraw;

// Output Drivers
#if defined(ESPS_MODE_PIXEL)
PixelDriver     pixels;         // Pixel object
#elif defined(ESPS_MODE_SERIAL)
SerialDriver    serial;         // Serial object
#else
#error "No valid output mode defined."
#endif

/////////////////////////////////////////////////////////
//
//  Forward Declarations
//
/////////////////////////////////////////////////////////

void loadConfig();
void initWifi();
void initWeb();
void updateConfig();
void publishState();
void resolveHosts();
void sendData();

// Radio config
RF_PRE_INIT() {
    //wifi_set_phy_mode(PHY_MODE_11G);    // Force 802.11g mode
    system_phy_set_powerup_option(31);  // Do full RF calibration on power-up
    system_phy_set_max_tpw(82);         // Set max TX power
}

void setup() {
    // Configure SDK params
    wifi_set_sleep_type(NONE_SLEEP_T);
    setupWebGpio();
    // Initial pin states
    pinMode(DATA_PIN, OUTPUT);
    digitalWrite(DATA_PIN, LOW);

    // Setup serial log port
    LOG_PORT.begin(115200);
    delay(10);

#if defined(DEBUG)
    ets_install_putc1((void *) &_u0_putc);
    system_set_os_print(1);
#endif

    // Set default data source to E131
    config.ds = DataSource::E131;

    LOG_PORT.println("");
    LOG_PORT.print(F("ESPixelBoard v"));
    for (uint8_t i = 0; i < strlen_P(VERSION); i++)
        LOG_PORT.print((char)(pgm_read_byte(VERSION + i)));
    LOG_PORT.print(F(" ("));
    for (uint8_t i = 0; i < strlen_P(BUILD_DATE); i++)
        LOG_PORT.print((char)(pgm_read_byte(BUILD_DATE + i)));
    LOG_PORT.println(")");
    LOG_PORT.println(ESP.getFullVersion());

    // Enable SPIFFS
    if (!SPIFFS.begin())
    {
        LOG_PORT.println(F("File system did not initialise correctly"));
    }
    else
    {
        LOG_PORT.println(F("File system initialised"));
    }

    FSInfo fs_info;
    if (SPIFFS.info(fs_info))
    {
        LOG_PORT.print("Total bytes in file system: ");
        LOG_PORT.println(fs_info.usedBytes);

        Dir dir = SPIFFS.openDir("/");
        while (dir.next()) {
          LOG_PORT.print(dir.fileName());
          File f = dir.openFile("r");
          LOG_PORT.println(f.size());
        }
    }
    else
    {
        LOG_PORT.println(F("Failed to read file system details"));
    }
    
    // Load configuration from SPIFFS and set Hostname
    loadConfig();
    if (config.hostname)
        WiFi.hostname(config.hostname);

#if defined (ESPS_MODE_PIXEL)
    pixels.setPin(DATA_PIN);
    updateConfig();

    // Do one effects cycle as early as possible
    if (config.ds == DataSource::WEB) {
        effects.run();
    }
    // set the effect idle timer
    idleTicker.attach(config.effect_idletimeout, idleTimeout);
    sendTimer.attach(1.0/config.effect_sendspeed, sendData);
    pixels.show();
#else
    updateConfig();
    // Do one effects cycle as early as possible
    if (config.ds == DataSource::WEB) {
        effects.run();
    }
    // set the effect idle timer
    idleTicker.attach(config.effect_idletimeout, idleTimeout);

    serial.show();
#endif

    // Configure the pwm outputs
#if defined (ESPS_SUPPORT_PWM)
    setupPWM();
    handlePWM(); // Do one update cycle as early as possible
#endif

    // Setup WiFi Handlers
    wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);

    // Setup MQTT Handlers
    if (config.mqtt) {
        mqtt.onConnect(onMqttConnect);
        mqtt.onDisconnect(onMqttDisconnect);
        mqtt.onMessage(onMqttMessage);
        mqtt.setServer(config.mqtt_ip.c_str(), config.mqtt_port);
        // Unset clean session (defaults to true) so we get retained messages of QoS > 0
        mqtt.setCleanSession(config.mqtt_clean);
        if (config.mqtt_user.length() > 0)
            mqtt.setCredentials(config.mqtt_user.c_str(), config.mqtt_password.c_str());
    }

    // Fallback to default SSID and passphrase if we fail to connect
    initWifi();
    if (WiFi.status() != WL_CONNECTED) {
        LOG_PORT.println(F("*** Timeout - Reverting to default SSID ***"));
        config.ssid = ssid;
        config.passphrase = passphrase;
        initWifi();
    }

    // If we fail again, go SoftAP or reboot
    if (WiFi.status() != WL_CONNECTED) {
        if (config.ap_fallback) {
            LOG_PORT.println(F("*** FAILED TO ASSOCIATE WITH AP, GOING SOFTAP ***"));
            WiFi.mode(WIFI_AP);
            String ssid = "ESPixelBoard " + String(config.hostname);
            WiFi.softAP(ssid.c_str());
        } else {
            LOG_PORT.println(F("*** FAILED TO ASSOCIATE WITH AP, REBOOTING ***"));
            ESP.restart();
        }
    }

    resolveHosts();

    wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWiFiDisconnect);

    // Configure and start the web server
    initWeb();

    // Setup E1.31
    if (config.multicast) {
        if (e131.begin(E131_MULTICAST, config.universe,
                uniLast - config.universe + 1)) {
            LOG_PORT.println(F("- Multicast Enabled"));
        }  else {
            LOG_PORT.println(F("*** MULTICAST INIT FAILED ****"));
        }
    } else {
        if (e131.begin(E131_UNICAST)) {
            LOG_PORT.print(F("- Unicast port: "));
            LOG_PORT.println(E131_DEFAULT_PORT);
        } else {
            LOG_PORT.println(F("*** UNICAST INIT FAILED ****"));
        }
    }

   /* check for raw packets on port 2801 */
#if defined(ESPS_ENABLE_UDPRAW)
    if (config.udp_enabled) {
        udpraw.begin(config.udp_port ? config.udp_port : ESPS_UDP_RAW_DEFAULT_PORT );
    }
#endif

#if defined(ESPS_ENABLE_BUTTONS)
    setupButtons();
#endif

}

/////////////////////////////////////////////////////////
//
//  WiFi Section
//
/////////////////////////////////////////////////////////

void initWifi() {
    // Switch to station mode and disconnect just in case
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    connectWifi();
    uint32_t timeout = millis();
    while (WiFi.status() != WL_CONNECTED) {
        LOG_PORT.print(".");
        delay(500);
        if (millis() - timeout > (1000 * config.sta_timeout) ){
            LOG_PORT.println("");
            LOG_PORT.println(F("*** Failed to connect ***"));
            break;
        }
    }
}

void connectWifi() {
    delay(secureRandom(100, 500));

    LOG_PORT.println("");
    LOG_PORT.print(F("Connecting to "));
    LOG_PORT.print(config.ssid);
    LOG_PORT.print(F(" as "));
    LOG_PORT.println(config.hostname);

    WiFi.begin(config.ssid.c_str(), config.passphrase.c_str());
    if (config.dhcp) {
        LOG_PORT.print(F("Connecting with DHCP"));
    } else {
        // We don't use DNS, so just set it to our gateway
        WiFi.config(IPAddress(config.ip[0], config.ip[1], config.ip[2], config.ip[3]),
                    IPAddress(config.gateway[0], config.gateway[1], config.gateway[2], config.gateway[3]),
                    IPAddress(config.netmask[0], config.netmask[1], config.netmask[2], config.netmask[3]),
                    IPAddress(config.gateway[0], config.gateway[1], config.gateway[2], config.gateway[3])
        );
        LOG_PORT.print(F("Connecting with Static IP"));
    }
}

void onWifiConnect(__attribute__ ((unused)) const WiFiEventStationModeGotIP &event) {
    LOG_PORT.println("");
    LOG_PORT.print(F("Connected with IP: "));
    LOG_PORT.println(WiFi.localIP());

    // Setup MQTT connection if enabled
    if (config.mqtt)
        connectToMqtt();

    // Setup mDNS / DNS-SD
    //TODO: Reboot or restart mdns when config.id is changed?
     char chipId[7] = { 0 };
    snprintf(chipId, sizeof(chipId), "%06x", ESP.getChipId());

//  MDNS setInstanceName changes in 2.5.0-dev:
//  - setInstanceName also changes hostname
//  - setInstanceName only takes c_str
//  - MDNS.update() is required
//  - use short name of hostname for now

//    MDNS.setInstanceName(config.id + " (" + String(chipId) + ")");

    char mdnsName[64] = {0};
    strncpy (mdnsName, config.hostname.c_str(), 63);
    char* firstDot = strchr (mdnsName, '.');
    if (firstDot) {
        *firstDot = '\0';
    }

    if (MDNS.begin(mdnsName)) {
        MDNS.addService("http", "tcp", HTTP_PORT);
        MDNS.addService("e131", "udp", E131_DEFAULT_PORT);
        MDNS.addServiceTxt("e131", "udp", "TxtVers", String(RDMNET_DNSSD_TXTVERS));
        MDNS.addServiceTxt("e131", "udp", "ConfScope", RDMNET_DEFAULT_SCOPE);
        MDNS.addServiceTxt("e131", "udp", "E133Vers", String(RDMNET_DNSSD_E133VERS));
        MDNS.addServiceTxt("e131", "udp", "CID", String(chipId));
        MDNS.addServiceTxt("e131", "udp", "Model", "ESPixelBoard");
        MDNS.addServiceTxt("e131", "udp", "Manuf", "Forkineye");
    } else {
        LOG_PORT.println(F("*** Error setting up mDNS responder ***"));
    }
}

void onWiFiDisconnect(__attribute__ ((unused)) const WiFiEventStationModeDisconnected &event) {
    LOG_PORT.println(F("*** WiFi Disconnected ***"));

    // Pause MQTT reconnect while WiFi is reconnecting
    mqttTicker.detach();
    wifiTicker.once(2, connectWifi);
}

// Subscribe to "n" universes, starting at "universe"
void multiSub() {
    uint8_t count;
    ip_addr_t ifaddr;
    ip_addr_t multicast_addr;

    count = uniLast - config.universe + 1;
    ifaddr.addr = static_cast<uint32_t>(WiFi.localIP());
    for (uint8_t i = 0; i < count; i++) {
        multicast_addr.addr = static_cast<uint32_t>(IPAddress(239, 255,
                (((config.universe + i) >> 8) & 0xff),
                (((config.universe + i) >> 0) & 0xff)));
        igmp_joingroup(&ifaddr, &multicast_addr);
    }
}

/////////////////////////////////////////////////////////
//
//  MQTT Section
//
/////////////////////////////////////////////////////////

void connectToMqtt() {
    LOG_PORT.print(F("- Connecting to MQTT Broker "));
    LOG_PORT.println(config.mqtt_ip);
    mqtt.connect();
}

void onMqttConnect(__attribute__ ((unused)) bool sessionPresent) {
    LOG_PORT.println(F("- MQTT Connected"));

    // Get retained MQTT state
    mqtt.subscribe(config.mqtt_topic.c_str(), 0);
    mqtt.unsubscribe(config.mqtt_topic.c_str());

    // old style Setup subscriptions
    mqtt.subscribe(String(config.mqtt_topic + MQTT_LIGHT_COMMAND_TOPIC).c_str(), 0);
    mqtt.subscribe(String(config.mqtt_topic + MQTT_LIGHT_BRIGHTNESS_COMMAND_TOPIC).c_str(), 0);
    mqtt.subscribe(String(config.mqtt_topic + MQTT_LIGHT_RGB_COMMAND_TOPIC).c_str(), 0);

    // Setup subscriptions
    mqtt.subscribe(String(config.mqtt_topic + MQTT_SET_COMMAND_TOPIC).c_str(), 0);

    // old style Publish state
    publishRGBState();
    publishRGBBrightness();
    publishRGBColor();

    // Publish state
    publishState();
}

void onMqttDisconnect(__attribute__ ((unused)) AsyncMqttClientDisconnectReason reason) {
    LOG_PORT.println(F("- MQTT Disconnected"));
    if (WiFi.isConnected()) {
        mqttTicker.once(2, connectToMqtt);
    }
}

void onMqttMessage(char* topic, char* payload,
        AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {

    mqtt_last_seen = millis();
    mqtt_num_packets++;


// check first char for {, if so, its new style json message
    if (payload[0] != '{') { // old mqtt handling

        String Spayload;
        for (uint8_t i = 0; i < len; i++)
            Spayload.concat((char)payload[i]);

        bool stateOn = false;

        // old style Handle message topic
        if (String(config.mqtt_topic + MQTT_LIGHT_COMMAND_TOPIC).equals(topic)) {
        // Test if the payload is equal to "ON" or "OFF"
            if (Spayload.equals(String(LIGHT_ON))) {
                stateOn = true;
            } else if (Spayload.equals(String(LIGHT_OFF))) {
                stateOn = false;
            }
        }
        else if (String(config.mqtt_topic + MQTT_LIGHT_BRIGHTNESS_COMMAND_TOPIC).equals(topic)) {
            uint8_t brightness = Spayload.toInt();
            if (brightness > 100) brightness = 100;
            stateOn = true;
            effects.setBrightness(brightness/100);
        }
        else if (String(config.mqtt_topic + MQTT_LIGHT_RGB_COMMAND_TOPIC).equals(topic)) {
            // Get the position of the first and second commas
            uint8_t firstIndex = Spayload.indexOf(',');
            uint8_t lastIndex = Spayload.lastIndexOf(',');

            uint8_t m_rgb_red = Spayload.substring(0, firstIndex).toInt();
            uint8_t m_rgb_green = Spayload.substring(firstIndex + 1, lastIndex).toInt();
            uint8_t m_rgb_blue = Spayload.substring(lastIndex + 1).toInt();
            stateOn = true;
            effects.setColor( { m_rgb_red, m_rgb_green, m_rgb_blue } );
        }

        // Set data source based on state - Fall back to E131 when off
        if (stateOn) {
            if (effects.getEffect().equalsIgnoreCase("Disabled"))
                effects.setEffect("Solid");
            config.ds = DataSource::MQTT;
        } else {
            config.ds = DataSource::E131;
            effects.clearAll();
        }
        publishRGBState();
        publishRGBBrightness();
        publishRGBColor();
        runningEffectSendAll("mqtt");

    } else {

        DynamicJsonDocument r(1024);
        DeserializationError error = deserializeJson(r, payload);

        if (error) {
            LOG_PORT.println(F("MQTT: json Parsing failed"));
            return;
        }

        JsonObject root = r.as<JsonObject>();

        // if its a retained message and we want clean session, ignore it
        if ( properties.retain && config.mqtt_clean ) {
            return;
        }

        bool stateOn = false;

        if (root.containsKey("state")) {
            if (strcmp(root["state"], LIGHT_ON) == 0) {
                stateOn = true;
            } else if (strcmp(root["state"], LIGHT_OFF) == 0) {
                stateOn = false;
            }
        }

        if (root.containsKey("brightness")) {
            effects.setBrightness((float)root["brightness"] / 255.0);
        }

        if (root.containsKey("speed")) {
            effects.setSpeed(root["speed"]);
        }

        if (root.containsKey("color")) {
            effects.setColor({
                root["color"]["r"],
                root["color"]["g"],
                root["color"]["b"]
            });
        }

        if (root.containsKey("effect")) {
            // Set the explict effect provided by the MQTT client
            effects.setEffect(root["effect"]);
        }

        if (root.containsKey("reverse")) {
            effects.setReverse(root["reverse"]);
        }

        if (root.containsKey("mirror")) {
            effects.setMirror(root["mirror"]);
        }

        if (root.containsKey("allleds")) {
            effects.setAllLeds(root["allleds"]);
        }

        // Set data source based on state - Fall back to E131 when off
        if (stateOn) {
            if (effects.getEffect().equalsIgnoreCase("Disabled"))
                effects.setEffect("Solid");
            config.ds = DataSource::MQTT;
        } else {
            config.ds = DataSource::E131;
            effects.clearAll();
        }

        publishState();
        runningEffectSendAll("mqtt");
    }
}

void publishHA(bool join) {
    // Setup HA discovery
    char chipId[7] = { 0 };
    snprintf(chipId, sizeof(chipId), "%06x", ESP.getChipId());
    String ha_config = config.mqtt_haprefix + "/light/" + String(chipId) + "/config";

    if (join) {
        DynamicJsonDocument root(1024);
        root["name"] = config.id;
        root["schema"] = "json";
        root["state_topic"] = config.mqtt_topic;
        root["command_topic"] = config.mqtt_topic + "/set";
        root["rgb"] = "true";
        root["brightness"] = "true";
        root["effect"] = "true";
        JsonArray effect_list = root.createNestedArray("effect_list");
        // effect[0] is 'disabled', skip it
        for (uint8_t i = 1; i < effects.getEffectCount(); i++) {
            effect_list.add(effects.getEffectInfo(i)->name);
        }

        char buffer[measureJson(root) + 1];
        serializeJson(root, buffer, sizeof(buffer));
        mqtt.publish(ha_config.c_str(), 0, true, buffer);
    } else {
        mqtt.publish(ha_config.c_str(), 0, true, "");
    }
}

void publishState() {

    DynamicJsonDocument root(1024);
    if ((config.ds != DataSource::E131) && (!effects.getEffect().equalsIgnoreCase("Disabled")))
        root["state"] = LIGHT_ON;
    else
        root["state"] = LIGHT_OFF;

    JsonObject color = root.createNestedObject("color");
    color["r"] = effects.getColor().r;
    color["g"] = effects.getColor().g;
    color["b"] = effects.getColor().b;
    root["brightness"] = effects.getBrightness()*255;
    root["speed"] = effects.getSpeed();
    if (!effects.getEffect().equalsIgnoreCase("Disabled")) {
        root["effect"] = effects.getEffect();
    }
    root["reverse"] = effects.getReverse();
    root["mirror"] = effects.getMirror();
    root["allleds"] = effects.getAllLeds();

    char buffer[measureJson(root) + 1];
    serializeJson(root, buffer, sizeof(buffer));
    mqtt.publish(config.mqtt_topic.c_str(), 0, true, buffer);
}

// Called to publish the state of the led (on/off)
void publishRGBState() {
//    if (effects.getEffect()) {
    if (config.ds != DataSource::E131) {
        mqtt.publish(String(config.mqtt_topic + MQTT_LIGHT_STATE_TOPIC).c_str(), 0, true, LIGHT_ON);
    } else {
        mqtt.publish(String(config.mqtt_topic + MQTT_LIGHT_STATE_TOPIC).c_str(), 0, true, LIGHT_OFF);
    }
}

// Called to publish the brightness of the led (0-100)
void publishRGBBrightness() {
    snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%d", (uint8_t)(effects.getBrightness()*100));
    mqtt.publish(String(config.mqtt_topic + MQTT_LIGHT_BRIGHTNESS_STATE_TOPIC).c_str(), 0, true, m_msg_buffer);
}

// Called to publish the colors of the led (xx(x),xx(x),xx(x))
void publishRGBColor() {
    snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%d,%d,%d", effects.getColor().r, effects.getColor().g, effects.getColor().b);
    mqtt.publish(String(config.mqtt_topic + MQTT_LIGHT_RGB_STATE_TOPIC).c_str(), 0, true, m_msg_buffer);
}

/////////////////////////////////////////////////////////
//
//  Web Section
//
/////////////////////////////////////////////////////////

// Configure and start the web server
void initWeb() {
    // Handle OTA update from asynchronous callbacks
    Update.runAsync(true);

    // Add header for SVG plot support?
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

    // Setup WebSockets
    ws.onEvent(wsEvent);
    web.addHandler(&ws);

    // Heap status handler
    web.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", String(ESP.getFreeHeap()));
    });

    // JSON Config Handler
    web.on("/conf", HTTP_GET, [](AsyncWebServerRequest *request) {
        String jsonString;
        serializeConfig(jsonString, true);
        request->send(200, "text/json", jsonString);
    });

    // Firmware upload handler - only in station mode
    web.on("/updatefw", HTTP_POST, [](__attribute__ ((unused)) AsyncWebServerRequest *request) {
        ws.textAll("X6");
    }, handle_fw_upload).setFilter(ON_STA_FILTER);

    // Static Handler
    web.serveStatic("/", SPIFFS, "/www/").setDefaultFile("index.html");

    // Raw config file Handler - but only on station
//  web.serveStatic("/config.json", SPIFFS, "/config.json").setFilter(ON_STA_FILTER);
    web.serveStatic("/config.json", SPIFFS, "/config.json");

    web.onNotFound([](AsyncWebServerRequest *request) {
        if (request->method() == HTTP_OPTIONS) {
            AsyncWebServerResponse *response = request->beginResponse(200);
            request->send(response);
        } else {
            request->send(404, "text/plain", "Page not found");
        }
    });

    DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Origin"), "*");

    // Config file upload handler - only in station mode
    web.on("/config", HTTP_POST, [](__attribute__ ((unused)) AsyncWebServerRequest *request) {
        ws.textAll("X6");
    }, handle_config_upload).setFilter(ON_STA_FILTER);

    web.begin();

    LOG_PORT.print(F("- Web Server started on port "));
    LOG_PORT.println(HTTP_PORT);
}

/////////////////////////////////////////////////////////
//
//  JSON / Configuration Section
//
/////////////////////////////////////////////////////////

// Configuration Validations
void validateConfig() {
    // E1.31 Limits
    if (config.universe < 1)
        config.universe = 1;

    if (config.universe_limit > UNIVERSE_MAX || config.universe_limit < 1)
        config.universe_limit = UNIVERSE_MAX;

    if (config.channel_start < 1)
        config.channel_start = 1;
    else if (config.channel_start > config.universe_limit)
        config.channel_start = config.universe_limit;

    // Set default MQTT port if missing
    if (config.mqtt_port == 0)
        config.mqtt_port = MQTT_PORT;

    // Generate default MQTT topic if blank
    if (!config.mqtt_topic.length()) {
        char chipId[7] = { 0 };
        snprintf(chipId, sizeof(chipId), "%06x", ESP.getChipId());
        config.mqtt_topic = "diy/esps/" + String(chipId);
    }

    // Set default Home Assistant Discovery prefix if blank
    if (!config.mqtt_haprefix.length()) {
        config.mqtt_haprefix = "homeassistant";
    }

#if defined(ESPS_SUPPORT_PWM)
    config.devmode.MPWM = true;
#endif

#if defined(ESPS_MODE_PIXEL)
    // Set Mode
    config.devmode.MPIXEL = true;
    config.devmode.MSERIAL = false;

    // Generic channel limits for pixels
    if (config.channel_count % 3)
        config.channel_count = (config.channel_count / 3) * 3;

    if (config.channel_count > PIXEL_LIMIT * 3)
        config.channel_count = PIXEL_LIMIT * 3;
    else if (config.channel_count < 1)
        config.channel_count = 1;

    if (config.groupSize > config.channel_count / 3)
        config.groupSize = config.channel_count / 3;
    else if (config.groupSize < 1)
        config.groupSize = 1;

    // GECE Limits
    if (config.pixel_type == PixelType::GECE) {
        config.pixel_color = PixelColor::RGB;
        if (config.channel_count > 63 * 3)
            config.channel_count = 63 * 3;
    }

    // Default gamma value
    if (config.gammaVal <= 0) {
        config.gammaVal = 2.2;
    }

    // Default brightness value
    if (config.briteVal <= 0) {
        config.briteVal = 1.0;
    }
#elif defined(ESPS_MODE_SERIAL)
    // Set Mode
    config.devmode.MPIXEL = false;
    config.devmode.MSERIAL = true;

    // Generic serial channel limits
    if (config.channel_count > RENARD_LIMIT)
        config.channel_count = RENARD_LIMIT;
    else if (config.channel_count < 1)
        config.channel_count = 1;

    if (config.serial_type == SerialType::DMX512 && config.channel_count > UNIVERSE_MAX)
        config.channel_count = UNIVERSE_MAX;

    // Baud rate check
    if (config.baudrate > BaudRate::BR_460800)
        config.baudrate = BaudRate::BR_460800;
    else if (config.baudrate < BaudRate::BR_38400)
        config.baudrate = BaudRate::BR_57600;
#endif

    if (config.effect_speed < 1)
        config.effect_speed = 1;
    if (config.effect_speed > 10)
        config.effect_speed = 10;

    if (config.effect_brightness > 1.0)
        config.effect_brightness = 1.0;
    if (config.effect_brightness < 0.0)
        config.effect_brightness = 0.0;

    if (config.effect_idletimeout == 0) {
        config.effect_idletimeout = 10;
        config.effect_idleenabled = false;
    }
    if (config.effect_sendspeed > 100)
        config.effect_sendspeed = 100;
    if (config.effect_sendspeed <= 0.001f)
        config.effect_sendspeed = 0.001;

    effects.setFromConfig();
    if (config.effect_startenabled) {
        if (effects.isValidEffect(config.effect_name)) {
            effects.setEffect(config.effect_name);

            if ( !config.effect_name.equalsIgnoreCase("disabled")
              && !config.effect_name.equalsIgnoreCase("view") ) {
                config.ds = DataSource::WEB;
            }
        }
    } else {
        effects.setEffect("Disabled");
    }
}

void updateConfig() {
    // Validate first
    validateConfig();

    // Find the last universe we should listen for
    uint16_t span = config.channel_start + config.channel_count - 1;
    if (span % config.universe_limit)
        uniLast = config.universe + span / config.universe_limit;
    else
        uniLast = config.universe + span / config.universe_limit - 1;

    // Setup the sequence error tracker
    uint8_t uniTotal = (uniLast + 1) - config.universe;

    if (seqTracker) free(seqTracker);
    if ((seqTracker = static_cast<uint8_t *>(malloc(uniTotal))))
        memset(seqTracker, 0x00, uniTotal);

    if (seqError) free(seqError);
    if ((seqError = static_cast<uint32_t *>(malloc(uniTotal * 4))))
        memset(seqError, 0x00, uniTotal * 4);

    // Zero out packet stats
    e131.stats.num_packets = 0;

    // Initialize for our pixel type
#if defined(ESPS_MODE_PIXEL)
    pixels.begin(config.pixel_type, config.pixel_color, config.channel_count / 3);
    pixels.setGroup(config.groupSize, config.zigSize);
    updateGammaTable(config.gammaVal, config.briteVal);
    if (config.groupSize == 0) config.groupSize = 1;
    effects.begin(&pixels, config.channel_count / 3 / config.groupSize);

#elif defined(ESPS_MODE_SERIAL)
    serial.begin(&SEROUT_PORT, config.serial_type, config.channel_count, config.baudrate);
    effects.begin(&serial, config.channel_count / 3 );

#endif

    LOG_PORT.print(F("- Listening for "));
    LOG_PORT.print(config.channel_count);
    LOG_PORT.print(F(" channels, from Universe "));
    LOG_PORT.print(config.universe);
    LOG_PORT.print(F(" to "));
    LOG_PORT.println(uniLast);

    // Setup IGMP subscriptions if multicast is enabled
    if (config.multicast)
        multiSub();

    // Update Home Assistant Discovery if enabled
    if (config.mqtt)
        publishHA(config.mqtt_hadisco);
}

// De-Serialize Network config
void dsNetworkConfig(const JsonObject &json) {
    if (json.containsKey("network")) {
        JsonObject networkJson = json["network"];

        // Fallback to embedded ssid and passphrase if null in config
        config.ssid = networkJson["ssid"].as<String>();
        if (!config.ssid.length())
            config.ssid = ssid;

        config.passphrase = networkJson["passphrase"].as<String>();
        if (!config.passphrase.length())
            config.passphrase = passphrase;

        // Network
        for (int i = 0; i < 4; i++) {
            config.ip[i] = networkJson["ip"][i];
            config.netmask[i] = networkJson["netmask"][i];
            config.gateway[i] = networkJson["gateway"][i];
        }
        config.dhcp = networkJson["dhcp"];
        config.sta_timeout = networkJson["sta_timeout"] | CLIENT_TIMEOUT;
        if (config.sta_timeout < 5) {
            config.sta_timeout = 5;
        }

        config.ap_fallback = networkJson["ap_fallback"];
        config.ap_timeout = networkJson["ap_timeout"] | AP_TIMEOUT;
        if (config.ap_timeout < 15) {
            config.ap_timeout = 15;
        }

        config.udp_enabled = networkJson["udp_enabled"];
        config.udp_port = networkJson["udp_port"] | ESPS_UDP_RAW_DEFAULT_PORT;

        // Generate default hostname if needed
        config.hostname = networkJson["hostname"].as<String>();
    }
    else
    {
        LOG_PORT.println(F("No network settings found."));
    }

    if (!config.hostname.length()) {
        char chipId[7] = { 0 };
        snprintf(chipId, sizeof(chipId), "%06x", ESP.getChipId());
        config.hostname = "esps-" + String(chipId);
    }
}

// De-serialize Effect Config
void dsEffectConfig(const JsonObject &json) {
    // Effects
    if (json.containsKey("effects")) {
        JsonObject effectsJson = json["effects"];
        config.effect_name = effectsJson["name"].as<String>();
        config.effect_mirror = effectsJson["mirror"];
        config.effect_allleds = effectsJson["allleds"];
        config.effect_reverse = effectsJson["reverse"];
        if (effectsJson.containsKey("speed"))
            config.effect_speed = effectsJson["speed"];
        config.effect_color = { effectsJson["r"], effectsJson["g"], effectsJson["b"] };
        if (effectsJson.containsKey("brightness"))
            config.effect_brightness = effectsJson["brightness"];
        config.effect_startenabled = effectsJson["startenabled"];
        config.effect_idleenabled = effectsJson["idleenabled"];
        config.effect_idletimeout = effectsJson["idletimeout"];
        config.effect_sendprotocol = effectsJson["sendprotocol"];
        config.effect_sendhost = effectsJson["sendhost"].as<String>();
        config.effect_sendport = effectsJson["sendport"];
        config.effect_sendspeed = effectsJson["sendspeed"] | 25;
    }
    else
    {
        LOG_PORT.println(F("No effect settings found."));
    }
}

// De-serialize Device Config
void dsDeviceConfig(const JsonObject &json) {
    // Device
    if (json.containsKey("device")) {
        config.id = json["device"]["id"].as<String>();
    }
    else
    {
        LOG_PORT.println(F("No device settings found."));
    }

    // E131
    if (json.containsKey("e131")) {
        config.universe = json["e131"]["universe"];
        config.universe_limit = json["e131"]["universe_limit"];
        config.channel_start = json["e131"]["channel_start"];
        config.channel_count = json["e131"]["channel_count"];
        config.multicast = json["e131"]["multicast"];
    }
    else
    {
        LOG_PORT.println(F("No e131 settings found."));
    }

    // MQTT
    if (json.containsKey("mqtt")) {
        JsonObject mqttJson = json["mqtt"];
        config.mqtt = mqttJson["enabled"];
        config.mqtt_ip = mqttJson["ip"].as<String>();
        config.mqtt_port = mqttJson["port"];
        config.mqtt_user = mqttJson["user"].as<String>();
        config.mqtt_password = mqttJson["password"].as<String>();
        config.mqtt_topic = mqttJson["topic"].as<String>();
        config.mqtt_clean = mqttJson["clean"] | false;
        config.mqtt_hadisco = mqttJson["hadisco"] | false;
        config.mqtt_haprefix = mqttJson["haprefix"].as<String>();
    }
    else
    {
        LOG_PORT.println(F("No mqtt settings found."));
    }

#if defined(ESPS_MODE_PIXEL)
    // Pixel
    if (json.containsKey("pixel")) {
        config.pixel_type = PixelType(static_cast<uint8_t>(json["pixel"]["type"]));
        config.pixel_color = PixelColor(static_cast<uint8_t>(json["pixel"]["color"]));
        config.groupSize = json["pixel"]["groupSize"];
        config.zigSize = json["pixel"]["zigSize"];
        config.gammaVal = json["pixel"]["gammaVal"];
        config.briteVal = json["pixel"]["briteVal"];
    }
    else
    {
        LOG_PORT.println(F("No pixel settings found."));
    }

#elif defined(ESPS_MODE_SERIAL)
    // Serial
    if (json.containsKey("serial")) {
        config.serial_type = SerialType(static_cast<uint8_t>(json["serial"]["type"]));
        config.baudrate = BaudRate(static_cast<uint32_t>(json["serial"]["baudrate"]));
    }
    else
    {
        LOG_PORT.println(F("No serial settings found."));
    }
#endif

#if defined(ESPS_SUPPORT_PWM)
    /* PWM */
    if (json.containsKey("pwm")) {
        JsonObject pwmJson = json["pwm"];
        config.pwm_global_enabled = pwmJson["enabled"];
        config.pwm_freq = pwmJson["freq"];
        config.pwm_gamma = pwmJson["gamma"];
        config.pwm_gpio_invert = 0;
        config.pwm_gpio_digital = 0;
        config.pwm_gpio_enabled = 0;
        for (int gpio = 0; gpio < NUM_GPIO; gpio++) {
            JsonObject gpioJson = pwmJson["gpio"][(String)gpio];

            config.pwm_gpio_comment[gpio] = gpioJson["comment"].as<String>();
            if (pwm_valid_gpio_mask & 1<<gpio) {
                config.pwm_gpio_dmx[gpio] = gpioJson["channel"];
                if (gpioJson["invert"])
                    config.pwm_gpio_invert |= 1<<gpio;
                if (gpioJson["digital"])
                    config.pwm_gpio_digital |= 1<<gpio;
                if (gpioJson["enabled"])
                    config.pwm_gpio_enabled |= 1<<gpio;

            }
        }
    }
    else
    {
        LOG_PORT.println(F("No pwm settings found."));
    }
#endif
}

// Load configugration JSON file
void loadConfig() {
    // Zeroize Config struct
    memset(&config, 0, sizeof(config));

    // default to ap_fallback if config file cant be read
    config.ap_fallback = true;

    effects.setFromDefaults();

    // Load CONFIG_FILE json. Create and init with defaults if not found
    File file = SPIFFS.open(CONFIG_FILE, "r");
    if (!file) {
        LOG_PORT.println(F("- No configuration file found."));
        config.ssid = ssid;
        config.passphrase = passphrase;
        char chipId[7] = { 0 };
        snprintf(chipId, sizeof(chipId), "%06x", ESP.getChipId());
        config.hostname = "esps-" + String(chipId);
        config.ap_fallback = true;
        saveConfig();
    } else {
        // Parse CONFIG_FILE json
        size_t size = file.size();
        if (size > CONFIG_MAX_SIZE) {
            LOG_PORT.println(F("*** Configuration File too large ***"));
            return;
        }

        std::unique_ptr<char[]> buf(new char[size]);
        file.readBytes(buf.get(), size);

        DynamicJsonDocument json(size);
        DeserializationError error = deserializeJson(json, buf.get());
        if (error) {
            LOG_PORT.println(F("*** Configuration File Format Error ***"));
            return;
        }

        dsNetworkConfig(json.as<JsonObject>());
        dsDeviceConfig(json.as<JsonObject>());
        dsEffectConfig(json.as<JsonObject>());

        LOG_PORT.println(F("- Configuration loaded."));
    }

    // Validate it
    validateConfig();

    effects.setFromConfig();

}

// Serialize the current config into a JSON string
void serializeConfig(String &jsonString, bool pretty, bool creds) {
    // Create buffer and root object
    DynamicJsonDocument json(2048);
    
    // Device
    JsonObject device = json.createNestedObject("device");
    device["id"] = config.id.c_str();
    device["mode"] = config.devmode.toInt();

    // Network
    JsonObject network = json.createNestedObject("network");
    network["ssid"] = config.ssid.c_str();
    if (creds)
        network["passphrase"] = config.passphrase.c_str();
    network["hostname"] = config.hostname.c_str();
    JsonArray ip = network.createNestedArray("ip");
    JsonArray netmask = network.createNestedArray("netmask");
    JsonArray gateway = network.createNestedArray("gateway");
    for (int i = 0; i < 4; i++) {
        ip.add(config.ip[i]);
        netmask.add(config.netmask[i]);
        gateway.add(config.gateway[i]);
    }
    network["dhcp"] = config.dhcp;
    network["sta_timeout"] = config.sta_timeout;

    network["ap_fallback"] = config.ap_fallback;
    network["ap_timeout"] = config.ap_timeout;

    network["udp_enabled"] = config.udp_enabled;
    network["udp_port"] = config.udp_port;

    // Effects
    JsonObject _effects = json.createNestedObject("effects");
    _effects["name"] = config.effect_name;

    _effects["mirror"] = config.effect_mirror;
    _effects["allleds"] = config.effect_allleds;
    _effects["reverse"] = config.effect_reverse;
    _effects["speed"] = config.effect_speed;
    _effects["brightness"] = config.effect_brightness;

    _effects["r"] = config.effect_color.r;
    _effects["g"] = config.effect_color.g;
    _effects["b"] = config.effect_color.b;

    _effects["brightness"] = config.effect_brightness;
    _effects["startenabled"] = config.effect_startenabled;
    _effects["idleenabled"] = config.effect_idleenabled;
    _effects["idletimeout"] = config.effect_idletimeout;
    _effects["sendprotocol"] = config.effect_sendprotocol;
    _effects["sendhost"] = config.effect_sendhost;
    _effects["sendport"] = config.effect_sendport;
    _effects["sendspeed"] = config.effect_sendspeed;

    // MQTT
    JsonObject _mqtt = json.createNestedObject("mqtt");
    _mqtt["enabled"] = config.mqtt;
    _mqtt["ip"] = config.mqtt_ip.c_str();
    _mqtt["port"] = config.mqtt_port;
    _mqtt["user"] = config.mqtt_user.c_str();
    _mqtt["password"] = config.mqtt_password.c_str();
    _mqtt["topic"] = config.mqtt_topic.c_str();
    _mqtt["clean"] = config.mqtt_clean;
    _mqtt["hadisco"] = config.mqtt_hadisco;
    _mqtt["haprefix"] = config.mqtt_haprefix.c_str();

    // E131
    JsonObject e131 = json.createNestedObject("e131");
    e131["universe"] = config.universe;
    e131["universe_limit"] = config.universe_limit;
    e131["channel_start"] = config.channel_start;
    e131["channel_count"] = config.channel_count;
    e131["multicast"] = config.multicast;

#if defined(ESPS_MODE_PIXEL)
    // Pixel
    JsonObject pixel = json.createNestedObject("pixel");
    pixel["type"] = static_cast<uint8_t>(config.pixel_type);
    pixel["color"] = static_cast<uint8_t>(config.pixel_color);
    pixel["groupSize"] = config.groupSize;
    pixel["zigSize"] = config.zigSize;
    pixel["gammaVal"] = config.gammaVal;
    pixel["briteVal"] = config.briteVal;

#elif defined(ESPS_MODE_SERIAL)
    // Serial
    JsonObject serial = json.createNestedObject("serial");
    serial["type"] = static_cast<uint8_t>(config.serial_type);
    serial["baudrate"] = static_cast<uint32_t>(config.baudrate);
#endif

#if defined(ESPS_SUPPORT_PWM)
    // PWM
    JsonObject pwm = json.createNestedObject("pwm");
    pwm["enabled"] = config.pwm_global_enabled;
    pwm["freq"] = config.pwm_freq;
    pwm["gamma"] = config.pwm_gamma;

    JsonObject gpioJ = pwm.createNestedObject("gpio");
    for (int gpio = 0; gpio < NUM_GPIO; gpio++ ) {
        JsonObject thisGpio = gpioJ.createNestedObject((String)gpio);
        thisGpio["comment"] = config.pwm_gpio_comment[gpio];
        if (pwm_valid_gpio_mask & 1<<gpio) {
            thisGpio["channel"] = static_cast<uint16_t>(config.pwm_gpio_dmx[gpio]);
            thisGpio["enabled"] = static_cast<bool>(config.pwm_gpio_enabled & 1<<gpio);
            thisGpio["invert"] = static_cast<bool>(config.pwm_gpio_invert & 1<<gpio);
            thisGpio["digital"] = static_cast<bool>(config.pwm_gpio_digital & 1<<gpio);
//LOG_PORT.println("serialize: config.pwm_gpio_comment");
//LOG_PORT.println(config.pwm_gpio_comment[gpio]);
        }
    }
#endif

    if (pretty)
        serializeJsonPretty(json, jsonString); 
    else
        serializeJson(json, jsonString);
}

#if defined(ESPS_MODE_PIXEL)
void dsGammaConfig(const JsonObject &json) {
    if (json.containsKey("pixel")) {
        config.gammaVal = json["pixel"]["gammaVal"];
        config.briteVal = json["pixel"]["briteVal"];

        if (config.gammaVal <= 0) { config.gammaVal = 2.2; }
        if (config.briteVal <= 0) { config.briteVal = 1.0; }

        updateGammaTable(config.gammaVal, config.briteVal);
    }
}
#endif

// Save configuration JSON file
void saveConfig() {
    // Update Config
    updateConfig();

    // Serialize Config
    String jsonString;
    serializeConfig(jsonString, true, true);

    // Save Config
    File file = SPIFFS.open(CONFIG_FILE, "w");
    if (!file) {
        LOG_PORT.println(F("*** Error creating configuration file ***"));
        return;
    } else {
        file.println(jsonString);
        LOG_PORT.println(F("* Configuration saved."));
    }
}

void sendData() {
    effects.sendUDPData();
    sendTimer.attach(1.0/config.effect_sendspeed, sendData);
}

void idleTimeout() {
   idleTicker.attach(config.effect_idletimeout, idleTimeout);
    if ( (config.effect_idleenabled) && (config.ds == DataSource::E131) ) {
        config.ds = DataSource::IDLEWEB;
        effects.setFromConfig();
    }
}

/////////////////////////////////////////////////////////
//
//  Main Loop
//
/////////////////////////////////////////////////////////
void loop() {

    /* check for rotary encoder and buttons */
#if defined(ESPS_ENABLE_BUTTONS)
    handleButtons();
#endif

    e131_packet_t packet;

    // Reboot handler
    if (reboot) {

        effects.clearAll();

#if defined(ESPS_MODE_PIXEL)
        while (!pixels.canRefresh()) {};
        pixels.show();
#elif defined(ESPS_MODE_SERIAL)
        while (!serial.canRefresh()) {};
        serial.show();
#endif

#if defined(ESPS_SUPPORT_PWM)
        handlePWM();
#endif

        delay(REBOOT_DELAY);
        ESP.restart();
    }

    // Render output for current data source
    if ( (config.ds == DataSource::E131) || (config.ds == DataSource::IDLEWEB) ) {
        // Parse a packet and update pixels
        if (!e131.isEmpty()) {
            idleTicker.attach(config.effect_idletimeout, idleTimeout);
            if (config.ds == DataSource::IDLEWEB) {
                config.ds = DataSource::E131;
            }

            e131.pull(&packet);
            uint16_t universe = htons(packet.universe);
            uint8_t *data = packet.property_values + 1;
            //LOG_PORT.print(universe);
            //LOG_PORT.println(packet.sequence_number);
            if ((universe >= config.universe) && (universe <= uniLast)) {
                // Universe offset and sequence tracking
                uint8_t uniOffset = (universe - config.universe);
                if (packet.sequence_number != seqTracker[uniOffset]++) {
                    LOG_PORT.print(F("Sequence Error - expected: "));
                    LOG_PORT.print(seqTracker[uniOffset] - 1);
                    LOG_PORT.print(F(" actual: "));
                    LOG_PORT.print(packet.sequence_number);
                    LOG_PORT.print(F(" universe: "));
                    LOG_PORT.println(universe);
                    seqError[uniOffset]++;
                    seqTracker[uniOffset] = packet.sequence_number + 1;
                }

                // Offset the channels if required
                uint16_t offset = 0;
                offset = config.channel_start - 1;

                // Find start of data based off the Universe
                int16_t dataStart = uniOffset * config.universe_limit - offset;

                // Calculate how much data we need for this buffer
                uint16_t dataStop = config.channel_count;
                uint16_t channels = htons(packet.property_value_count) - 1;
                if (config.universe_limit < channels)
                    channels = config.universe_limit;
                if ((dataStart + channels) < dataStop)
                    dataStop = dataStart + channels;

                // Set the data
                uint16_t buffloc = 0;

                // ignore data from start of first Universe before channel_start
                if (dataStart < 0) {
                    dataStart = 0;
                    buffloc = config.channel_start - 1;
                }

                for (int i = dataStart; i < dataStop; i++) {
#if defined(ESPS_MODE_PIXEL)
                    pixels.setValue(i, data[buffloc]);
#elif defined(ESPS_MODE_SERIAL)
                    serial.setValue(i, data[buffloc]);
#endif
                    buffloc++;
                }
            }
        }
    }

    if ( (config.ds == DataSource::WEB)
      || (config.ds == DataSource::IDLEWEB)
      || (config.ds == DataSource::MQTT) ) {
            effects.run();
    }

    handleToggleGpio();

/* Streaming refresh */
#if defined(ESPS_MODE_PIXEL)
    if (pixels.canRefresh())
        pixels.show();
#elif defined(ESPS_MODE_SERIAL)
    if (serial.canRefresh())
        serial.show();
#endif

/* Update the PWM outputs */
#if defined(ESPS_SUPPORT_PWM)
  handlePWM();
#endif

// workaround crash - consume incoming bytes on serial port
    if (LOG_PORT.available()) {
        while (LOG_PORT.read() >= 0);
    }

    MDNS.update();
}

void resolveHosts() {
// no point trying if we're an AP
    if (WiFi.getMode() == WIFI_STA) {
        IPAddress tempIP;

        if (config.effect_sendprotocol) {
// dont know why this doesnt work...
//          WiFi.hostByName(config.effect_sendhost.c_str(), config.effect_sendIP);

            WiFi.hostByName(config.effect_sendhost.c_str(), tempIP);

            if ( ( tempIP[0] & 0b11100000 ) == 224 ) {
                config.effect_sendmulticast = true;
            } else {
                config.effect_sendmulticast = false;
            }
            config.effect_sendIP = tempIP;
        }
    }
}
