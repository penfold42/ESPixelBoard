class OLEDEngine;

#define LOG_PORT        Serial

class OLEDEngine {
  
  public: 
    OLEDEngine();
    String getDisplayElements();
    String getDisplayConfig();
    String getDisplayEvents();
    void saveDisplayConfig(JsonObject &data);
    void readDisplayConfigJson();
  private:
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

    dispconf_t  dispconfig;
    // void populateConfig(JsonObject &data);
};
