#ifndef GPIO_H_
#define GPIO_H_
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include "ESPixelStick.h"

void setupWebGpio();
String handleGPIO ( String request );
void handleWebGPIO (AsyncWebServerRequest *request);
int splitString(char separator, String input, String results[], int numStrings);
void handleToggleGpio();

#define toggleMS 200

#endif /* GPIO_H_ */
