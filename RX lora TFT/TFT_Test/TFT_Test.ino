/**
 * ============================================================================
 * PROJETO: Receptor Universal de Dados (ESP32 + TFT SPI 240x320 + LoRa E220)
 * ETAPA 1: Teste e Validação do Funcionamento do Display TFT SPI 240x320 v1.2
 * ============================================================================
 * 
 * Conexões Físicas (ESP32 Dev Module <-> TFT SPI 240x320):
 * ----------------------------------------------------------------------------
 *  TFT Pin      | ESP32 GPIO | Descrição
 *  -------------|------------|------------------------------------------------
 *  VCC          | 5V ou 3.3V | Alimentação (Se o display tiver regulador U1, use 5V)
 *  GND          | GND        | Terra Comum
 *  CS           | GPIO 5     | VSPI Chip Select (Seleção do Dispositivo SPI)
 *  RESET (RST)  | GPIO 4     | Controle de Reset Físico
 *  DC (RS)      | GPIO 2     | Data/Command (Seleção de Dados ou Comandos)
 *  SDI (MOSI)   | GPIO 23    | VSPI MOSI (Linha de dados SPI)
 *  SCK (SCLK)   | GPIO 18    | VSPI SCK (Relógio SPI)
 *  LED (BL)     | 3.3V       | Backlight (Luz de fundo, ligado direto ou via resistor)
 *  SDO (MISO)   | GPIO 19    | Opcional (MISO, necessário se for usar Toque/Touch)
 * ----------------------------------------------------------------------------
 * 
 * NOTA SOBRE CONTROLADORES (Drivers):
 * A maioria das telas de 240x320 v1.2 usam o driver ILI9341. Algumas mais recentes
 * usam o ST7789. Este código foi estruturado de forma que você possa alternar
 * facilmente entre eles comentando/descomentando as seções abaixo.
 * 
 * BIBLIOTECAS REQUERIDAS (Instalar via Gerenciador de Bibliotecas no Arduino IDE):
 * 1. "Adafruit GFX Library" (Mecanismo gráfico base)
 * 2. "Adafruit ILI9341" (Se seu display for ILI9341 - Padrão)
 * 3. "Adafruit ST7735 and ST7789 Library" (Se seu display for ST7789)
 */

#include <SPI.h>
#include <Adafruit_GFX.h>

// ============================================================================
// CONFIGURAÇÃO DO DRIVER - DESCOMENTE APENAS UM DOS DOIS ABAIXO
// ============================================================================
#define USE_ILI9341      // Descomente esta linha para o driver ILI9341 (Padrão)
// #define USE_ST7789    // Descomente esta linha para o driver ST7789

#ifdef USE_ILI9341
  #include <Adafruit_ILI9341.h>
  #define TFT_CS   5
  #define TFT_DC   2
  #define TFT_RST  4
  Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
  #define DRIVER_NAME "ILI9341"
#elif defined(USE_ST7789)
  #include <Adafruit_ST7789.h>
  #define TFT_CS   5
  #define TFT_DC   2
  #define TFT_RST  4
  Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
  #define DRIVER_NAME "ST7789"
#else
  #error "Defina um dos drivers (USE_ILI9341 ou USE_ST7789) no topo do codigo!"
#endif
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("--- Teste do Display TFT SPI 240x320 ---");

  // Inicialização do display baseada no Driver escolhido
  #ifdef USE_ILI9341
    Serial.println("Inicializando com driver ILI9341...");
    tft.begin();
  #elif defined(USE_ST7789)
    Serial.println("Inicializando com driver ST7789...");
    tft.init(240, 320); // Inicializa ST7789 no modo 240x320
  #endif

  // Define a rotação da tela (0 a 3)
  // 1 ou 3 deixa o display em modo paisagem (Landscape)
  // 0 ou 2 deixa em modo retrato (Portrait)
  tft.setRotation(1); 

  Serial.println("Display inicializado com sucesso!");
}

void loop() {
  // 1. Limpa a tela com cores sólidas diferentes
  testFillScreen();
  delay(1000);

  // 2. Escreve texto na tela com diferentes tamanhos e cores
  testText();
  delay(3000);

  // 3. Desenha formas geométricas básicas
  testShapes();
  delay(3000);

  // 4. Desenha linhas dinâmicas
  testLines();
  delay(2000);
}

// --- Funções de Teste ---

// Preenche a tela inteira com cores primárias
void testFillScreen() {
  Serial.println("Preenchendo tela com cores primarias...");
  
  tft.fillScreen(ILI9341_BLACK);
  delay(500);
  tft.fillScreen(ILI9341_RED);
  delay(500);
  tft.fillScreen(ILI9341_GREEN);
  delay(500);
  tft.fillScreen(ILI9341_BLUE);
  delay(500);
  tft.fillScreen(ILI9341_WHITE);
}

// Escreve textos de teste e informações na tela
void testText() {
  Serial.println("Escrevendo texto de teste...");
  tft.fillScreen(ILI9341_BLACK);
  
  // Cabeçalho
  tft.setCursor(10, 10);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextSize(2);
  tft.println("ESP32 Universal RX");
  
  tft.setCursor(10, 30);
  tft.setTextColor(ILI9341_GREEN);
  tft.setTextSize(1);
  tft.print("Driver: ");
  tft.println(DRIVER_NAME);

  // Linha divisória
  tft.drawFastHLine(10, 45, tft.width() - 20, ILI9341_WHITE);

  // Corpo de texto
  tft.setCursor(10, 60);
  tft.setTextColor(ILI9341_CYAN);
  tft.setTextSize(2);
  tft.println("Status: Aguardando...");

  tft.setCursor(10, 90);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(1);
  tft.println("Mapeamento de pinos TFT SPI:");
  tft.println("CS: GPIO 5  | RST: GPIO 4");
  tft.println("DC: GPIO 2  | MOSI: GPIO 23");
  tft.println("SCK: GPIO 18| LED: VCC/3.3V");

  tft.setCursor(10, 150);
  tft.setTextColor(ILI9341_MAGENTA);
  tft.setTextSize(2);
  tft.println("TFT SPI OK!");
  
  tft.setCursor(10, 180);
  tft.setTextColor(ILI9341_ORANGE);
  tft.setTextSize(1.5);
  tft.println("Proxima etapa: LoRa E220...");
}

// Desenha retângulos, círculos e triângulos na tela
void testShapes() {
  Serial.println("Desenhando formas geometricas...");
  tft.fillScreen(ILI9341_BLACK);

  // Retângulos vazios e preenchidos
  tft.drawRect(20, 20, 80, 50, ILI9341_RED);
  tft.fillRect(120, 20, 80, 50, ILI9341_BLUE);

  // Círculos vazios e preenchidos
  tft.drawCircle(60, 120, 30, ILI9341_GREEN);
  tft.fillCircle(160, 120, 30, ILI9341_YELLOW);

  // Triângulos
  tft.drawTriangle(60, 180, 20, 220, 100, 220, ILI9341_MAGENTA);
  tft.fillTriangle(160, 180, 120, 220, 200, 220, ILI9341_CYAN);
}

// Desenha linhas cruzando a tela
void testLines() {
  Serial.println("Desenhando linhas...");
  tft.fillScreen(ILI9341_BLACK);
  
  int w = tft.width();
  int h = tft.height();
  
  for (int i = 0; i < w; i += 20) {
    tft.drawLine(0, 0, i, h - 1, ILI9341_GREEN);
  }
  for (int i = 0; i < h; i += 20) {
    tft.drawLine(0, 0, w - 1, i, ILI9341_YELLOW);
  }
  
  delay(500);
}
