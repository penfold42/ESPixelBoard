/*
Generic GPIO interface via the web interface

IMPLEMENTED URLs:
/uptime            // show uptime, heap, signal strength

/gpio/<n>/get      // read gpio <n>
/gpio/<n>/set/0    // set to low
/gpio/<n>/set/1    // set to high
/gpio/<n>/mode/0   // set to input
/gpio/<n>/mode/1   // set to output
/gpio/<n>/toggle/101  // toggle the output 1,0,1 with 200ms delay

NOT IMPLEMENTED:
/acl/<row>/set/aaa.bbb.ccc.ddd.eee/mask  // add aaa.bbb.ccc.ddd to the allowed IP list in slot <row>
/acl/<row>/get    // show allowed IP list in slot <row>

*/

#include "gpio.h"

int gpio;
int state = -1;
int toggleCounter = -1;
String toggleString;
int toggleGpio = -1;

using timeType = decltype(millis());

timeType this_mill;
timeType last_gpio_mill;
timeType last_mill;
unsigned long long extended_mill;
unsigned long long mill_rollover_count;

extern AsyncWebServer  web; // Web Server
extern uint32_t        pwm_valid_gpio_mask;
extern config_t        config;


void handleWebGPIO (AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("text/html");

    response->print( handleGPIO(request->url()) );
    request->send(response);
}

String handleGPIO ( String request ) {
#if defined (ESPS_SUPPORT_PWM)
    String reply;
    char tempstr[40];

    String substrings[10];
    splitString('/', request, substrings, sizeof(substrings) / sizeof(substrings[0]));
//  /gpio/<n>/set/1  
//    1    2   3  4

    gpio = substrings[2].toInt();
    // distinguish between valid gpio 0 and invalid data (which also return 0 *sigh*)
    if ((gpio == 0) && (substrings[2].charAt(0) != '0')) {
      gpio = -1;
    }

    if ( ( gpio >= 0 )
      && ( pwm_valid_gpio_mask & 1<<gpio )
      && ( config.pwm_gpio_enabled & 1<<gpio ) ) {

        if (substrings[3] == "get") {
            snprintf(tempstr, 40, "gpio%d is %d\r\n", gpio, digitalRead(gpio));

        } else if (substrings[3] == "set") {
            int state = substrings[4].toInt();
            digitalWrite(gpio, state);
            snprintf(tempstr, 40, "gpio%d set to %d\r\n", gpio, state);

        } else if (substrings[3] == "toggle") {
            snprintf(tempstr, 40, "gpio%d toggled to %s", gpio, substrings[4].c_str());
            toggleGpio = gpio;
            toggleString = substrings[4];
            toggleCounter = 0;

        } else if (substrings[3] == "mode") {
            if (substrings[4].charAt(0) == 'S') {
                pinMode(gpio, SPECIAL);
            }
            else if (substrings[4].charAt(0) == '?') {
// add query pin mode when i can work out how!
            } else {
              int state = substrings[4].toInt();
              pinMode(gpio, state);
              snprintf(tempstr, 40, "gpio%d mode %d\r\n", gpio, state);
            }
        }
    } else {
        snprintf(tempstr, 40, "Invalid gpio %d\r\n",gpio);
    }

    return (String) tempstr;
#else
    return (String) "PWM disabled\r\n";
#endif
}


int splitString(char separator, String input, String results[], int numStrings) {
    int idx;
    int last_idx = 0;
    int retval = 0;

    for (int i = 0; i < numStrings; i++) {
        results[i] = "";     // pre clear this
        idx = input.indexOf(separator, last_idx);
        if ((idx == -1) && (last_idx == -1)) {
            break;
        } else {
            results[i] = input.substring(last_idx, idx);
            retval ++;

            if (idx != -1) {
                last_idx = idx + 1;
            } else {
                last_idx = -1;
            }
        }
    }
    return retval;
}


void handleToggleGpio() {

  this_mill = millis();
  if (last_mill > this_mill) {  // rollover
      mill_rollover_count ++;
  }
  last_mill = this_mill;
  extended_mill = (mill_rollover_count << (8*sizeof(this_mill))) + this_mill;

  if (( this_mill - last_gpio_mill ) >= toggleMS) {
    last_gpio_mill = this_mill;
    
    if (toggleCounter >= 0) {
      if (toggleString.charAt(toggleCounter) == '1') {
        digitalWrite(toggleGpio, 1);
      } else {
        digitalWrite(toggleGpio, 0);
      }

      toggleCounter++;
      if (toggleCounter >= (signed)toggleString.length()) {
        toggleCounter = -1;
      }
    }
  }
}

void setupWebGpio() {
    last_gpio_mill = millis();  // initial setup
    // gpio url handler
    web.on("/gpio", HTTP_GET, handleWebGPIO).setFilter(ON_STA_FILTER);

    // uptime Handler
    web.on("/uptime", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("text/plain");
        long int secs = (extended_mill/1000);
        long int mins = (secs/60);
        long int hours = (mins/60);
        long int days = (hours/24);

        response->printf ("Uptime: %d days, %02d:%02d:%02d.%03d\r\nFreeHeap: %x\r\nSignal: %d\r\n", 
                (int)days, (int)hours%24, (int)mins%60, (int)secs%60, (int)extended_mill%1000, ESP.getFreeHeap(), WiFi.RSSI() );
        request->send(response);
    });
}

