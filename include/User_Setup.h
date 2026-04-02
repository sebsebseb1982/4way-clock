// TFT_eSPI User Setup for CYD (ESP32-2432S028)
// ILI9341 2.8" 320x240, landscape orientation
// Do NOT edit unless you know the CYD pin mapping.

#define USER_SETUP_LOADED 1

// Driver
#define ILI9341_DRIVER

// Resolution
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// SPI pins (HSPI bus on CYD)
#define TFT_MOSI 13
#define TFT_MISO 12
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1   // Not connected; pull RST high via hardware

// Backlight (active HIGH, PWM capable)
#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH

// SPI frequency
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  20000000

// Color order
#define TFT_RGB_ORDER TFT_BGR

// Fonts (built-in)
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_GFXFF
#define SMOOTH_FONT

// DMA (improves performance on ESP32)
#define SUPPORT_TRANSACTIONS
