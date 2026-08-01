/**
 * ============================================================================
 * ARQUIVO: LoRa_RX_TFT_Database.ino
 * PROJETO: Projeto GAIA - Central Receptora Agroclimática
 * VERSÃO: 2.5
 * DATA: 2026-08-01
 * AUTOR: Antigravity AI
 * MODIFICAÇÕES:
 * 1. Implementação de criptografia simétrica AES-128-CBC via hardware (MbedTLS)
 *    para recepção segura de dados via rádio LoRa.
 * 2. Portal Web AP reformulado com CSS responsivo e premium, listagem dinâmica
 *    de redes Wi-Fi e chave de criptografia configurável em Preferences.
 * 3. Gestão robusta da fila off-line (/fila.csv) linha a linha para evitar estouros
 *    de memória (Heap) e duplicações.
 * 4. Validação e descarte de pacotes que falharem na descriptografia, prevenindo
 *    ataques de injeção de dados.
 * 5. Adicionado rotina de reconexão de rede ativa não-bloqueante no loop do receptor.
 * 6. Adicionado script de validação JavaScript no portal AP para garantir chave de 16 caracteres.
 * 7. Alterado o salvamento local de dados do receptor para utilizar o arquivo "/dados.txt"
 *    em vez de ".csv", harmonizando com o transmissor.
 * 8. Simplificado e otimizado o algoritmo do Encoder Rotativo (EC11) para usar 
 *    interrupção simples por descida (FALLING) com debounce de 40ms por software. 
 *    Isso resolve o travamento e comportamento errático em encoders comuns (como o KY-040).
 * 9. Implementado atalho duplo para forçar o Modo Portal AP:
 *    - Segurando o botão do encoder (GPIO 27) durante a inicialização (boot).
 *    - Segurando o botão do encoder por 5 segundos contínuos durante a operação.
 * ============================================================================
 * 
 * PROJETO: Central Receptora de Dados (ESP32 + TFT ILI9341 + LoRa E220)
 * ETAPA: Receptor LoRa + Salvar no Cartão SD + Configuração WiFi via Portal Web (AP)
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <WebServer.h>
#include <Preferences.h>

#include "TFT_UI.h"

// Inclusão de criptografia nativa mbedtls do ESP32
#include "mbedtls/aes.h"
#include "mbedtls/base64.h"

// ============================================================================
// CONFIGURAÇÕES DE HARDWARE (PINOS)
// ============================================================================
#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4
#define SD_CS    15  // Chip Select do leitor de Cartão SD

#define LORA_RX  16  // RX2 do ESP32 (conectar no TX do LoRa)
#define LORA_TX  17  // TX2 do ESP32 (conectar no RX do LoRa)

// Pinos do Encoder Rotativo EC11
#define ENCODER_CLK 25
#define ENCODER_DT  26
#define ENCODER_SW  27

// Passos por clique físico (Com interrupção simples, 1 passo por clique é o ideal)
#define ENCODER_STEPS_PER_CLICK 1

// Caminhos dos arquivos no cartão SD
#define DATA_FILE  "/dados.txt"
#define QUEUE_FILE "/fila.csv"

// Instanciamento dos objetos do Display e Servidor Web
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
WebServer server(80);

// Configuração da Porta Serial do LoRa (Serial2)
HardwareSerial LoraSerial(2);

// ============================================================================
// VARIÁVEIS GLOBAIS DE CONFIGURAÇÃO E ESTADO
// ============================================================================
bool sdOK = false;

char wifi_ssid[64] = "";
char wifi_password[64] = "";
char chave_crypto[17] = "MARIACRYPTOKEY12"; // Chave padrão
String database_url = "http://sua-api-aqui.com/api/receber-dados"; 

// Controle do Encoder e Menu
volatile int encoderPos = 0;
volatile unsigned long lastEncoderInterrupt = 0;
int lastEncoderPos = 0;
unsigned long lastButtonPress = 0;
unsigned long buttonPressedTimer = 0; // Timer para atalho de 5 segundos

enum UIState {
  STATE_LOGS, 
  STATE_MENU  
};

UIState currentState = STATE_LOGS;
int scrollOffset = 0;
int menuSelection = 0;

// Variáveis de controle de rede
unsigned long lastWifiCheck = 0;
const unsigned long wifiCheckInterval = 10000; // Verifica WiFi a cada 10s
int lastHttpStatus = 0;
char activeSSID[32] = "OFFLINE";

// Buffer para leitura do LoRa
String loraBuffer = "";

// ============================================================================
// INTERRUPÇÃO SIMPLES DO ENCODER (DEBOUNCE POR SOFTWARE - ESTÁVEL E FLUIDO)
// ============================================================================
void IRAM_ATTR readEncoder() {
  unsigned long now = millis();
  // Ignora ruídos de bouncing mecânico ocorridos em menos de 40ms
  if (now - lastEncoderInterrupt < 40) {
    return;
  }
  
  int dtVal = digitalRead(ENCODER_DT);
  if (dtVal == HIGH) {
    encoderPos++;
  } else {
    encoderPos--;
  }
  
  lastEncoderInterrupt = now;
}

// ============================================================================
// FUNÇÃO UTILITÁRIA DE DESCRIPTOGRAFIA AES-128-CBC
// ============================================================================
bool decryptAES(const String &cipherTextB64, const char *key, String &plainText) {
  size_t decodedLen = 0;
  size_t cipherTextB64Len = cipherTextB64.length();
  size_t maxDecLen = (cipherTextB64Len * 3) / 4 + 2;
  uint8_t *decoded = (uint8_t *)malloc(maxDecLen);
  if (!decoded) return false;

  int ret = mbedtls_base64_decode(decoded, maxDecLen, &decodedLen, (const unsigned char *)cipherTextB64.c_str(), cipherTextB64Len);
  if (ret != 0 || decodedLen < 32) { // IV (16) + bloco de dados min (16)
    free(decoded);
    return false;
  }

  uint8_t iv[16];
  memcpy(iv, decoded, 16);

  int encryptedLen = decodedLen - 16;
  uint8_t *encrypted = decoded + 16;

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_dec(&aes, (const unsigned char *)key, 128);

  uint8_t *decrypted = (uint8_t *)malloc(encryptedLen);
  if (!decrypted) {
    mbedtls_aes_free(&aes);
    free(decoded);
    return false;
  }

  ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, encryptedLen, iv, encrypted, decrypted);
  mbedtls_aes_free(&aes);
  free(decoded);

  if (ret != 0) {
    free(decrypted);
    return false;
  }

  // Remove preenchimento PKCS7
  int padLen = decrypted[encryptedLen - 1];
  if (padLen < 1 || padLen > 16 || padLen > encryptedLen) {
    free(decrypted);
    return false; 
  }

  int plainLen = encryptedLen - padLen;
  char *plainStr = (char *)malloc(plainLen + 1);
  if (!plainStr) {
    free(decrypted);
    return false;
  }
  memcpy(plainStr, decrypted, plainLen);
  plainStr[plainLen] = '\0';
  plainText = String(plainStr);

  free(decrypted);
  free(plainStr);
  return true;
}

// ============================================================================
// FUNÇÕES DO CARTÃO SD (SALVAMENTO E FILA LOCAL)
// ============================================================================
void gravarNoSD(String project, String data, int status) {
  if (!sdOK) return;
  File dataFile = SD.open(DATA_FILE, FILE_APPEND);
  if (dataFile) {
    struct tm timeinfo;
    char timeStr[24] = "00/00/0000 00:00:00";
    if (getLocalTime(&timeinfo)) {
      strftime(timeStr, sizeof(timeStr), "%d/%m/%Y %H:%M:%S", &timeinfo);
    } else {
      snprintf(timeStr, sizeof(timeStr), "Uptime:%lu", millis() / 1000);
    }
    
    dataFile.printf("[%s] Proj: %s | Dado: %s | HTTP: %d\n", timeStr, project.c_str(), data.c_str(), status);
    dataFile.close();
    Serial.println("Dado salvo localmente em " DATA_FILE);
  } else {
    Serial.println("Erro ao abrir arquivo para gravar no SD.");
  }
}

void enfileirar(String payload) {
  if (!sdOK) return;
  File f = SD.open(QUEUE_FILE, FILE_APPEND);
  if (f) {
    f.println(payload);
    f.close();
    Serial.println("Pacote nao enviado. Enfileirado no SD em " QUEUE_FILE);
  }
}

// Processamento linha a linha seguro sem fragmentação de RAM
void processarFila() {
  if (!sdOK || !SD.exists(QUEUE_FILE)) return;
  if (WiFi.status() != WL_CONNECTED) return;
  
  File f = SD.open(QUEUE_FILE, FILE_READ);
  if (!f) return;
  
  String tempPath = "/q_tmp.csv";
  if (SD.exists(tempPath)) SD.remove(tempPath);
  
  File fTemp = SD.open(tempPath, FILE_WRITE);
  if (!fTemp) {
    f.close();
    return;
  }
  
  int reenviados = 0;
  int pendentes = 0;
  
  while (f.available()) {
    String payload = f.readStringUntil('\n');
    payload.trim();
    if (payload.length() == 0) continue;
    
    HTTPClient http;
    http.begin(database_url);
    http.addHeader("Content-Type", "application/json");
    
    int code = http.POST(payload);
    http.end();
    
    if (code >= 200 && code < 300) {
      reenviados++;
    } else {
      fTemp.println(payload);
      pendentes++;
    }
    delay(50); 
  }
  f.close();
  fTemp.close();
  
  SD.remove(QUEUE_FILE);
  if (pendentes > 0) {
    SD.rename(tempPath, QUEUE_FILE);
  } else {
    SD.remove(tempPath);
  }
  
  if (reenviados > 0) {
    Serial.printf("Fila de dados offline processada: %d pacotes enviados com sucesso.\n", reenviados);
  }
}

// ============================================================================
// FUNÇÃO: Enviar dados ao Banco de Dados via HTTP POST
// ============================================================================
int sendDataToDatabase(String project, String data, String &rawPayload) {
  #if ARDUINOJSON_VERSION_MAJOR >= 7
    JsonDocument doc;
  #else
    StaticJsonDocument<512> doc;
  #endif
  
  doc["project"] = project;
  doc["data"] = data;
  doc["timestamp"] = millis() / 1000;
  
  serializeJson(doc, rawPayload);

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi offline. Guardando na fila local.");
    return -1;
  }
  
  HTTPClient http;
  http.begin(database_url);
  http.addHeader("Content-Type", "application/json");
  
  Serial.print("Enviando HTTP POST: ");
  Serial.println(rawPayload);
  
  int httpResponseCode = http.POST(rawPayload);
  http.end();
  
  return httpResponseCode;
}

// ============================================================================
// FUNÇÃO: Conectar WiFi
// ============================================================================
void connectWiFi() {
  if (strlen(wifi_ssid) == 0) {
    Serial.println("Sem credenciais WiFi salvas.");
    strncpy(activeSSID, "OFFLINE", sizeof(activeSSID));
    drawHeader(false, "OFFLINE", sdOK, 0);
    return;
  }

  Serial.printf("Conectando ao WiFi: %s...\n", wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_password);
  
  drawHeader(false, "CONECTANDO...", sdOK, 0);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 15) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi conectado!");
    strncpy(activeSSID, wifi_ssid, sizeof(activeSSID) - 1);
    
    // Sincroniza o relógio (GMT-3 Brasília)
    configTime(-3 * 3600, 0, "a.ntp.br", "pool.ntp.org");
    
    drawHeader(true, activeSSID, sdOK, lastHttpStatus);
    processarFila();
  } else {
    Serial.println("\nFalha na conexao.");
    strncpy(activeSSID, "OFFLINE", sizeof(activeSSID));
    drawHeader(false, "OFFLINE", sdOK, 0);
  }
}

// ============================================================================
// PORTAL DE CONFIGURAÇÃO (MODO SETUP AP)
// ============================================================================
void iniciarModoAP() {
  WiFi.disconnect();
  delay(100);
  
  // Desenhar tela de aviso de Setup no display
  tft.fillScreen(COR_FUNDO);
  tft.fillRect(0, 0, 320, 45, COR_HEADER_BG);
  tft.setTextColor(ILI9341_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 15);
  tft.print("CONFIGURACAO WIFI");
  
  tft.fillRect(0, 45, 320, 3, COR_DIVISOR);
  
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 70);
  tft.print("O receptor entrou em modo de configuracao.");
  
  tft.setCursor(10, 100);
  tft.setTextSize(2);
  tft.setTextColor(COR_AMARELO);
  tft.print("SSID: GAIA-RX-SETUP");
  
  tft.setCursor(10, 130);
  tft.setTextColor(COR_AMARELO);
  tft.print("Senha: gaiaconfig");
  
  tft.setCursor(10, 170);
  tft.setTextColor(ILI9341_CYAN);
  tft.print("Acesse: 192.168.4.1");
  
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_LIGHTGREY);
  tft.setCursor(10, 210);
  tft.print("Use seu celular para conectar e configurar.");

  // Inicializa o Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP("GAIA-RX-SETUP", "gaiaconfig");
  
  Serial.println("=========================================");
  Serial.println("MODO SETUP ATIVO - PROJETO GAIA");
  Serial.println("SSID: GAIA-RX-SETUP");
  Serial.println("Senha: gaiaconfig");
  Serial.println("Acesse: http://192.168.4.1");
  Serial.println("=========================================");

  // Rota raiz (Formulário)
  server.on("/", HTTP_GET, []() {
    int n = WiFi.scanNetworks();
    String scanOptions = "";
    if (n <= 0) {
      scanOptions = "<option value=\"\">Nenhuma rede Wi-Fi encontrada</option>";
    } else {
      for (int i = 0; i < n; i++) {
        scanOptions += "<option value=\"" + WiFi.SSID(i) + "\">" + WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + " dBm)</option>";
      }
    }
    
    // Portal de setup premium unificado
    String html = R"(
    <!DOCTYPE html>
    <html lang="pt-BR">
    <head>
      <meta charset="utf-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>Configuração Central GAIA</title>
      <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600;700&display=swap" rel="stylesheet">
      <style>
        :root {
          --bg-color: #0b0f19;
          --card-bg: rgba(255, 255, 255, 0.05);
          --card-border: rgba(255, 255, 255, 0.08);
          --accent-color: #3b82f6;
          --accent-hover: #2563eb;
          --text-color: #f3f4f6;
          --text-muted: #9ca3af;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
          font-family: 'Outfit', sans-serif;
          background-color: var(--bg-color);
          background-image: radial-gradient(circle at top right, rgba(59, 130, 246, 0.08), transparent 45%),
                            radial-gradient(circle at bottom left, rgba(16, 185, 129, 0.05), transparent 45%);
          color: var(--text-color);
          display: flex;
          justify-content: center;
          align-items: center;
          min-height: 100vh;
          padding: 20px;
        }
        .container {
          width: 100%;
          max-width: 480px;
          background: var(--card-bg);
          backdrop-filter: blur(16px);
          -webkit-backdrop-filter: blur(16px);
          border: 1px solid var(--card-border);
          border-radius: 24px;
          padding: 35px;
          box-shadow: 0 10px 40px rgba(0, 0, 0, 0.6);
        }
        h2 {
          font-weight: 700;
          font-size: 1.8rem;
          margin-bottom: 8px;
          background: linear-gradient(135deg, #3b82f6, #10b981);
          -webkit-background-clip: text;
          -webkit-text-fill-color: transparent;
          text-align: center;
        }
        .subtitle {
          color: var(--text-muted);
          font-size: 0.9rem;
          text-align: center;
          margin-bottom: 30px;
        }
        .form-group {
          margin-bottom: 22px;
        }
        label {
          display: block;
          font-size: 0.8rem;
          font-weight: 600;
          margin-bottom: 8px;
          color: var(--text-muted);
          text-transform: uppercase;
          letter-spacing: 0.07em;
        }
        input[type="text"], input[type="password"], select {
          width: 100%;
          padding: 14px 18px;
          background: rgba(255, 255, 255, 0.03);
          border: 1px solid var(--card-border);
          border-radius: 12px;
          color: var(--text-color);
          font-family: inherit;
          font-size: 1rem;
          transition: all 0.3s ease;
        }
        input:focus, select:focus {
          outline: none;
          border-color: var(--accent-color);
          box-shadow: 0 0 12px rgba(59, 130, 246, 0.25);
          background: rgba(255, 255, 255, 0.07);
        }
        .btn {
          display: block;
          width: 100%;
          padding: 15px;
          border: none;
          border-radius: 12px;
          font-size: 1rem;
          font-weight: 600;
          cursor: pointer;
          transition: all 0.3s ease;
          text-align: center;
        }
        .btn-primary {
          background: var(--accent-color);
          color: #fff;
        }
        .btn-primary:hover {
          background: var(--accent-hover);
          box-shadow: 0 6px 20px rgba(59, 130, 246, 0.35);
        }
      </style>
    </head>
    <body>
      <div class="container">
        <h2>Setup Central GAIA</h2>
        <p class="subtitle">Gerencie os parâmetros da central receptora</p>
        
        <form action="/salvar" method="POST" onsubmit="return validarForm()">
          <div class="form-group">
            <label for="ssid">Selecione a Rede WiFi</label>
            <select id="ssid" name="ssid">
              )" + scanOptions + R"(
            </select>
          </div>
          
          <div class="form-group">
            <label for="senha">Senha do WiFi</label>
            <input type="password" id="senha" name="senha" placeholder="Senha da Rede">
          </div>
          
          <div class="form-group">
            <label for="db_url">URL do Banco de Dados (API)</label>
            <input type="text" id="db_url" name="db_url" value=")" + database_url + R"(">
          </div>
          
          <div class="form-group">
            <label for="chave_crypto">Chave Criptográfica LoRa (16 chars)</label>
            <input type="text" id="chave_crypto" name="chave_crypto" value=")" + String(chave_crypto) + R"(" minlength="16" maxlength="16" required placeholder="Ex: ChaveSecreta123">
          </div>
          
          <button type="submit" class="btn btn-primary">Salvar e Reiniciar</button>
        </form>
      </div>
      
      <script>
        function validarForm() {
          var key = document.getElementById('chave_crypto').value;
          if (key.length !== 16) {
            alert('Erro: A chave criptográfica deve ter exatamente 16 caracteres!');
            return false;
          }
          return true;
        }
      </script>
    </body>
    </html>
    )";
    server.send(200, "text/html", html);
  });

  // Rota para processar os dados recebidos
  server.on("/salvar", HTTP_POST, []() {
    String newSSID = server.arg("ssid");
    String newPass = server.arg("senha");
    String newUrl  = server.arg("db_url");
    String newChave = server.arg("chave_crypto");
    
    // Preenche com '0' se menor que 16
    if (newChave.length() < 16) {
      while (newChave.length() < 16) newChave += "0";
    }
    
    Preferences prefs;
    prefs.begin("wifi-db", false);
    prefs.putString("ssid", newSSID);
    prefs.putString("senha", newPass);
    prefs.putString("db_url", newUrl);
    prefs.putString("chave_crypto", newChave);
    prefs.end();
    
    server.send(200, "text/plain", "Dados salvos! Reiniciando a central...");
    delay(2000);
    ESP.restart();
  });

  server.begin();
  
  // Trava no loop de atendimento do Setup
  while (true) {
    server.handleClient();
    delay(10);
  }
}

// ============================================================================
// FUNÇÃO: Processar Mensagem Recebida via LoRa
// ============================================================================
void processLoraMessage(String message) {
  message.trim();
  if (message.length() == 0) return;
  
  Serial.print("LoRa Recebido (Encrypted): ");
  Serial.println(message);
  
  // Tenta descriptografar usando a chave de segurança
  String decryptedMessage;
  if (!decryptAES(message, chave_crypto, decryptedMessage)) {
    Serial.println("ERRO: Falha ao descriptografar mensagem LoRa. Chave incorreta ou pacote corrompido.");
    return;
  }
  
  Serial.print("LoRa Descriptografado: ");
  Serial.println(decryptedMessage);
  
  String projectName = "GAIA RX";
  String payloadData = decryptedMessage;
  
  #if ARDUINOJSON_VERSION_MAJOR >= 7
    JsonDocument doc;
  #else
    StaticJsonDocument<512> doc;
  #endif
  
  DeserializationError error = deserializeJson(doc, decryptedMessage);
  
  if (!error) {
    if (doc.containsKey("codSta")) {
      projectName = doc["codSta"].as<String>();
    }
    payloadData = decryptedMessage;
  }
  
  // Envia ao Banco de Dados e gera payload completo
  String rawPayload = "";
  int responseCode = sendDataToDatabase(projectName, payloadData, rawPayload);
  lastHttpStatus = responseCode;
  
  // Atualiza barra superior
  drawHeader(WiFi.status() == WL_CONNECTED, activeSSID, sdOK, lastHttpStatus);
  
  // Salva no Cartão SD localmente no arquivo de texto (/dados.txt)
  gravarNoSD(projectName, payloadData, responseCode);
  
  // Se falhou o envio para o banco, armazena na fila para reenvio
  if (responseCode < 200 || responseCode >= 300) {
    enfileirar(rawPayload);
  }
  
  // Adiciona a entrada na lista de Logs da Tela
  addLogEntry(projectName.c_str(), "Telemetria OK", responseCode);
  
  // Volta o scrollOffset para mostrar o dado novo no topo
  if (currentState == STATE_LOGS) {
    scrollOffset = 0;
    drawLogs(scrollOffset);
  }
}

// ============================================================================
// CONFIGURAÇÃO INICIAL (SETUP)
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("--- Inicializando Central Receptora GAIA LoRa ---");
  
  // Configuração das entradas do Encoder Rotativo (Internal Pull-Up)
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  
  // Atalho 1: Se o botão do encoder for pressionado no boot, força modo AP
  if (digitalRead(ENCODER_SW) == LOW) {
    Serial.println("⚠️ Botão do Encoder pressionado no boot! Forçando modo AP.");
    iniciarModoAP();
  }
  
  // Interrupção simples na descida (FALLING) do CLK para leitura estável
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK), readEncoder, FALLING);
  Serial.println("Encoder Rotativo inicializado.");
  
  // Inicializa o módulo LoRa na Serial2 a 9600 bps (Padrão do E220)
  LoraSerial.begin(9600, SERIAL_8N1, LORA_RX, LORA_TX);
  Serial.println("Porta Serial2 do LoRa inicializada.");
  
  // Configura os pinos de Chip Select como saída e desativa no boot
  pinMode(TFT_CS, OUTPUT);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(SD_CS, HIGH);

  // Inicializa o barramento SPI com os pinos corretos (SCK=18, MISO=19, MOSI=23)
  SPI.begin(18, 19, 23, TFT_CS);
  delay(50);
  
  // Inicialização do display TFT
  tft.begin();
  tft.setRotation(1); // Modo Paisagem (320x240)
  Serial.println("Display TFT inicializado.");
  
  // Inicialização do cartao SD (SPI compartilhado)
  sdOK = SD.begin(SD_CS);
  if (sdOK) {
    Serial.println("Cartao SD inicializado com sucesso.");
  } else {
    Serial.println("AVISO: Falha ao inicializar o cartao SD.");
  }
  
  // Inicializa a Interface Gráfica da Tela
  initUI();
  
  // Carrega configurações salvas na memória interna (Preferences)
  Preferences prefs;
  prefs.begin("wifi-db", true); // Leitura
  String storedSSID = prefs.getString("ssid", "");
  String storedPassword = prefs.getString("senha", "");
  String storedDBUrl = prefs.getString("db_url", "");
  String storedChave = prefs.getString("chave_crypto", "MARIACRYPTOKEY12");
  prefs.end();
  
  if (storedDBUrl.length() > 0) {
    database_url = storedDBUrl;
  }
  
  // Prepara chave criptográfica de 16 caracteres
  if (storedChave.length() < 16) {
    while (storedChave.length() < 16) storedChave += "0";
  }
  strncpy(chave_crypto, storedChave.c_str(), 16);
  chave_crypto[16] = '\0';
  
  // Se for a primeira vez rodando (sem WiFi configurado), inicia portal AP
  if (storedSSID.length() == 0) {
    iniciarModoAP();
  }
  
  // Converte as credenciais para uso na biblioteca
  strncpy(wifi_ssid, storedSSID.c_str(), sizeof(wifi_ssid) - 1);
  strncpy(wifi_password, storedPassword.c_str(), sizeof(wifi_password) - 1);
  
  // Inicia conexão WiFi
  connectWiFi();
}

// ============================================================================
// LOOP PRINCIPAL
// ============================================================================
void loop() {
  // 1. Atalho 2: Se segurar o botão do encoder por 5 segundos contínuos, força AP de setup
  if (digitalRead(ENCODER_SW) == LOW) {
    if (buttonPressedTimer == 0) {
      buttonPressedTimer = millis();
    } else if (millis() - buttonPressedTimer > 5000) {
      Serial.println("⚠️ Botão pressionado por 5s! Forçando modo AP...");
      iniciarModoAP();
    }
  } else {
    buttonPressedTimer = 0;
  }

  // 2. Processar cliques rápidos comuns no botão do Encoder (Enter / Menu)
  if (digitalRead(ENCODER_SW) == LOW) {
    if (millis() - lastButtonPress > 250) { 
      lastButtonPress = millis();
      
      if (currentState == STATE_LOGS) {
        currentState = STATE_MENU;
        menuSelection = 0;
        drawMenu(menuSelection);
      } else {
        // Executa a opção selecionada
        if (menuSelection == 0) {
          // 1. Voltar aos Logs
          currentState = STATE_LOGS;
          scrollOffset = 0;
          drawLogs(scrollOffset);
        } 
        else if (menuSelection == 1) {
          // 2. Limpar Histórico de logs
          logCount = 0;
          currentState = STATE_LOGS;
          scrollOffset = 0;
          drawLogs(scrollOffset);
        } 
        else if (menuSelection == 2) {
          // 3. Configurar WiFi (Modo AP Setup)
          iniciarModoAP();
        } 
        else if (menuSelection == 3) {
          // 4. Enviar mensagem de teste (Ping)
          String dummyPayload = "";
          int pingCode = sendDataToDatabase("GAIA_Test", "Ping!", dummyPayload);
          lastHttpStatus = pingCode;
          
          gravarNoSD("GAIA_Test", "Ping!", pingCode);
          if (pingCode < 200 || pingCode >= 300) {
            enfileirar(dummyPayload);
          }
          
          addLogEntry("GAIA_Test", "Ping enviado!", pingCode);
          drawHeader(WiFi.status() == WL_CONNECTED, activeSSID, sdOK, lastHttpStatus);
          
          currentState = STATE_LOGS;
          scrollOffset = 0;
          drawLogs(scrollOffset);
        }
      }
    }
  }

  // 3. Processar rotação física do Encoder (Giro estável de 1 passo por clique)
  int change = encoderPos - lastEncoderPos;
  if (abs(change) >= ENCODER_STEPS_PER_CLICK) {
    int diff = (change > 0) ? 1 : -1;
    lastEncoderPos += diff * ENCODER_STEPS_PER_CLICK;
    
    if (currentState == STATE_LOGS) {
      int maxScroll = (logCount > 7) ? (logCount - 7) : 0;
      scrollOffset += diff;
      if (scrollOffset < 0) scrollOffset = 0;
      if (scrollOffset > maxScroll) scrollOffset = maxScroll;
      
      drawLogs(scrollOffset);
    } else {
      menuSelection += diff;
      if (menuSelection < 0) menuSelection = NUM_MENU_OPTIONS - 1;
      if (menuSelection >= NUM_MENU_OPTIONS) menuSelection = 0;
      
      drawMenu(menuSelection);
    }
  }

  // 4. Escuta de mensagens via LoRa (Serial2)
  while (LoraSerial.available()) {
    char c = LoraSerial.read();
    if (c == '\n' || c == '\r') {
      if (loraBuffer.length() > 0) {
        processLoraMessage(loraBuffer);
        loraBuffer = "";
      }
    } else {
      if (loraBuffer.length() < 256) { 
        loraBuffer += c;
      }
    }
  }
  
  // 5. Gerenciamento assíncrono da conexão WiFi e reenvio de fila offline com auto-reconexão ativa
  unsigned long currentMillis = millis();
  if (currentMillis - lastWifiCheck >= wifiCheckInterval) {
    lastWifiCheck = currentMillis;
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi offline. Tentando reconectar...");
      strncpy(activeSSID, "OFFLINE", sizeof(activeSSID));
      drawHeader(false, "OFFLINE", sdOK, lastHttpStatus);
      
      // Executa reconexão assíncrona não-bloqueante ativa
      WiFi.begin(wifi_ssid, wifi_password);
    } else {
      strncpy(activeSSID, wifi_ssid, sizeof(activeSSID) - 1);
      drawHeader(true, activeSSID, sdOK, lastHttpStatus);
      processarFila();
    }
  }
  
  delay(10);
}
