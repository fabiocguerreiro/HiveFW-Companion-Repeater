#ifdef ST7789

#include "ST7789Display.h"

#ifndef X_OFFSET
#define X_OFFSET 0  // No offset needed for landscape
#endif

#ifndef Y_OFFSET
#define Y_OFFSET 1  // Vertical offset to prevent top row cutoff
#endif

#ifdef HELTEC_VISION_MASTER_T190
  #define SCALE_X  2.5f        // 320 / 128
  #define SCALE_Y  2.65625f    // 170 / 64
#else
  #define SCALE_X  1.875f      // 240 / 128
  #define SCALE_Y  2.109375f   // 135 / 64
#endif

#ifdef DISPLAY_SCALE_X
  #define SCALE_X DISPLAY_SCALE_X
#endif

#ifdef DISPLAY_SCALE_Y
  #define SCALE_Y DISPLAY_SCALE_Y
#endif

// Color scheme
ColorVal UIColor::window_bkg = OLEDDISPLAY_COLOR::BLACK;

#ifdef HELTEC_T114_WITH_DISPLAY

// HiveFW T114:
// framebuffer:
//   barra = pixels ativos
//   texto = pixels apagados
//
// O writer RGB converte os pixels ativos desta zona
// diretamente para a cor selecionada durante a passagem normal.
ColorVal UIColor::title_bkg = OLEDDISPLAY_COLOR::WHITE;
ColorVal UIColor::title_txt = OLEDDISPLAY_COLOR::BLACK;

#else

ColorVal UIColor::title_bkg = OLEDDISPLAY_COLOR::BLACK;
ColorVal UIColor::title_txt = OLEDDISPLAY_COLOR::WHITE;

#endif
ColorVal UIColor::primary_txt = OLEDDISPLAY_COLOR::WHITE;
ColorVal UIColor::secondary_txt = OLEDDISPLAY_COLOR::WHITE;
ColorVal UIColor::warning_txt = OLEDDISPLAY_COLOR::WHITE;
ColorVal UIColor::popup_bkg = OLEDDISPLAY_COLOR::BLACK;
ColorVal UIColor::popup_txt = OLEDDISPLAY_COLOR::WHITE;
ColorVal UIColor::corp_blue = OLEDDISPLAY_COLOR::WHITE;


void ST7789Display::applyRotation() {

#ifdef HELTEC_T114_WITH_DISPLAY

  if (_rotation == 1) {

    // Landscape oposto ao modo HiveFW original.
    display.flipScreenVertically();

  } else {

    // Orientação original do HiveFW/T114.
    display.landscapeScreen();
  }

#else

  // Preservar comportamento dos restantes ST7789.
  display.landscapeScreen();

#ifdef DISPLAY_FLIP_VERTICALLY
  display.flipScreenVertically();
#endif

#endif
}


bool ST7789Display::begin() {
  if(!_isOn) {
    pinMode(PIN_TFT_VDD_CTL, OUTPUT);
    pinMode(PIN_TFT_LEDA_CTL, OUTPUT);
    digitalWrite(PIN_TFT_VDD_CTL, LOW);
  #ifdef PIN_TFT_LEDA_CTL_ACTIVE
    digitalWrite(PIN_TFT_LEDA_CTL, PIN_TFT_LEDA_CTL_ACTIVE);
  #else
    digitalWrite(PIN_TFT_LEDA_CTL, LOW);
  #endif
    digitalWrite(PIN_TFT_RST, HIGH);

    display.init();
    applyRotation();
    display.displayOn();
    setCursor(0,0);

    _isOn = true;
  }
  return true;
}

void ST7789Display::turnOn() {
  if (!_isOn) {
    // Restore power to the display but keep backlight off
    digitalWrite(PIN_TFT_VDD_CTL, LOW);
    digitalWrite(PIN_TFT_RST, HIGH);
    
    // Re-initialize the display
    display.init();
    applyRotation();
    display.displayOn();
    delay(20);

    // Now turn on the backlight
  #ifdef PIN_TFT_LEDA_CTL_ACTIVE
    digitalWrite(PIN_TFT_LEDA_CTL, PIN_TFT_LEDA_CTL_ACTIVE);
  #else
    digitalWrite(PIN_TFT_LEDA_CTL, LOW);
  #endif    
    _isOn = true;
  }
}

void ST7789Display::turnOff() {
  digitalWrite(PIN_TFT_VDD_CTL, HIGH);
#ifdef PIN_TFT_LEDA_CTL_ACTIVE
  digitalWrite(PIN_TFT_LEDA_CTL, !PIN_TFT_LEDA_CTL_ACTIVE);
#else
  digitalWrite(PIN_TFT_LEDA_CTL, HIGH);
#endif
  digitalWrite(PIN_TFT_RST, LOW);
  _isOn = false;
}

void ST7789Display::clear() {
  display.clear();
}

void ST7789Display::startFrame(ColorVal bkg) {
  display.clear();  // TODO: use bkg

  // HiveFW:
  // cada frame começa sem faixa colorida.
  // Os ecrãs normais ativam-na explicitamente
  // através de setHeaderAccent().
  _headerAccentEnabled = false;
  _bootLogoAccentEnabled = false;

  setColor(UIColor::primary_txt);
  display.setFont(ArialMT_Plain_16);
}

void ST7789Display::setTextSize(int sz) {
  switch(sz) {
    case 1 :
      display.setFont(ArialMT_Plain_16);
      break;
    case 2 :
      display.setFont(ArialMT_Plain_24);
      break;
    default:
      display.setFont(ArialMT_Plain_16);
  }
}

void ST7789Display::setColor(ColorVal c) {
  _color = c;
  display.setColor((OLEDDISPLAY_COLOR)_color);
  display.setRGB(_color == OLEDDISPLAY_COLOR::WHITE ? ST77XX_WHITE : ST77XX_BLACK);
}


void ST7789Display::setHeaderAccent(
  uint8_t colorIndex
) {

  // Este frame pediu explicitamente a barra superior.
  _headerAccentEnabled = true;

  switch (colorIndex) {

    case 0:
      _headerAccent = ST77XX_RED;
      break;

    case 1:
      _headerAccent = ST77XX_GREEN;
      break;

    case 2:
      _headerAccent = ST77XX_BLUE;
      break;

    case 3:
      _headerAccent = ST77XX_CYAN;
      break;

    case 4:
      _headerAccent = ST77XX_MAGENTA;
      break;

    case 5:
      _headerAccent = ST77XX_YELLOW;
      break;

    case 6:
      _headerAccent = ST77XX_ORANGE;
      break;

    case 7:
      _headerAccent = ST77XX_WHITE;
      break;

    default:
      _headerAccent = ST77XX_RED;
      break;
  }
}




void ST7789Display::setBootLogoAccent(
  uint8_t logoColorIndex,
  uint8_t textColorIndex
) {

  auto colorFromIndex =
    [](uint8_t index) -> uint16_t {

      switch (index) {

        case 0:
          return ST77XX_RED;

        case 1:
          return ST77XX_GREEN;

        case 2:
          return ST77XX_BLUE;

        case 3:
          return ST77XX_CYAN;

        case 4:
          return ST77XX_MAGENTA;

        case 5:
          return ST77XX_YELLOW;

        case 6:
          return ST77XX_ORANGE;

        case 7:
        default:
          return ST77XX_WHITE;
      }
    };

  _bootLogoAccent =
    colorFromIndex(
      logoColorIndex
    );

  _bootTextAccent =
    colorFromIndex(
      textColorIndex
    );

  _bootLogoAccentEnabled = true;
}


void ST7789Display::setDisplayRotation(
  uint8_t rotation
) {

#ifdef HELTEC_T114_WITH_DISPLAY

  uint8_t normalized =
    rotation ? 1 : 0;

  if (_rotation == normalized) {
    return;
  }

  _rotation =
    normalized;

  if (_isOn) {
    applyRotation();
  }

#else

  (void)rotation;

#endif
}


void ST7789Display::setCursor(int x, int y) {
  _x = x*SCALE_X + X_OFFSET;
  _y = y*SCALE_Y + Y_OFFSET;
}

void ST7789Display::print(const char* str) {
  display.drawString(_x, _y, str);
}

void ST7789Display::printWordWrap(const char* str, int max_width) {
  display.drawStringMaxWidth(_x, _y, max_width*SCALE_X, str);
}

void ST7789Display::fillRect(int x, int y, int w, int h) {
  display.fillRect(x*SCALE_X + X_OFFSET, y*SCALE_Y + Y_OFFSET, w*SCALE_X, h*SCALE_Y);
}

void ST7789Display::drawRect(int x, int y, int w, int h) {
  display.drawRect(x*SCALE_X + X_OFFSET, y*SCALE_Y + Y_OFFSET, w*SCALE_X, h*SCALE_Y);
}

void ST7789Display::drawXbm(int x, int y, const uint8_t* bits, int w, int h) {
  // Calculate the base position in display coordinates
  uint16_t startX = x * SCALE_X + X_OFFSET;
  uint16_t startY = y * SCALE_Y + Y_OFFSET;
  
  // Width in bytes for bitmap processing
  uint16_t widthInBytes = (w + 7) / 8;
  
  // Process the bitmap row by row
  for (uint16_t by = 0; by < h; by++) {
    // Calculate the target y-coordinates for this logical row
    int y1 = startY + (int)(by * SCALE_Y);
    int y2 = startY + (int)((by + 1) * SCALE_Y);
    int block_h = y2 - y1;
    
    // Scan across the row bit by bit
    for (uint16_t bx = 0; bx < w; bx++) {
      // Calculate the target x-coordinates for this logical column
      int x1 = startX + (int)(bx * SCALE_X);
      int x2 = startX + (int)((bx + 1) * SCALE_X);
      int block_w = x2 - x1;
      
      // Get the current bit
      uint16_t byteOffset = (by * widthInBytes) + (bx / 8);
      uint8_t bitMask = 0x80 >> (bx & 7);
      bool bitSet = pgm_read_byte(bits + byteOffset) & bitMask;
      
      // If the bit is set, draw a block of pixels
      if (bitSet) {
        // Draw the block as a filled rectangle
        display.fillRect(x1, y1, block_w, block_h);
      }
    }
  }
}

uint16_t ST7789Display::getTextWidth(const char* str) {
  return display.getStringWidth(str) / SCALE_X;
}

void ST7789Display::endFrame() {

  // UI normal:
  // branco sobre preto.
  display.setRGB(
    ST77XX_WHITE
  );

#ifdef HELTEC_T114_WITH_DISPLAY

  // ----------------------------------------------------------
  // HiveFW T114
  //
  // A barra colorida tem 11 unidades de altura.
  // A unidade lógica seguinte é o separador branco.
  // Convertê-la para as linhas reais do framebuffer ST7789.
  //
  // A cor é aplicada PELO MESMO writer usado para o frame
  // completo. Não existe uma segunda passagem/rotação.
  // ----------------------------------------------------------

  int header_height =
    Y_OFFSET +
    (int)(
      11.0f *
      SCALE_Y
    );

  if (header_height < 1) {
    header_height = 1;
  }

  if (_headerAccentEnabled) {

    display.setTopBand(
      (uint16_t)header_height,
      _headerAccent
    );

  } else {

    // SplashScreen e qualquer outro frame sem barra:
    // não recolorir as linhas superiores.
    display.setTopBand(
      0,
      ST77XX_WHITE
    );
  }

#else

  display.setTopBand(
    0,
    ST77XX_WHITE
  );

#endif

  // ----------------------------------------------------------
  // HiveFW — BOOT LOGO
  //
  // O bitmap continua monocromático e inalterado.
  // Aqui apenas escolhemos a cor RGB dos pixels ATIVOS
  // pertencentes às duas zonas do bitmap:
  //
  //   LOGO  = x 0..15
  //   TEXTO = x 16..127
  //
  // O bitmap é desenhado em y=3 com altura 13.
  // ----------------------------------------------------------

  if (_bootLogoAccentEnabled) {

    const uint16_t boot_x1 =
      X_OFFSET;

    const uint16_t boot_split_x =
      X_OFFSET +
      (uint16_t)(
        16.0f *
        SCALE_X
      );

    const uint16_t boot_x2 =
      X_OFFSET +
      (uint16_t)(
        128.0f *
        SCALE_X
      );

    const uint16_t boot_y1 =
      Y_OFFSET +
      (uint16_t)(
        3.0f *
        SCALE_Y
      );

    const uint16_t boot_y2 =
      Y_OFFSET +
      (uint16_t)(
        16.0f *
        SCALE_Y
      );

    display.setBootLogoBands(
      boot_x1,
      boot_split_x,
      boot_x2,
      boot_y1,
      boot_y2,
      _bootLogoAccent,
      _bootTextAccent
    );

  } else {

    display.clearBootLogoBands();
  }

  display.display();
}

#endif