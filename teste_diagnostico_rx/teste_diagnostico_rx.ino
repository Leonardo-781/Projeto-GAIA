/**
 * ============================================================================
 * ARQUIVO: teste_diagnostico_rx.ino
 * VERSÃO: 1.0
 * DATA: 2026-08-01
 * DESCRIÇÃO: Código de teste e diagnóstico completo para a Central Receptora (RX).
 *            Testa o display TFT ILI9341, o slot de cartão SD, o codificador 
 *            rotativo (Encoder EC11) e a comunicação serial com o rádio LoRa E220.
 * COMUNICAÇÃO: ESP32 conectado na porta COM25.
 * ============================================================================
 */

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SD.h>

// Pinos do Display e SD
#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4
#define SD_CS    15

// Pinos do LoRa
#define LORA_RX  16  // RX2 do ESP32 (conectar no TX do LoRa)
#define LORA_TX  17  // TX2 do ESP32 (conectar no RX do LoRa)

// Pinos do Encoder
#define ENCODER_CLK 25
#define ENCODER_DT  26
#define ENCODER_SW  27

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
HardwareSerial LoraSerial(2);

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("\n==================================================");
  Serial.println("  INICIANDO DIAGNÓSTICO DA CENTRAL RECEPTORA (RX) ");
  Serial.println("==================================================");

  // 1. Configurando pinos do Encoder
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  Serial.println("[1] Pinos do Encoder configurados (GPIO 25, 26, 27).");

  // 2. Inicializando Serial do LoRa
  LoraSerial.begin(9600, SERIAL_8N1, LORA_RX, LORA_TX);
  Serial.println("[2] Porta Serial do LoRa inicializada a 9600 bps.");

  // 3. Inicializando Barramento SPI
  pinMode(TFT_CS, OUTPUT);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(SD_CS, HIGH);
  SPI.begin(18, 19, 23);
  Serial.println("[3] Barramento SPI inicializado (SCK=18, MISO=19, MOSI=23).");
  delay(100);

  // 4. Teste do Cartão SD (Antes do Display)
  Serial.println("[4] Testando Cartão SD...");
  bool sdOK = SD.begin(SD_CS, SPI, 4000000);
  if (sdOK) {
    Serial.println("   ➔ Cartão SD: OK!");
    uint8_t cardType = SD.cardType();
    Serial.print("   ➔ Tipo do cartão: ");
    if (cardType == CARD_MMC) Serial.println("MMC");
    else if (cardType == CARD_SD) Serial.println("SDSC");
    else if (cardType == CARD_SDHC) Serial.println("SDHC");
    else Serial.println("Desconhecido");
    Serial.printf("   ➔ Tamanho: %llu MB\n", SD.cardSize() / (1024 * 1024));
  } else {
    Serial.println("   ❌ Cartão SD: ERRO (Não detectado ou formatação errada).");
  }

  // 5. Teste do Display TFT ILI9341
  Serial.println("[5] Inicializando Display TFT...");
  tft.begin();
  tft.setRotation(1); // Modo Paisagem
  
  // Teste de cores rápido
  tft.fillScreen(ILI9341_RED);
  delay(200);
  tft.fillScreen(ILI9341_GREEN);
  delay(200);
  tft.fillScreen(ILI9341_BLUE);
  delay(200);
  
  tft.fillScreen(ILI9341_BLACK);
  tft.drawRect(0, 0, 320, 240, ILI9341_WHITE);
  
  tft.setTextColor(ILI9341_GREEN);
  tft.setTextSize(2);
  tft.setCursor(15, 20);
  tft.println("GAIA RX - DIAGNOSTICO");
  
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(1);
  tft.setCursor(15, 60);
  tft.print("Cartao SD: ");
  if (sdOK) {
    tft.setTextColor(ILI9341_GREEN);
    tft.println("OK");
  } else {
    tft.setTextColor(ILI9341_RED);
    tft.println("ERRO");
  }

  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(15, 80);
  tft.println("Gire o Encoder para testar...");
  tft.setCursor(15, 100);
  tft.println("Pressione o botao do encoder...");
  tft.setCursor(15, 130);
  tft.setTextColor(ILI9341_YELLOW);
  tft.println("Aguardando pacotes LoRa...");

  Serial.println("[5] Tela TFT desenhada. Verifique as cores na tela física.");

  // 6. Varredura I2C (Caso possua RTC ou sensores extras na central)
  Serial.println("[6] Varrendo Barramento I2C...");
  Wire.begin();
  int nDevices = 0;
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();
    if (error == 0) {
      Serial.printf("   ➔ Dispositivo I2C encontrado no endereco 0x%02X\n", address);
      nDevices++;
    }
  }
  if (nDevices == 0) {
    Serial.println("   ➔ Nenhum dispositivo I2C encontrado.");
  }

  Serial.println("\n==================================================");
  Serial.println(" DIAGNÓSTICO INICIAL CONCLUÍDO. MONITORANDO INPUTS ");
  Serial.println("==================================================");
}

int lastPos = -99;
int pos = 0;
bool lastSw = HIGH;

void loop() {
  // Leitura do estado físico dos pinos do encoder
  int clkVal = digitalRead(ENCODER_CLK);
  int dtVal = digitalRead(ENCODER_DT);
  bool swVal = digitalRead(ENCODER_SW);

  // Detecção simples de giro para diagnóstico
  static int lastClk = HIGH;
  if (clkVal != lastClk) {
    lastClk = clkVal;
    if (clkVal == LOW) {
      if (dtVal == HIGH) {
        pos++;
      } else {
        pos--;
      }
    }
  }

  if (pos != lastPos) {
    lastPos = pos;
    Serial.printf("[INPUT] Encoder girou. Posição: %d\n", pos);
    
    tft.fillRect(15, 160, 290, 20, ILI9341_BLACK);
    tft.setCursor(15, 160);
    tft.setTextColor(ILI9341_CYAN);
    tft.printf("Encoder Pos: %d", pos);
  }

  if (swVal != lastSw) {
    lastSw = swVal;
    Serial.printf("[INPUT] Botão do Encoder: %s\n", swVal == LOW ? "PRESSIONADO" : "SOLTO");
    
    tft.fillRect(15, 190, 290, 20, ILI9341_BLACK);
    tft.setCursor(15, 190);
    if (swVal == LOW) {
      tft.setTextColor(ILI9341_RED);
      tft.println("Botao: PRESSIONADO");
    } else {
      tft.setTextColor(ILI9341_GREEN);
      tft.println("Botao: SOLTO");
    }
  }

  // Escuta dados recebidos do LoRa (Serial2) e replica no monitor serial
  while (LoraSerial.available()) {
    char c = LoraSerial.read();
    Serial.print(c);
  }
  
  delay(10);
}
