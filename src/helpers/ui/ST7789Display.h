#pragma once

#include "DisplayDriver.h"
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include "ST7789Spi.h"

class ST7789Display : public DisplayDriver {
  ST7789Spi display;
  bool _isOn;
  uint16_t _color;

  // HiveFW — RGB565 real da barra superior.
  uint16_t _headerAccent = ST77XX_RED;

  // HiveFW — a barra superior só é aplicada ao frame
  // quando setHeaderAccent() for chamado explicitamente.
  bool _headerAccentEnabled = false;

  // HiveFW — cores exclusivas do bitmap do BOOT LOGO.
  bool _bootLogoAccentEnabled = false;
  uint16_t _bootLogoAccent = ST77XX_WHITE;
  uint16_t _bootTextAccent = ST77XX_WHITE;

  // HiveFW — família e tamanho lógico atuais.
  // 0 = ArialMT
  // 1 = Geist Sans
  uint8_t _uiFont = 0;
  uint8_t _uiTextSize = 1;


  // 0 = orientação HiveFW atual
  // 1 = invertido 180 graus
  uint8_t _rotation = 0;

  void applyRotation();

  int _x=0, _y=0;

  bool i2c_probe(TwoWire& wire, uint8_t addr);
public:
#if defined(HELTEC_VISION_MASTER_T190)
  ST7789Display() : DisplayDriver(128, 64), display(&SPI, PIN_TFT_RST, PIN_TFT_DC, PIN_TFT_CS, GEOMETRY_RAWMODE, 320, 170,PIN_TFT_SDA,-1,PIN_TFT_SCL) {_isOn = false;}
#elif defined(THINKNODE_M9)
  ST7789Display() : DisplayDriver(128, 64), display(&SPI, ST7789_RESET, ST7789_RS, ST7789_CS, GEOMETRY_RAWMODE, 320, 240, ST7789_SDA, ST7789_MISO, ST7789_SCK) {_isOn = false;}
#else
  ST7789Display() : DisplayDriver(128, 64), display(&SPI1, PIN_TFT_RST, PIN_TFT_DC, PIN_TFT_CS, GEOMETRY_RAWMODE, 240, 135) {_isOn = false;}
#endif
  bool begin();

  bool isOn() override { return _isOn; }
  void turnOn() override;
  void turnOff() override;
  void clear() override;
  void startFrame(ColorVal bkg = UIColor::window_bkg) override;
  void setTextSize(int sz) override;
  void setUIFont(uint8_t fontIndex) override;

  // HiveFW T114:
  // manter UTF-8 intacto para o renderer OLEDDisplay,
  // que já converte corretamente UTF-8 -> Latin-1.
  void translateUTF8ToBlocks(
    char* dest,
    const char* src,
    size_t dest_size
  ) override;

  void setColor(ColorVal c) override;
  void setHeaderAccent(uint8_t colorIndex) override;
  void setBootLogoAccent(
    uint8_t logoColorIndex,
    uint8_t textColorIndex
  ) override;
  void setDisplayRotation(uint8_t rotation) override;
  void setCursor(int x, int y) override;
  void print(const char* str) override;
  void printWordWrap(const char* str, int max_width) override;
  void fillRect(int x, int y, int w, int h) override;
  void drawRect(int x, int y, int w, int h) override;
  void drawXbm(int x, int y, const uint8_t* bits, int w, int h) override;
  uint16_t getTextWidth(const char* str) override;
  void endFrame() override;
};
