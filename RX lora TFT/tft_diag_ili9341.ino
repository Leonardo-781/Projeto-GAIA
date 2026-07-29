/*
  Diagnostico TFT ILI9341 com pinos explicitos (ESP32)
  Ligacao esperada:
  CS=5, DC=2, RST=4, MOSI=23, SCK=18, MISO=19 (opcional), LED=3V3
*/

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

static const int PIN_TFT_CS   = 5;
static const int PIN_TFT_DC   = 2;
static const int PIN_TFT_RST  = 4;
static const int PIN_TFT_MOSI = 23;
static const int PIN_TFT_SCLK = 18;
static const int PIN_TFT_MISO = 19;

Adafruit_ILI9341 tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

void drawScreen(const char* msg, uint16_t bg, uint16_t fg) {
  tft.fillScreen(bg);
  tft.setTextColor(fg);
  tft.setTextSize(2);
  tft.setCursor(12, 20);
  tft.println("GH2O TFT DIAG");
  tft.setCursor(12, 55);
  tft.println(msg);

  tft.drawRect(10, 90, 300, 140, ILI9341_WHITE);
  tft.fillCircle(70, 150, 20, ILI9341_RED);
  tft.fillCircle(160, 150, 20, ILI9341_GREEN);
  tft.fillCircle(250, 150, 20, ILI9341_BLUE);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("Iniciando diagnostico ILI9341...");

  SPI.begin(PIN_TFT_SCLK, PIN_TFT_MISO, PIN_TFT_MOSI, PIN_TFT_CS);
  delay(50);

  tft.begin();
  tft.setRotation(1);

  drawScreen("Teste de cores", ILI9341_BLACK, ILI9341_YELLOW);
  delay(1200);

  tft.invertDisplay(true);
  drawScreen("invertDisplay(true)", ILI9341_NAVY, ILI9341_WHITE);
  delay(1200);

  tft.invertDisplay(false);
  drawScreen("invertDisplay(false)", ILI9341_DARKGREEN, ILI9341_WHITE);
}

void loop() {
  static uint32_t last = 0;
  static uint32_t counter = 0;

  if (millis() - last >= 1000) {
    last = millis();
    counter++;

    tft.fillRect(12, 200, 290, 24, ILI9341_BLACK);
    tft.setTextColor(ILI9341_CYAN);
    tft.setTextSize(2);
    tft.setCursor(12, 205);
    tft.print("Contador: ");
    tft.print(counter);

    Serial.print("tick ");
    Serial.println(counter);
  }
}
