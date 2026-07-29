/*
  Teste TFT baseado no projeto do horimetro_v330
  - Mesmas bibliotecas e estilo visual
  - Sem modem, sem SD e sem RTC
  - Apenas validacao do display e renderizacao
*/

#include "SPI.h"
#include "TFT_eSPI.h"
#include "logo.h"
#include "Free_Fonts.h"

TFT_eSPI tft = TFT_eSPI();

const uint16_t myGreen  = 0x04C0;
const uint16_t myRed    = 0xF800;
const uint16_t myYellow = 0xFFE0;
const uint16_t myBlue   = 0x001F;
const uint16_t myOrange = 0xFCC0;

uint32_t tStart = 0;
uint32_t ciclos = 0;

void splashScreen() {
  tft.init();
  tft.invertDisplay(false);
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(true);
  tft.pushImage(0, 0, myWidth, myHeight, logo);
}

void showStatus(String s) {
  tft.fillRect(202, 210, 118, 29, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setFreeFont(FSSB12);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(s, 205, 219, GFXFF);
}

void drawBaseScreen() {
  tft.fillScreen(TFT_BLACK);

  tft.fillRect(0, 0, 320, 64, TFT_WHITE);
  tft.fillRect(0, 64, 320, 4, myGreen);
  tft.fillRect(0, 200, 320, 2, myGreen);
  tft.fillRect(0, 156, 200, 2, myGreen);
  tft.fillRect(200, 64, 2, 186, myGreen);

  tft.setTextDatum(TL_DATUM);
  tft.setFreeFont(FSSB12);
  tft.setTextColor(myBlue, TFT_WHITE);
  tft.drawString("HORIMETRO", 20, 10, GFXFF);
  tft.drawString("TESTE TFT", 20, 40, GFXFF);

  tft.setFreeFont(TT1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("FUNCIONAMENTO", 1, 68, GFXFF);
  tft.drawString("STAND BY", 1, 160, GFXFF);
  tft.drawString("DATA/HORA", 1, 202, GFXFF);
  tft.drawString("STATUS TFT", 202, 202, GFXFF);
  tft.drawString("CONTROLE", 202, 68, GFXFF);
}

void drawDynamicArea() {
  uint32_t segundos = (millis() - tStart) / 1000;
  uint16_t corTempo = (segundos % 2 == 0) ? myYellow : TFT_WHITE;
  uint16_t corStandBy = (segundos % 2 == 0) ? TFT_WHITE : myYellow;

  char dataHora[20] = "";
  snprintf(dataHora, sizeof(dataHora), "17/07/2026 12:%02lu", segundos % 60);

  tft.fillRect(0, 210, 195, 29, TFT_BLACK);
  tft.setFreeFont(FSSB12);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(myBlue, TFT_BLACK);
  tft.drawString(String(dataHora), 1, 219, GFXFF);

  tft.fillRect(0, 76, 197, 70, TFT_BLACK);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(corTempo, TFT_BLACK);
  tft.setFreeFont(FMB24);
  tft.drawString(String(segundos), 196, 95, GFXFF);
  tft.setFreeFont(FMB12);
  tft.drawString("s de funcionamento", 196, 124, GFXFF);

  tft.fillRect(0, 168, 195, 30, TFT_BLACK);
  tft.setTextColor(corStandBy, TFT_BLACK);
  tft.setFreeFont(FMB12);
  tft.drawString(String(ciclos) + " ciclos de teste", 196, 170, GFXFF);

  tft.fillRect(202, 76, 118, 120, TFT_BLACK);
  tft.setTextDatum(TR_DATUM);
  tft.setFreeFont(FM9);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("00 " + String(segundos), 320, 76, GFXFF);
  tft.drawString("01 " + String(segundos + 1), 320, 107, GFXFF);
  tft.drawString("10 " + String(segundos + 2), 320, 138, GFXFF);
  tft.drawString("11 " + String(segundos + 3), 320, 169, GFXFF);

  if (segundos % 2 == 0) {
    showStatus("RUN");
  } else {
    showStatus("STBY");
  }
}

void setup() {
  splashScreen();
  delay(1200);

  drawBaseScreen();
  showStatus("initOK");

  tStart = millis();
}

void loop() {
  drawDynamicArea();
  ciclos++;
  delay(1000);
}
