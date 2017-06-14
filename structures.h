#define ESPS_MODE_PIXEL

#if defined(ESPS_MODE_PIXEL)
#include "PixelDriver.h"
extern PixelDriver     pixels;         /* Pixel object */
#elif defined(ESPS_MODE_SERIAL)
#include "SerialDriver.h"
extern  SerialDriver    serial;         /* Serial object */
#else
#error "No valid output mode defined."
#endif

/* Pixel Types */
enum class DevMode : uint8_t {
    MPIXEL,
    MSERIAL
};

/* Test Modes */
enum class TestMode : uint8_t {
    DISABLED,
    STATIC,
    CHASE,
    RAINBOW,
    VIEW_STREAM
};

typedef struct {
    uint8_t r,g,b;              //hold requested color
    uint16_t step;               //step in testing routine
    uint32_t last;              //last update
} testing_t;

/* Configuration structure */
typedef struct {
    /* Device */
    String      id;             /* Device ID */
    DevMode     devmode;           /* Device Mode - used for reporting mode, can't be set */
    TestMode    testmode;       /* Testing mode */

    /* Network */
    String      ssid;
    String      passphrase;
    String      hostname;
    uint8_t     ip[4];
    uint8_t     netmask[4];
    uint8_t     gateway[4];
    bool        dhcp;           /* Use DHCP */
    bool        ap_fallback;    /* Fallback to AP if fail to associate */

    /* E131 */
    uint16_t    universe;       /* Universe to listen for */
    uint16_t    channel_start;  /* Channel to start listening at - 1 based */
    uint16_t    channel_count;  /* Number of channels */
    bool        multicast;      /* Enable multicast listener */

#if defined(ESPS_MODE_PIXEL)
    /* Pixels */
    PixelType   pixel_type;     /* Pixel type */
    PixelColor  pixel_color;    /* Pixel color order */
    bool        gamma;          /* Use gamma map? */

#elif defined(ESPS_MODE_SERIAL)
    /* Serial */
    SerialType  serial_type;    /* Serial type */
    BaudRate    baudrate;       /* Baudrate */
#endif
} config_t;

