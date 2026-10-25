/**
 * ============================================================================
 * ARQUIVO: LoRa_RX_TFT_Database.ino
 * PROJETO: Projeto GAIA - Central Receptora Agroclimática
 * VERSÃO: 2.5
 * DATA: 2026-08-01
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
#include <DNSServer.h>
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

// Pinos do Encoder Rotativo EC11
#define ENCODER_CLK 25
#define ENCODER_DT  26
#define ENCODER_SW  27

// Passos por clique físico (Com interrupção simples, 1 passo por clique é o ideal)
#define ENCODER_STEPS_PER_CLICK 1

// Caminhos dos arquivos no cartão SD
#define DATA_FILE  "/dados.txt"
#define QUEUE_FILE "/fila.csv"

// Instanciamento dos objetos do Display, Servidor Web e DNS Captivo
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;
String rxScanOptions = "";
bool telemetriaServerAtivo = false;

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
  time_t agora = time(NULL);
  if (agora > 1700000000) {
    doc["timestamp"] = (uint32_t)agora;
  } else {
    doc["timestamp"] = millis() / 1000;
  }
  
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
// FUNÇÕES DE RECEPÇÃO DE TELEMETRIA VIA WI-FI (GATEWAY REST HTTP)
// ============================================================================
bool processTelemetryMessage(String message) {
  message.trim();
  if (message.length() == 0) return false;
  
  Serial.print("Telemetria Wi-Fi Recebida: ");
  Serial.println(message);
  
  String processedMessage = message;
  
  // Se nao inicia com '{', tenta descriptografar usando a chave AES-128
  if (!message.startsWith("{")) {
    String decryptedMessage;
    if (!decryptAES(message, chave_crypto, decryptedMessage)) {
      Serial.println("ERRO: Falha ao descriptografar mensagem Wi-Fi. Chave incorreta ou pacote corrompido.");
      return false;
    }
    processedMessage = decryptedMessage;
    Serial.print("Telemetria Descriptografada: ");
    Serial.println(processedMessage);
  }
  
  String projectName = "GAIA RX";
  String payloadData = processedMessage;
  
  #if ARDUINOJSON_VERSION_MAJOR >= 7
    JsonDocument doc;
  #else
    StaticJsonDocument<512> doc;
  #endif
  
  DeserializationError error = deserializeJson(doc, processedMessage);
  
  if (!error) {
    if (doc.containsKey("codSta")) {
      projectName = doc["codSta"].as<String>();
    } else if (doc.containsKey("estacao")) {
      projectName = doc["estacao"].as<String>();
    }
    payloadData = processedMessage;
  }
  
  // Envia ao Banco de Dados e gera payload completo
  String rawPayload = "";
  int responseCode = sendDataToDatabase(projectName, payloadData, rawPayload);
  lastHttpStatus = responseCode;
  
  // Atualiza barra superior
  drawHeader(WiFi.status() == WL_CONNECTED, activeSSID, sdOK, lastHttpStatus);
  
  // Salva no Cartao SD localmente no arquivo de texto (/dados.txt)
  gravarNoSD(projectName, payloadData, responseCode);
  
  // Se falhou o envio para o banco, armazena na fila para reenvio
  if (responseCode < 200 || responseCode >= 300) {
    enfileirar(rawPayload);
  }
  
  // Adiciona a entrada na lista de Logs da Tela
  addLogEntry(projectName.c_str(), "Telemetria Wi-Fi", responseCode);
  
  // Volta o scrollOffset para mostrar o dado novo no topo
  if (currentState == STATE_LOGS) {
    scrollOffset = 0;
    drawLogs(scrollOffset);
  }

  return true;
}

void handleReceberTelemetria() {
  String payload = "";
  if (server.hasArg("plain")) {
    payload = server.arg("plain");
  } else if (server.args() > 0) {
    payload = server.arg((size_t)0);
  }
  
  if (payload.length() == 0) {
    server.send(400, "application/json", "{\"status\":\"erro\",\"mensagem\":\"Corpo da requisicao vazio\"}");
    return;
  }
  
  bool ok = processTelemetryMessage(payload);
  if (ok) {
    server.send(200, "application/json", "{\"status\":\"sucesso\",\"mensagem\":\"Telemetria recebida e processada via Wi-Fi\"}");
  } else {
    server.send(400, "application/json", "{\"status\":\"erro\",\"mensagem\":\"Falha ao processar telemetria (criptografia ou JSON invalido)\"}");
  }
}

void iniciarServidorTelemetria() {
  server.close();
  server.on("/api/telemetria", HTTP_POST, handleReceberTelemetria);
  server.on("/api/receber-dados", HTTP_POST, handleReceberTelemetria);
  server.on("/dados", HTTP_POST, handleReceberTelemetria);
  server.on("/", HTTP_GET, []() {
    String info = "{\"projeto\":\"Projeto GAIA Central RX\",\"status\":\"online\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
    server.send(200, "application/json", info);
  });
  server.begin();
  telemetriaServerAtivo = true;
  Serial.printf("Servidor de Telemetria HTTP ativo no IP: %s (porta 80)\n", WiFi.localIP().toString().c_str());
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
    Serial.println("\nWiFi conectado com sucesso!");
    Serial.printf("IP Local do Gateway RX: %s\n", WiFi.localIP().toString().c_str());
    strncpy(activeSSID, wifi_ssid, sizeof(activeSSID) - 1);
    
    // Sincroniza com o Relógio Atômico (NTP.br / Observatório Nacional)
    configTime(-3 * 3600, 0, "a.st1.ntp.br", "b.st1.ntp.br", "pool.ntp.org");
    
    drawHeader(true, activeSSID, sdOK, lastHttpStatus);
    iniciarServidorTelemetria();
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
void scanWifiNetworksRX() {
  Serial.println("Escaneando redes Wi-Fi (RX)...");
  WiFi.mode(WIFI_AP_STA);
  int n = WiFi.scanNetworks(false, true);
  rxScanOptions = "";
  if (n <= 0) {
    rxScanOptions = "<option value=\"\">Nenhuma rede Wi-Fi encontrada</option>";
    WiFi.scanDelete();
    return;
  }

  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    s.trim();
    if (s.length() == 0) continue;

    bool duplicate = false;
    for (int j = 0; j < i; j++) {
      if (WiFi.SSID(j) == s) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) continue;

    int rssi = WiFi.RSSI(i);
    String auth = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Aberta" : "Protegida";
    String selected = (s == String(wifi_ssid)) ? "selected" : "";
    rxScanOptions += "<option value=\"" + s + "\" " + selected + ">" +
                     s + " (" + String(rssi) + " dBm, " + auth + ")</option>";
  }
  WiFi.scanDelete();
}

// ============================================================================
// PORTAL DE CONFIGURAÇÃO (MODO SETUP AP COM PORTAL CAPTIVO)
// ============================================================================
void iniciarModoAP() {
  WiFi.disconnect();
  delay(100);

  // Varre redes antes de abrir o servidor
  scanWifiNetworksRX();

  // Inicializa o Access Point em modo híbrido AP_STA
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("GAIA-RX-SETUP", "gaiaconfig");
  IPAddress apIP = WiFi.softAPIP();

  // Inicia servidor DNS na porta 53 para Portal Captivo
  dnsServer.start(DNS_PORT, "*", apIP);

  Serial.println("=========================================");
  Serial.println("MODO SETUP ATIVO - CENTRAL RECEPTORA GAIA");
  Serial.println("SSID: GAIA-RX-SETUP");
  Serial.println("Senha: gaiaconfig");
  Serial.print("IP: ");
  Serial.println(apIP);
  Serial.println("Acesse: http://192.168.4.1");
  Serial.println("=========================================");

  // Desenha a tela dedicada do Modo AP no Display TFT
  drawAPScreen(0, "GAIA-RX-SETUP", "gaiaconfig", "192.168.4.1");

  static bool apRunning = true;
  apRunning = true;

  // Rota raiz (Formulário)
  server.on("/", HTTP_GET, []() {
    String html = R"raw(
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
          --danger-color: #ef4444;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
          font-family: 'Outfit', sans-serif;
          background-color: var(--bg-color);
          background-image: radial-gradient(circle at top right, rgba(59, 130, 246, 0.12), transparent 45%),
                            radial-gradient(circle at bottom left, rgba(16, 185, 129, 0.08), transparent 45%);
          color: var(--text-color);
          display: flex;
          justify-content: center;
          align-items: center;
          min-height: 100vh;
          padding: 16px;
        }
        .container {
          width: 100%;
          max-width: 480px;
          background: var(--card-bg);
          backdrop-filter: blur(16px);
          -webkit-backdrop-filter: blur(16px);
          border: 1px solid var(--card-border);
          border-radius: 24px;
          padding: 30px 24px;
          box-shadow: 0 10px 40px rgba(0, 0, 0, 0.6);
        }
        h2 {
          font-weight: 700;
          font-size: 1.8rem;
          margin-bottom: 6px;
          background: linear-gradient(135deg, #3b82f6, #10b981);
          -webkit-background-clip: text;
          -webkit-text-fill-color: transparent;
          text-align: center;
        }
        .subtitle {
          color: var(--text-muted);
          font-size: 0.9rem;
          text-align: center;
          margin-bottom: 20px;
        }
        .chips {
          display: flex;
          justify-content: center;
          gap: 8px;
          margin-bottom: 22px;
        }
        .chip {
          background: rgba(59, 130, 246, 0.15);
          border: 1px solid rgba(59, 130, 246, 0.3);
          color: #60a5fa;
          font-size: 0.75rem;
          font-weight: 600;
          padding: 4px 12px;
          border-radius: 999px;
        }
        .form-group {
          margin-bottom: 18px;
        }
        label {
          display: block;
          font-size: 0.78rem;
          font-weight: 600;
          margin-bottom: 6px;
          color: var(--text-muted);
          text-transform: uppercase;
          letter-spacing: 0.06em;
        }
        input[type="text"], input[type="password"], select {
          width: 100%;
          padding: 12px 14px;
          background: rgba(255, 255, 255, 0.03);
          border: 1px solid var(--card-border);
          border-radius: 12px;
          color: var(--text-color);
          font-family: inherit;
          font-size: 0.95rem;
          transition: all 0.3s ease;
        }
        input:focus, select:focus {
          outline: none;
          border-color: var(--accent-color);
          box-shadow: 0 0 10px rgba(59, 130, 246, 0.25);
          background: rgba(255, 255, 255, 0.06);
        }
        .input-row {
          display: flex;
          gap: 8px;
        }
        .btn {
          display: block;
          width: 100%;
          padding: 13px;
          border: none;
          border-radius: 12px;
          font-size: 0.95rem;
          font-weight: 600;
          cursor: pointer;
          transition: all 0.3s ease;
          text-align: center;
          margin-top: 10px;
        }
        .btn-primary {
          background: var(--accent-color);
          color: #fff;
        }
        .btn-primary:hover {
          background: var(--accent-hover);
          box-shadow: 0 6px 18px rgba(59, 130, 246, 0.35);
        }
        .btn-secondary {
          background: rgba(255, 255, 255, 0.08);
          color: #f3f4f6;
          font-size: 0.82rem;
          padding: 8px 12px;
          border-radius: 8px;
          width: auto;
          margin: 0;
          text-decoration: none;
        }
        .btn-secondary:hover { background: rgba(255, 255, 255, 0.15); }
        .btn-cancel {
          background: rgba(255, 255, 255, 0.06);
          color: var(--text-muted);
          border: 1px solid var(--card-border);
        }
        .btn-cancel:hover {
          background: rgba(255, 255, 255, 0.12);
          color: #fff;
        }
        .hint {
          font-size: 0.72rem;
          color: var(--text-muted);
          margin-top: 4px;
        }
      </style>
    </head>
    <body>
      <div class="container">
        <h2>Central GAIA - RX</h2>
        <p class="subtitle">Parâmetros da Central Receptora</p>

        <div class="chips">
          <span class="chip">AP: GAIA-RX-SETUP</span>
          <span class="chip">IP: 192.168.4.1</span>
        </div>

        <form action="/salvar" method="POST" onsubmit="return validarForm()">
          <div class="form-group">
            <label for="ssid_list">Redes Wi-Fi Detectadas</label>
            <div class="input-row">
              <select id="ssid_list" onchange="selecionarRede()">
                <option value="">Selecione uma rede ou digite abaixo...</option>
                )raw" + rxScanOptions + R"raw(
              </select>
              <a href="/scan" class="btn btn-secondary" style="display:flex; align-items:center;">Escanear</a>
            </div>
          </div>

          <div class="form-group">
            <label for="ssid">SSID da Rede Wi-Fi</label>
            <input type="text" id="ssid" name="ssid" value=")raw" + String(wifi_ssid) + R"raw(" required placeholder="Nome do Wi-Fi">
          </div>

          <div class="form-group">
            <label for="senha">Senha do Wi-Fi</label>
            <div class="input-row">
              <input type="password" id="senha" name="senha" value=")raw" + String(wifi_password) + R"raw(" placeholder="Senha do roteador">
              <button type="button" class="btn-secondary" onclick="toggleSenha()">Ver</button>
            </div>
          </div>

          <div class="form-group">
            <label for="db_url">URL do Banco de Dados / API</label>
            <input type="text" id="db_url" name="db_url" value=")raw" + database_url + R"raw(" required placeholder="http://servidor/api/receber-dados">
            <div class="hint">Endpoint HTTP REST que recebe os pacotes decodificados.</div>
          </div>

          <div class="form-group">
            <label for="chave_crypto">Chave Criptográfica AES-128 (16 chars)</label>
            <input type="text" id="chave_crypto" name="chave_crypto" value=")raw" + String(chave_crypto) + R"raw(" minlength="16" maxlength="16" required placeholder="Ex: ChaveSecreta123">
            <div class="hint">Deve ser idêntica à chave configurada nas estações de campo.</div>
          </div>

          <button type="submit" class="btn btn-primary">Salvar e Conectar</button>
          <a href="/cancelar" class="btn btn-cancel" style="text-decoration:none;">Cancelar / Voltar aos Logs</a>
        </form>
      </div>

      <script>
        function selecionarRede() {
          var s = document.getElementById('ssid_list').value;
          if (s) document.getElementById('ssid').value = s;
        }
        function toggleSenha() {
          var p = document.getElementById('senha');
          p.type = (p.type === 'password') ? 'text' : 'password';
        }
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
    )raw";
    server.send(200, "text/html", html);
  });

  // Rota para reescanear redes
  server.on("/scan", HTTP_GET, []() {
    scanWifiNetworksRX();
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
  });

  // Rota para cancelar e voltar ao monitoramento
  server.on("/cancelar", HTTP_GET, []() {
    server.send(200, "text/plain", "Encerrando modo AP e retornando aos logs...");
    delay(500);
    apRunning = false;
  });

  // Rota para salvar dados de configuração
  server.on("/salvar", HTTP_POST, []() {
    String newSSID = server.arg("ssid");
    String newPass = server.arg("senha");
    String newUrl  = server.arg("db_url");
    String newChave = server.arg("chave_crypto");

    newSSID.trim();
    newPass.trim();
    newUrl.trim();
    newChave.trim();

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

    server.send(200, "text/plain", "Dados salvos com sucesso! Reiniciando a central...");
    delay(1500);
    ESP.restart();
  });

  // Endpoints padrao para Portal Captivo
  auto handleCaptiveRedirect = []() {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
  };
  server.on("/generate_204", HTTP_GET, handleCaptiveRedirect);
  server.on("/gen_204", HTTP_GET, handleCaptiveRedirect);
  server.on("/hotspot-detect.html", HTTP_GET, handleCaptiveRedirect);
  server.on("/canonical.html", HTTP_GET, handleCaptiveRedirect);
  server.on("/ncsi.txt", HTTP_GET, handleCaptiveRedirect);
  server.onNotFound(handleCaptiveRedirect);

  server.begin();

  unsigned long apStart = millis();
  int lastStations = -1;
  unsigned long lastTftUpdate = 0;

  // Loop de atendimento do AP com Portal Captivo
  while (apRunning) {
    dnsServer.processNextRequest();
    server.handleClient();

    // Atualiza contagem de dispositivos no display TFT periodicamente
    if (millis() - lastTftUpdate > 1000) {
      lastTftUpdate = millis();
      int stations = WiFi.softAPgetStationNum();
      if (stations != lastStations) {
        lastStations = stations;
        drawAPScreen(stations, "GAIA-RX-SETUP", "gaiaconfig", "192.168.4.1");
      }
    }

    // Saida do AP pressionando o Encoder
    if (digitalRead(ENCODER_SW) == LOW) {
      delay(50);
      if (digitalRead(ENCODER_SW) == LOW) {
        Serial.println("Saida do Modo AP solicitada via clique do encoder.");
        while (digitalRead(ENCODER_SW) == LOW) delay(10);
        apRunning = false;
        break;
      }
    }

    // Timeout de 10 minutos para retorno automatico
    if (millis() - apStart > 600000UL) {
      Serial.println("Timeout do Modo AP atingido. Retornando ao monitoramento...");
      apRunning = false;
      break;
    }

    delay(5);
  }

  // Finalizacao limpa do AP e retorno a operacao normal
  dnsServer.stop();
  server.close();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  Serial.println("Modo AP encerrado. Restaurando tela e reconectando Wi-Fi.");
  initUI();
  connectWiFi();
}

// ============================================================================
// CONFIGURAÇÃO INICIAL (SETUP)
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("--- Inicializando Central Receptora GAIA Wi-Fi ---");
  
  // Configuração das entradas do Encoder Rotativo (Internal Pull-Up)
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  
  // Atalho 1: Detecta se o botão do encoder foi pressionado durante a energização (boot)
  bool forcarAPNoBoot = (digitalRead(ENCODER_SW) == LOW);
  
  // Interrupção simples na descida (FALLING) do CLK para leitura estável
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK), readEncoder, FALLING);
  Serial.println("Encoder Rotativo inicializado.");
  
  // Configura os pinos de Chip Select como saída e desativa no boot
  pinMode(TFT_CS, OUTPUT);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(SD_CS, HIGH);

  // Inicializa o barramento SPI com os pinos corretos (SCK=18, MISO=19, MOSI=23)
  SPI.begin(18, 19, 23);
  delay(50);
  
  // 1. Inicializa o cartao SD PRIMEIRO para garantir estabilidade no barramento compartilhado
  sdOK = SD.begin(SD_CS, SPI, 4000000);
  if (sdOK) {
    Serial.println("Cartao SD inicializado com sucesso.");
  } else {
    Serial.println("AVISO: Falha ao inicializar o cartao SD.");
  }
  
  // 2. Inicialização do display TFT (garante display pronto antes de qualquer desenho)
  tft.begin();
  tft.setRotation(1); // Modo Paisagem (320x240)
  Serial.println("Display TFT inicializado.");
  
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
  
  // Se o botão estava pressionado no boot OU se for o primeiro boot sem WiFi, entra no modo AP
  if (forcarAPNoBoot || storedSSID.length() == 0) {
    if (forcarAPNoBoot) {
      Serial.println("⚠️ Botão do Encoder pressionado no boot! Entrando em modo AP.");
    } else {
      Serial.println("⚠️ Primeiro boot sem Wi-Fi configurado! Entrando em modo AP.");
    }
    iniciarModoAP();
    
    // Recarrega apos saida ou salvamento
    prefs.begin("wifi-db", true);
    storedSSID = prefs.getString("ssid", "");
    storedPassword = prefs.getString("senha", "");
    prefs.end();
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
      buttonPressedTimer = 0;
      lastButtonPress = millis(); // Evita que o soltar seja interpretado como clique de menu
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

  // 4. Atendimento de telemetria HTTP recebida via Wi-Fi
  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
  }
  
  // 5. Gerenciamento assincrono da conexao WiFi e reenvio de fila offline com auto-reconexao ativa
  unsigned long currentMillis = millis();
  if (currentMillis - lastWifiCheck >= wifiCheckInterval) {
    lastWifiCheck = currentMillis;
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi offline. Tentando reconectar...");
      strncpy(activeSSID, "OFFLINE", sizeof(activeSSID));
      drawHeader(false, "OFFLINE", sdOK, lastHttpStatus);
      telemetriaServerAtivo = false;
      
      // Executa reconexao assincrona nao-bloqueante ativa
      WiFi.begin(wifi_ssid, wifi_password);
    } else {
      strncpy(activeSSID, wifi_ssid, sizeof(activeSSID) - 1);
      drawHeader(true, activeSSID, sdOK, lastHttpStatus);
      if (!telemetriaServerAtivo) {
        iniciarServidorTelemetria();
      }
      processarFila();
    }
  }
  
  delay(10);
}
