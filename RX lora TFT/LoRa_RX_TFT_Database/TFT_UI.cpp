#include "TFT_UI.h"
#include <time.h>

// Definir as variáveis globais
LogEntry logs[MAX_LOGS];
int logCount = 0;

const char* menuOptions[] = {
  "1. Voltar aos Logs",
  "2. Limpar Historico",
  "3. Configurar WiFi",
  "4. Enviar Ping Teste"
};
const int NUM_MENU_OPTIONS = 4;

void initUI() {
  tft.fillScreen(COR_FUNDO);
  
  // Desenhar Cabeçalho Base (WiFi desconectado, SD offline por padrao na inicializacao)
  drawHeader(false, "OFFLINE", false, 0);
  
  // Linha divisória (Verde)
  tft.fillRect(0, 45, 320, 3, COR_DIVISOR);
  
  // Limpar histórico interno
  logCount = 0;
  for (int i = 0; i < MAX_LOGS; i++) {
    memset(&logs[i], 0, sizeof(LogEntry));
  }
  
  drawLogs(0);
}

void drawHeader(bool wifiConnected, const char* ssid, bool sdConnected, int lastHttpStatus) {
  // Banner branco
  tft.fillRect(0, 0, 320, 45, COR_HEADER_BG);
  
  // Desenhar Logo MonteBot (50x34 px)
  tft.drawRGBBitmap(5, 5, (const uint16_t*)logo_montebot, LOGO_WIDTH, LOGO_HEIGHT);
  
  // Nome do Projeto "PROJETO GAIA"
  tft.setTextColor(ILI9341_BLACK);
  tft.setTextSize(2);
  tft.setCursor(60, 6);
  tft.print("PROJETO GAIA");
  
  // Subtítulo / Status WiFi e SD
  tft.setTextSize(1);
  tft.setCursor(60, 25);
  
  if (wifiConnected) {
    tft.setTextColor(ILI9341_DARKGREEN);
    tft.printf("W:%s", ssid);
  } else {
    tft.setTextColor(ILI9341_RED);
    tft.print("W:OFF");
  }
  
  tft.setTextColor(ILI9341_DARKGREY);
  tft.print(" | ");
  
  if (sdConnected) {
    tft.setTextColor(ILI9341_DARKGREEN);
    tft.print("SD:OK");
  } else {
    tft.setTextColor(ILI9341_RED);
    tft.print("SD:ERR");
  }
  
  // Painel de Status HTTP (Direita)
  tft.setTextColor(ILI9341_BLACK);
  tft.setTextSize(1);
  tft.setCursor(210, 6);
  tft.print("STATUS HTTP:");
  
  tft.setCursor(210, 18);
  if (lastHttpStatus == 0) {
    tft.setTextColor(ILI9341_DARKGREY);
    tft.setTextSize(2);
    tft.print("---");
  } else if (lastHttpStatus >= 200 && lastHttpStatus < 300) {
    tft.setTextColor(ILI9341_DARKGREEN);
    tft.setTextSize(2);
    tft.printf("%d", lastHttpStatus);
  } else {
    tft.setTextColor(ILI9341_RED);
    tft.setTextSize(2);
    tft.printf("%d", lastHttpStatus);
  }
}

void addLogEntry(const char* project, const char* data, int httpStatus) {
  // Desloca os logs anteriores para cima
  for (int i = 0; i < MAX_LOGS - 1; i++) {
    logs[i] = logs[i + 1];
  }
  
  // Obter hora atual (do NTP ou do Uptime)
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    strftime(logs[MAX_LOGS - 1].timestamp, sizeof(logs[MAX_LOGS - 1].timestamp), "%H:%M:%S", &timeinfo);
  } else {
    unsigned long sec = millis() / 1000;
    snprintf(logs[MAX_LOGS - 1].timestamp, sizeof(logs[MAX_LOGS - 1].timestamp), "%02lu:%02lu:%02lu", (sec / 3600) % 24, (sec / 60) % 60, sec % 60);
  }
  
  // Copiar strings truncando se necessário
  strncpy(logs[MAX_LOGS - 1].project, project, sizeof(logs[MAX_LOGS - 1].project) - 1);
  logs[MAX_LOGS - 1].project[sizeof(logs[MAX_LOGS - 1].project) - 1] = '\0';
  
  strncpy(logs[MAX_LOGS - 1].data, data, sizeof(logs[MAX_LOGS - 1].data) - 1);
  logs[MAX_LOGS - 1].data[sizeof(logs[MAX_LOGS - 1].data) - 1] = '\0';
  
  logs[MAX_LOGS - 1].httpStatus = httpStatus;
  
  if (logCount < MAX_LOGS) {
    logCount++;
  }
  
  // Redesenhar logs com scrollOffset = 0 (mostra sempre os mais recentes ao receber novos dados)
  drawLogs(0);
}

void drawLogs(int scrollOffset) {
  // Limpar a área dos logs (altura 190)
  tft.fillRect(0, 49, 320, 190, COR_FUNDO);
  
  tft.setTextSize(1);
  
  if (logCount == 0) {
    tft.setTextColor(ILI9341_DARKGREY);
    tft.setCursor(85, 130);
    tft.print("Aguardando dados LoRa...");
    return;
  }
  
  // Calcular limites de scroll. Queremos ver até 7 linhas por vez.
  int maxScroll = (logCount > 7) ? (logCount - 7) : 0;
  if (scrollOffset < 0) scrollOffset = 0;
  if (scrollOffset > maxScroll) scrollOffset = maxScroll;
  
  // startIdx indica o primeiro log da janela de exibição
  int startIdx = (MAX_LOGS - logCount) + scrollOffset;
  int endIdx = startIdx + 7;
  if (endIdx > MAX_LOGS) endIdx = MAX_LOGS;
  
  int line = 0;
  for (int i = startIdx; i < endIdx; i++) {
    int y = 56 + line * 25;
    
    // 1. Timestamp (Amarelo)
    tft.setTextColor(COR_AMARELO);
    tft.setCursor(5, y);
    tft.printf("[%s]", logs[i].timestamp);
    
    // 2. Projeto (Branco)
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(65, y);
    tft.printf("%s:", logs[i].project);
    
    // 3. Dado / Mensagem (Ciano)
    tft.setTextColor(ILI9341_CYAN);
    tft.setCursor(140, y);
    tft.printf("%s", logs[i].data);
    
    // 4. Status HTTP (Verde se OK, Vermelho se erro, Cinza se nulo)
    tft.setCursor(265, y);
    if (logs[i].httpStatus == 0) {
      tft.setTextColor(ILI9341_DARKGREY);
      tft.print("HTTP ---");
    } else if (logs[i].httpStatus >= 200 && logs[i].httpStatus < 300) {
      tft.setTextColor(ILI9341_GREEN);
      tft.printf("HTTP %d", logs[i].httpStatus);
    } else {
      tft.setTextColor(ILI9341_RED);
      tft.printf("HTTP %d", logs[i].httpStatus);
    }
    
    line++;
  }
  
  // Indicador visual de rolagem (Setas)
  // Se houver mais logs antigos "para trás" (ou seja, scrollOffset > 0)
  if (scrollOffset > 0) {
    tft.fillTriangle(312, 54, 308, 59, 316, 59, COR_AMARELO);
  }
  // Se houver logs mais recentes "para frente" (ou seja, scrollOffset < maxScroll)
  if (scrollOffset < maxScroll) {
    tft.fillTriangle(312, 230, 308, 225, 316, 225, COR_AMARELO);
  }
}

void drawMenu(int selectedOption) {
  // Limpar a área dos logs para exibir o menu
  tft.fillRect(0, 49, 320, 190, COR_FUNDO);
  
  // Título do Menu
  tft.setTextColor(COR_DIVISOR);
  tft.setTextSize(2);
  tft.setCursor(15, 60);
  tft.print("MENU DE SELECAO");
  
  tft.fillRect(15, 78, 290, 2, ILI9341_DARKGREY);
  
  // Desenhar opções
  tft.setTextSize(1);
  for (int i = 0; i < NUM_MENU_OPTIONS; i++) {
    int y = 96 + i * 30;
    
    if (i == selectedOption) {
      // Destaque para a opção selecionada
      tft.fillRoundRect(15, y - 5, 290, 22, 4, COR_SELECAO);
      tft.setTextColor(ILI9341_WHITE);
      tft.setCursor(25, y + 2);
      tft.printf("> %s", menuOptions[i]);
    } else {
      // Opção normal
      tft.setTextColor(ILI9341_LIGHTGREY);
      tft.setCursor(25, y + 2);
      tft.printf("  %s", menuOptions[i]);
    }
  }
}
