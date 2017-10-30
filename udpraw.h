#if defined(ESPS_MODE_PIXEL)
extern PixelDriver     pixels;         // Pixel object
#elif defined(ESPS_MODE_SERIAL)
extern SerialDriver    serial;         // Serial object
#endif

void handle_raw_port();

