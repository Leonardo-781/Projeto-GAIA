/*
  Teste TFT isolado para upload via Arduino CLI
  ESP32 + TFT_eSPI
*/

#include <SPI.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

const uint16_t myGreen  = 0x04C0;
const uint16_t myYellow = 0xFFE0;
const uint16_t myBlue   = 0x001F;

uint32_t tStart = 0;

void drawBaseScreen() {
  tft.fillScreen(TFT_BLACK);

  tft.fillRect(0, 0, 320, 64, TFT_WHITE);
  tft.fillRect(0, 64, 320, 4, myGreen);
  tft.fillRect(0, 200, 320, 2, myGreen);
  tft.fillRect(0, 156, 200, 2, myGreen);
  tft.fillRect(200, 64, 2, 186, myGreen);

  tft.setTextColor(myBlue, TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(18, 12);
  tft.println("HORIMETRO");
  tft.setCursor(18, 38);
  tft.println("TESTE TFT");

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(2, 70);
  tft.println("FUNCIONAMENTO");
  tft.setCursor(2, 160);
  tft.println("STAND BY");
  tft.setCursor(2, 205);
  tft.println("DATA/HORA");
  tft.setCursor(203, 205);
  tft.println("STATUS TFT");
}

void updateDynamicArea() {
  uint32_t sec = (millis() - tStart) / 1000;

  char line[24];
  snprintf(line, sizeof(line), "17/07/2026 12:%02lu", sec % 60);

  tft.fillRect(0, 210, 195, 26, TFT_BLACK);
  tft.setTextColor(myBlue, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(2, 214);
  tft.print(line);

  tft.fillRect(0, 82, 196, 60, TFT_BLACK);
  tft.setTextColor((sec % 2 == 0) ? myYellow : TFT_WHITE, TFT_BLACK);
  tft.setTextSize(3);
  tft.setCursor(4, 90);
  tft.printf("%lus", sec);

  tft.setTextSize(2);
  tft.setCursor(4, 122);
  tft.print("render OK");

  tft.fillRect(0, 172, 196, 22, TFT_BLACK);
  tft.setTextColor((sec % 2 == 0) ? TFT_WHITE : myYellow, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(4, 174);
  tft.printf("ciclo %lu", sec);

  tft.fillRect(202, 210, 116, 26, TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(206, 214);
  tft.print((sec % 2 == 0) ? "RUN" : "STBY");
}

void setup() {
  tft.init();
  tft.invertDisplay(false);
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  drawBaseScreen();
  tStart = millis();
}

void loop() {
  updateDynamicArea();
  delay(1000);
}
