#ifndef TFT_UI_H
#define TFT_UI_H

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include "logo_montebot.h"

// Cores personalizadas
#define COR_FUNDO       ILI9341_BLACK
#define COR_TEXTO_LOG   ILI9341_WHITE
#define COR_HEADER_BG   ILI9341_WHITE
#define COR_DIVISOR     ILI9341_GREEN
#define COR_VERDE       ILI9341_GREEN
#define COR_AMARELO     ILI9341_YELLOW
#define COR_AZUL        ILI9341_BLUE
#define COR_SELECAO     ILI9341_DARKCYAN

extern Adafruit_ILI9341 tft;

// Estrutura para os logs
struct LogEntry {
  char timestamp[9]; // HH:MM:SS
  char project[16];
  char data[24];
  int httpStatus;
};

// Histórico de logs ampliado para suportar navegação (Scroll)
#define MAX_LOGS 15
extern LogEntry logs[MAX_LOGS];
extern int logCount;

// Opções do menu do Encoder
extern const char* menuOptions[];
extern const int NUM_MENU_OPTIONS;

// Protótipos das funções
void initUI();
void drawHeader(bool wifiConnected, const char* ssid, bool sdConnected, int lastHttpStatus);
void addLogEntry(const char* project, const char* data, int httpStatus);
void drawLogs(int scrollOffset = 0);
void drawMenu(int selectedOption);

#endif
