#ifndef OLEDENGINE_H_
#define OLEDENGINE_H_
#include <ArduinoJson.h>
#include "images.h"
#define LOG_PORT        Serial

const char DISP_CONFIG_FILE[] = "/display.json";

typedef struct {
  int16_t       enabled;
  String        elementid;
  int16_t       px;
  int16_t       py;
  int16_t       format;
  String        text;
} element_t;

typedef struct {
  String        eventid;
  element_t*    elements;
  size_t         size;
} event_t;

typedef struct {
  event_t*     events;
  size_t         size;
} dispconf_t;

void initDisplay();
String getDisplayElements();
String getDisplayConfig();
String getDisplayEvents();
void saveDisplayConfig(JsonObject &data);
void showDisplay(const String event, const String data);
void showElementValue(const element_t elem, const String custData);
#endif // OLEDENGINE_H_
