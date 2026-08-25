// Setup file for the generic "ESP32S3 1.54 TFT LCD V1.0" (ZJYUNJIE) board
// sold rebranded as "MinerMaker" - ST7789 240x240 SPI, ESP32-S3-WROOM-1 N8R8
#define USER_SETUP_ID 999

#define ST7789_DRIVER

#define TFT_WIDTH  240
#define TFT_HEIGHT 240

#define CGRAM_OFFSET

#define TFT_RGB_ORDER TFT_BGR

#define TFT_INVERSION_ON

#define TFT_MOSI 39
#define TFT_SCLK 40
#define TFT_CS   41
#define TFT_DC   42
#define TFT_RST  38

#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SMOOTH_FONT

#define SPI_FREQUENCY  27000000
#define SPI_READ_FREQUENCY  16000000
