/**
 * ============================================================================
 * ARQUIVO: TX Telemetria.c
 * PROJETO: Projeto GAIA - Estação Agroclimática de Campo
 * VERSÃO: 2.5
 * DATA: 2026-07-29
 * MODIFICAÇÕES:
 * 1. Implementação de criptografia simétrica AES-128-CBC via hardware (MbedTLS).
 * 2. Portal Web AP reformulado com CSS responsivo e premium, listagem dinâmica
 *    de redes Wi-Fi e sincronização automática do relógio RTC do ESP32 via navegador.
 * 3. Gestão robusta da fila off-line (/fila.csv) linha a linha para evitar estouros
 *    de memória (Heap) e duplicações.
 * 4. Logs permanentes de envio e erros (/logs.txt) no cartão SD.
 * 5. Sincronização automática do relógio RTC DS3231 físico ao conectar ao NTP (WiFi).
 * 6. Melhoria na inicialização dos sensores (validação real do DHT22).
 * 7. Timeout no barramento I2C para prevenir travamentos físicos.
 * 8. Otimização no uso de Strings dinâmicas para evitar fragmentação de memória.
 * 9. Implementação de Deep Sleep inteligente alinhada ao relógio (RTC) e offset de dispositivo.
 * 10. Indicação visual por código de piscadas no LED interno (GPIO 2).
 * 11. Validação JavaScript no portal web para chaves de exatamente 16 caracteres.
 * 12. Adicionado cálculos agronômicos: Ponto de Orvalho, VPD e Evapotranspiração (ET0).
 * 13. Alterado o salvamento local de dados para salvar no arquivo "/dados.txt" 
 *     no formato de linhas JSON (JSON Lines).
 * 14. Implementado o suporte e leitura do Sensor de Temperatura do Solo DS18B20 
 *     (à prova d'água) no pino GPIO 13 via protocolo 1-Wire.
 * 15. Renomeação e aplicação da identidade oficial do projeto: "Projeto GAIA".
 * ============================================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <time.h>
#include <esp_task_wdt.h>
#include <math.h>

#include <Adafruit_BMP280.h>
#include <Adafruit_VEML7700.h>
#include <DHT.h>
#include <RTClib.h>
#include <ArduinoJson.h>

#include "LoRa_E220.h"

// Inclusão de bibliotecas para o sensor DS18B20 (1-Wire)
#include <OneWire.h>
#include <DallasTemperature.h>

// Inclusão de criptografia nativa mbedtls do ESP32
#include "mbedtls/aes.h"
#include "mbedtls/base64.h"

struct Leituras {
  float tBmp = NAN;
  float pressao = NAN;
  float lux = NAN;
  float umidade = NAN;
  float tDht = NAN;
  int   solo = -1;
  float tSolo = NAN; // Temperatura do solo (DS18B20)
  // Campos calculados
  float po = NAN;   // Ponto de Orvalho
  float vpd = NAN;  // Vapor Pressure Deficit
  float et0 = NAN;  // Evapotranspiração de referência
};

// pinos
#define SOLO_PIN        34
#define SD_CS           5
#define DHTPIN          14
#define DHTTYPE         DHT22
#define BTN_FORMAT_PIN  4    // segure 5s no boot ou em uso para formatar o SD
#define BTN_AP_PIN      15   // segure no boot para forcar modo AP de configuracao

#define LORA_RX_PIN     16   
#define LORA_TX_PIN     17   
#define LORA_M0_PIN     25
#define LORA_M1_PIN     26
#define LORA_AUX_PIN    27

#define LED_PIN         2    // LED interno do ESP32 para feedback visual

// Pino de dados do DS18B20
#define ONE_WIRE_BUS    13   

#define SERVER_URL "http://10.64.49.85/maria-web/"

#define CONFIG_FILE   "/ATMARIA.txt"
#define QUEUE_FILE    "/fila.csv"
#define LASTACK_FILE  "/lastack.txt"
#define DATA_FILE     "/dados.txt"
#define LOG_FILE      "/logs.txt"

// Wi-Fi fixo de emergencia, usado apenas se o arquivo de config sumir/corromper
#define FALLBACK_SSID  "WIFI_FIXO"
#define FALLBACK_PASS  "WIFI_FIXO"

Adafruit_BMP280 bmp;
Adafruit_VEML7700 veml;
DHT dht(DHTPIN, DHTTYPE);
RTC_DS3231 rtc;
WebServer server(80);
HTTPClient http;

// Setup da rede 1-Wire para o DS18B20
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18Sensors(&oneWire);

HardwareSerial loraSerial(2);
LoRa_E220 e220ttl(&loraSerial, LORA_AUX_PIN, LORA_M0_PIN, LORA_M1_PIN);

bool sdOK      = false;
bool bmpOK     = false;
bool vemlOK    = false;
bool dhtOK     = false;
bool rtcOK     = false;
bool wifiOK    = false;
bool loraOK    = false;
bool ds18OK    = false;

struct Config {
  String modo;       
  String ssid;
  String senha;
  int    intervaloMin;  // 5, 10 ou 15
  String estacao;       // ex: E99999
  String chaveCrypto;   // chave de 16 caracteres para criptografia
  bool   valida = false;
};
Config cfg;

long offsetSegundos = 0;
uint32_t lastAckTs = 0;
uint32_t softClockBase = 0;
unsigned long softClockMillisBase = 0;

// Protótipos das funções auxiliares
void formatarCartaoSD();
void verificarBotaoFormatar();
bool enviarHTTP(const String &payload, bool isEncrypted);
bool enviarPorRadio(const String &payload, bool isEncrypted);
void gravarLogPermanente(const String &nivel, const String &mensagem);

void piscarLED(int piscadas, int tempoLigado, int tempoDesligado) {
  for (int i = 0; i < piscadas; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(tempoLigado);
    digitalWrite(LED_PIN, LOW);
    if (i < piscadas - 1) delay(tempoDesligado);
  }
}

uint32_t obterTimestamp() {
  if (rtcOK) {
    return rtc.now().unixtime();
  }
  time_t agora;
  time(&agora);
  if (agora > 1600000000) { 
    return (uint32_t)agora;
  }
  return softClockBase + (millis() - softClockMillisBase) / 1000;
}

String timestampFormatado(uint32_t ts) {
  time_t t = ts;
  struct tm *tmInfo = localtime(&t);
  char buf[32];
  snprintf(buf, sizeof(buf), "%d/%d/%d %02d:%02d:%02d",
           tmInfo->tm_mday, tmInfo->tm_mon + 1, tmInfo->tm_year + 1900,
           tmInfo->tm_hour, tmInfo->tm_min, tmInfo->tm_sec);
  return String(buf);
}

bool nomeEstacaoValido(const String &nome) {
  if (nome.length() != 6) return false;
  if (!isAlpha(nome[0])) return false;
  for (int i = 1; i < 6; i++) if (!isDigit(nome[i])) return false;
  return true;
}

/**
 * Funções de cálculos físicos e agronômicos
 */
float calcularPontoOrvalho(float t, float rh) {
  if (isnan(t) || isnan(rh)) return NAN;
  if (rh < 1.0) rh = 1.0;
  if (rh > 100.0) rh = 100.0;
  float a = 17.27;
  float b = 237.7;
  float alpha = ((a * t) / (b + t)) + log(rh / 100.0);
  return (b * alpha) / (a - alpha);
}

float calcularVPD(float t, float rh) {
  if (isnan(t) || isnan(rh)) return NAN;
  float vpsat = 0.61078 * exp((17.27 * t) / (t + 237.3));
  float vpact = vpsat * (rh / 100.0);
  return vpsat - vpact; // kPa
}

float calcularET0(float t, float rh, float pressaoHpa, float lux, int intervaloMin) {
  if (isnan(t) || isnan(rh) || isnan(pressaoHpa) || isnan(lux)) return NAN;
  
  float vpsat = 0.61078 * exp((17.27 * t) / (t + 237.3));
  float vpact = vpsat * (rh / 100.0);
  float vpd = vpsat - vpact;
  
  float delta = (4098.0 * vpsat) / ((t + 237.3) * (t + 237.3));
  float gamma = 0.000665 * (pressaoHpa / 10.0); // kPa
  
  // Conversão de Lux para Radiação Solar (Rs) em MJ/m²
  float rs = (lux * 0.0079 * (intervaloMin * 60.0)) / 1000000.0;
  float netRad = 0.63 * rs; // Rn - G aproximado
  
  // Fórmula simplificada Penman-Monteith (FAO-56)
  float num = (0.257 * delta * netRad) + (gamma * (74.0 / (t + 273.0)) * vpd);
  float den = delta + (1.68 * gamma);
  
  float et0 = num / den;
  return (et0 < 0.0) ? 0.0 : et0; // mm por intervalo
}

/**
 * Função utilitária para encriptação AES-128-CBC
 */
bool encryptAES(const String &plainText, const String &keyStr, String &cipherTextB64) {
  char key[17] = "MARIACRYPTOKEY12"; // Padrão caso falhe
  if (keyStr.length() > 0) {
    memset(key, 0, sizeof(key));
    strncpy(key, keyStr.c_str(), 16);
    for (int i = keyStr.length(); i < 16; i++) {
      key[i] = '0'; // Padding simples da chave
    }
  }

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);

  int plainLen = plainText.length();
  int padLen = 16 - (plainLen % 16);
  int paddedLen = plainLen + padLen;

  uint8_t *paddedInput = (uint8_t *)malloc(paddedLen);
  if (!paddedInput) {
    mbedtls_aes_free(&aes);
    return false;
  }
  memcpy(paddedInput, plainText.c_str(), plainLen);
  for (int i = plainLen; i < paddedLen; i++) {
    paddedInput[i] = padLen; // Preenchimento PKCS7
  }

  uint8_t *encryptedOutput = (uint8_t *)malloc(paddedLen);
  if (!encryptedOutput) {
    free(paddedInput);
    mbedtls_aes_free(&aes);
    return false;
  }

  mbedtls_aes_setkey_enc(&aes, (const unsigned char *)key, 128);

  // Vetor de Inicialização (IV) Randômico seguro
  uint8_t iv[16];
  for (int i = 0; i < 16; i++) {
    iv[i] = esp_random() & 0xFF;
  }

  uint8_t ivTemp[16];
  memcpy(ivTemp, iv, 16);

  int ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, paddedLen, ivTemp, paddedInput, encryptedOutput);
  mbedtls_aes_free(&aes);
  free(paddedInput);

  if (ret != 0) {
    free(encryptedOutput);
    return false;
  }

  // Prepara buffer contendo IV + Ciphertext
  int totalLen = 16 + paddedLen;
  uint8_t *combined = (uint8_t *)malloc(totalLen);
  if (!combined) {
    free(encryptedOutput);
    return false;
  }
  memcpy(combined, iv, 16);
  memcpy(combined + 16, encryptedOutput, paddedLen);
  free(encryptedOutput);

  // Conversão para Base64
  size_t olen = 0;
  size_t expectedB64Len = ((totalLen + 2) / 3) * 4 + 1;
  char *b64Buffer = (char *)malloc(expectedB64Len);
  if (!b64Buffer) {
    free(combined);
    return false;
  }

  ret = mbedtls_base64_encode((unsigned char *)b64Buffer, expectedB64Len, &olen, combined, totalLen);
  free(combined);

  if (ret != 0) {
    free(b64Buffer);
    return false;
  }

  b64Buffer[olen] = '\0';
  cipherTextB64 = String(b64Buffer);
  free(b64Buffer);
  return true;
}

bool carregarConfig() {
  if (!sdOK) return false;
  if (!SD.exists(CONFIG_FILE)) {
    Serial.println("AVISO: " CONFIG_FILE " ausente. Usando WiFi fixo.");
    return false;
  }
  File f = SD.open(CONFIG_FILE, FILE_READ);
  if (!f) return false;

  while (f.available()) {
    String linha = f.readStringUntil('\n');
    linha.trim();
    int idx = linha.indexOf('=');
    if (idx < 0) continue;
    String chave = linha.substring(0, idx);
    String valor = linha.substring(idx + 1);
    valor.trim();

    if (chave == "modo")      cfg.modo = valor;
    else if (chave == "ssid")      cfg.ssid = valor;
    else if (chave == "senha")     cfg.senha = valor;
    else if (chave == "intervalo") cfg.intervaloMin = valor.toInt();
    else if (chave == "estacao")   cfg.estacao = valor;
    else if (chave == "chave_crypto") cfg.chaveCrypto = valor;
  }
  f.close();

  // Validacao
  if (cfg.modo != "WIFI" && cfg.modo != "RADIO") return false;
  if (cfg.intervaloMin != 1 && cfg.intervaloMin != 5 &&
      cfg.intervaloMin != 10 && cfg.intervaloMin != 15) return false;
  if (!nomeEstacaoValido(cfg.estacao)) return false;
  if (cfg.chaveCrypto.length() < 16) cfg.chaveCrypto = "MARIACRYPTOKEY12"; // Fallback seguro

  cfg.valida = true;
  return true;
}

bool salvarConfig() {
  if (!sdOK) return false;
  SD.remove(CONFIG_FILE);
  File f = SD.open(CONFIG_FILE, FILE_WRITE);
  if (!f) return false;
  f.printf("modo=%s\n", cfg.modo.c_str());
  f.printf("ssid=%s\n", cfg.ssid.c_str());
  f.printf("senha=%s\n", cfg.senha.c_str());
  f.printf("intervalo=%d\n", cfg.intervaloMin);
  f.printf("estacao=%s\n", cfg.estacao.c_str());
  f.printf("chave_crypto=%s\n", cfg.chaveCrypto.c_str());
  f.close();
  return true;
}

String paginaConfigHTML() {
  int n = WiFi.scanNetworks();
  String redesHtml = "";
  if (n <= 0) {
    redesHtml = "<option value=\"\">Nenhuma rede Wi-Fi encontrada</option>";
  } else {
    for (int i = 0; i < n; i++) {
      String ssid = WiFi.SSID(i);
      String selected = (ssid == cfg.ssid) ? "selected" : "";
      redesHtml += "<option value=\"" + ssid + "\" " + selected + ">" +
                   ssid + " (" + String(WiFi.RSSI(i)) + " dBm)</option>";
    }
  }

  // Layout responsivo premium com efeito Glassmorphism escuro
  String html = R"raw(
  <!DOCTYPE html>
  <html lang="pt-BR">
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Configuração - Projeto GAIA</title>
    <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600;700&display=swap" rel="stylesheet">
    <style>
      :root {
        --bg-color: #0b0f19;
        --card-bg: rgba(255, 255, 255, 0.05);
        --card-border: rgba(255, 255, 255, 0.08);
        --accent-color: #10b981;
        --accent-hover: #059669;
        --text-color: #f3f4f6;
        --text-muted: #9ca3af;
        --danger-color: #ef4444;
        --danger-hover: #dc2626;
      }
      * { box-sizing: border-box; margin: 0; padding: 0; }
      body {
        font-family: 'Outfit', sans-serif;
        background-color: var(--bg-color);
        background-image: radial-gradient(circle at top right, rgba(16, 185, 129, 0.08), transparent 45%),
                          radial-gradient(circle at bottom left, rgba(59, 130, 246, 0.05), transparent 45%);
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
        background: linear-gradient(135deg, #10b981, #3b82f6);
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
        box-shadow: 0 0 12px rgba(16, 185, 129, 0.25);
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
        box-shadow: 0 6px 20px rgba(16, 185, 129, 0.35);
      }
      .btn-danger {
        background: rgba(239, 68, 68, 0.12);
        border: 1px solid rgba(239, 68, 68, 0.25);
        color: var(--danger-color);
      }
      .btn-danger:hover {
        background: var(--danger-color);
        color: #fff;
        box-shadow: 0 6px 20px rgba(239, 68, 68, 0.25);
      }
      hr {
        border: 0;
        height: 1px;
        background: var(--card-border);
        margin: 30px 0;
      }
    </style>
  </head>
  <body>
    <div class="container">
      <h2>Estação GAIA</h2>
      <p class="subtitle">Ajuste os parâmetros operacionais da estação</p>
      
      <form action="/salvar" method="POST" onsubmit="return prepararForm()">
        <input type="hidden" name="browser_ts" id="browser_ts">
        
        <div class="form-group">
          <label for="estacao">Código da Estação (6 caracteres)</label>
          <input type="text" id="estacao" name="estacao" value="%ESTACAO%" maxlength="6" required placeholder="Ex: G00001">
        </div>
        
        <div class="form-group">
          <label for="modo">Modo de Operação</label>
          <select id="modo" name="modo" onchange="toggleWifi()">
            <option value="WIFI" %MODO_WIFI%>WiFi (Servidor HTTP)</option>
            <option value="RADIO" %MODO_RADIO%>Rádio (LoRa E220)</option>
          </select>
        </div>
        
        <div id="wifiDiv" style="display: %WIFI_DISPLAY%;">
          <div class="form-group">
            <label for="ssid">Redes WiFi Disponíveis</label>
            <select id="ssid" name="ssid">
              %REDES_WIFI%
            </select>
          </div>
          
          <div class="form-group">
            <label for="senha">Senha do WiFi</label>
            <input type="password" id="senha" name="senha" value="%SENHA%" placeholder="Digite a senha">
          </div>
        </div>
        
        <div class="form-group">
          <label for="intervalo">Intervalo de Transmissão</label>
          <select id="intervalo" name="intervalo">
            <option value="1" %INT_1%>1 minuto (Teste)</option>
            <option value="5" %INT_5%>5 minutos</option>
            <option value="10" %INT_10%>10 minutos</option>
            <option value="15" %INT_15%>15 minutos</option>
          </select>
        </div>
        
        <div class="form-group">
          <label for="chave_crypto">Chave Criptográfica LoRa / WiFi (16 chars)</label>
          <input type="text" id="chave_crypto" name="chave_crypto" value="%CHAVE%" minlength="16" maxlength="16" required placeholder="Ex: ChaveSecreta123">
        </div>
        
        <button type="submit" class="btn btn-primary">Salvar e Reiniciar</button>
      </form>
      
      <hr>
      
      <form action="/formatar" method="POST" onsubmit="return confirm('Deseja realmente apagar todos os dados e logs do cartão SD?');">
        <button type="submit" class="btn btn-danger">Formatar Cartão SD</button>
      </form>
    </div>
    
    <script>
      function toggleWifi() {
        var m = document.getElementById('modo').value;
        document.getElementById('wifiDiv').style.display = (m == 'WIFI') ? 'block' : 'none';
      }
      function prepararForm() {
        var key = document.getElementById('chave_crypto').value;
        if (key.length !== 16) {
          alert('Erro: A chave criptográfica deve ter exatamente 16 caracteres!');
          return false;
        }
        document.getElementById('browser_ts').value = Math.floor(Date.now() / 1000);
        return true;
      }
      toggleWifi();
    </script>
  </body>
  </html>
  )raw";

  html.replace("%ESTACAO%", cfg.estacao);
  html.replace("%MODO_WIFI%", cfg.modo == "WIFI" ? "selected" : "");
  html.replace("%MODO_RADIO%", cfg.modo == "RADIO" ? "selected" : "");
  html.replace("%WIFI_DISPLAY%", cfg.modo == "WIFI" ? "block" : "none");
  html.replace("%REDES_WIFI%", redesHtml);
  html.replace("%SENHA%", cfg.senha);
  html.replace("%INT_1%", cfg.intervaloMin == 1 ? "selected" : "");
  html.replace("%INT_5%", cfg.intervaloMin == 5 ? "selected" : "");
  html.replace("%INT_10%", cfg.intervaloMin == 10 ? "selected" : "");
  html.replace("%INT_15%", cfg.intervaloMin == 15 ? "selected" : "");
  html.replace("%CHAVE%", cfg.chaveCrypto);

  return html;
}

void tratarRaiz() {
  server.send(200, "text/html", paginaConfigHTML());
}

void tratarSalvar() {
  String estacao = server.arg("estacao");
  estacao.toUpperCase();

  if (!nomeEstacaoValido(estacao)) {
    server.send(400, "text/plain", "Nome de estacao invalido. Use formato E99999.");
    return;
  }

  cfg.estacao      = estacao;
  cfg.modo         = server.arg("modo");
  cfg.ssid         = server.arg("ssid");
  cfg.senha        = server.arg("senha");
  cfg.intervaloMin = server.arg("intervalo").toInt();
  cfg.chaveCrypto  = server.arg("chave_crypto");

  // Ajuste do RTC via navegador
  if (server.hasArg("browser_ts")) {
    uint32_t ts = server.arg("browser_ts").toInt();
    if (ts > 1600000000 && rtcOK) {
      rtc.adjust(DateTime(ts));
      gravarLogPermanente("INFO", "RTC sincronizado via AP com horario do browser: " + timestampFormatado(ts));
    }
  }

  bool ok = salvarConfig();
  server.send(200, "text/plain", ok ? "Configuracao salva! Reiniciando..." :
                                       "Erro ao salvar no SD.");
  gravarLogPermanente("INFO", "Dispositivo configurado via AP. Reiniciando...");
  delay(1500);
  ESP.restart();
}

void tratarFormatar() {
  formatarCartaoSD();
  server.send(200, "text/plain", "Cartao formatado. Reiniciando...");
  delay(1500);
  ESP.restart();
}

void iniciarModoAP() {
  String apNome = "GAIA-TX-" + String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF), HEX);
  apNome.toUpperCase();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apNome.c_str(), "gaiaconfig");

  Serial.println("AVISO: Iniciando AP de setup.");
  Serial.print("AP ativo: ");
  Serial.println(apNome);
  Serial.println("Acesse: http://192.168.4.1");

  server.on("/", HTTP_GET, tratarRaiz);
  server.on("/salvar", HTTP_POST, tratarSalvar);
  server.on("/formatar", HTTP_POST, tratarFormatar);
  server.begin();

  unsigned long inicioAP = millis();
  
  // Timeout de 10 minutos para sair do portal caso inativo
  while (true) {
    server.handleClient();
    esp_task_wdt_reset();
    verificarBotaoFormatar();
    
    // Pisca o LED lentamente para indicar modo de configuracao ativo
    static unsigned long ultimoBlink = 0;
    if (millis() - ultimoBlink > 500) {
      ultimoBlink = millis();
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
    
    // Auto-recuperacao: Se ficar inativo por mais de 10 min, reinicia
    if (millis() - inicioAP > 600000UL) {
      gravarLogPermanente("WARN", "Timeout do modo AP atingido. Reiniciando...");
      delay(100);
      ESP.restart();
    }
    delay(10);
  }
}

void formatarCartaoSD() {
  gravarLogPermanente("WARN", "Formatando cartao SD (removendo fila e dados)...");
  if (SD.exists(DATA_FILE))    SD.remove(DATA_FILE);
  if (SD.exists(QUEUE_FILE))   SD.remove(QUEUE_FILE);
  if (SD.exists(LASTACK_FILE)) SD.remove(LASTACK_FILE);
  if (SD.exists(LOG_FILE))     SD.remove(LOG_FILE);
  Serial.println("Cartao SD limpo.");
}

unsigned long btnFormatPressedAt = 0;
void verificarBotaoFormatar() {
  if (digitalRead(BTN_FORMAT_PIN) == LOW) {
    if (btnFormatPressedAt == 0) btnFormatPressedAt = millis();
    if (millis() - btnFormatPressedAt > 5000) {
      formatarCartaoSD();
      Serial.println("SD formatado via botao fisico. Reiniciando...");
      delay(1000);
      ESP.restart();
    }
  } else {
    btnFormatPressedAt = 0;
  }
}

void iniciarSensores() {
  sdOK = SD.begin(SD_CS);
  if (!sdOK) {
    Serial.println("ERRO: Falha no cartao SD. Rodando sem persistência local.");
  } else {
    Serial.println("Cartao SD inicializado");
  }

  bmpOK = bmp.begin(0x76);
  if (!bmpOK) {
    Serial.println("AVISO: BME280 nao detectado em 0x76.");
  }

  vemlOK = veml.begin();
  if (!vemlOK) {
    Serial.println("AVISO: VEML7700 nao detectado.");
  }

  dht.begin();
  // Validacao real da conexao fisica do DHT22
  float tTest = dht.readTemperature();
  float hTest = dht.readHumidity();
  if (isnan(tTest) || isnan(hTest)) {
    dhtOK = false;
    Serial.println("AVISO: Sensor DHT22 desconectado ou inoperante.");
  } else {
    dhtOK = true;
    Serial.println("DHT22 inicializado com sucesso.");
  }

  // Inicializacao do sensor de solo DS18B20
  ds18Sensors.begin();
  ds18Sensors.requestTemperatures();
  float tSoloTest = ds18Sensors.getTempCByIndex(0);
  if (tSoloTest == DEVICE_DISCONNECTED_C) {
    ds18OK = false;
    Serial.println("AVISO: Sensor DS18B20 nao detectado no barramento 1-Wire.");
  } else {
    ds18OK = true;
    Serial.println("DS18B20 inicializado com sucesso.");
  }
}

void iniciarRTC() {
  rtcOK = rtc.begin();
  if (!rtcOK) {
    Serial.println("AVISO: RTC DS3231 nao detectado. Usando relogio interno.");
  } else {
    // Checa se perdeu energia
    if (rtc.lostPower()) {
      Serial.println("WARN: RTC perdeu energia. Ajuste necessario.");
    }
  }
}

bool iniciarLoRa() {
  loraSerial.begin(9600, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);
  e220ttl.begin();

  ResponseStructContainer c = e220ttl.getConfiguration();
  if (c.status.code != 1) { 
    Serial.print("ERRO: Falha ao ler configuracao do E220: ");
    Serial.println(c.status.getResponseDescription());
    c.close();
    return false;
  }
  c.close();

  Serial.println("LoRa E220 inicializado.");
  return true;
}

bool conectarWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.ssid.c_str(), cfg.senha.c_str());

  for (int tentativa = 1; tentativa <= 5; tentativa++) {
    Serial.printf("Tentando WiFi %d/5\n", tentativa);
    unsigned long inicio = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - inicio < 4000) {
      delay(200);
      esp_task_wdt_reset();
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi conectado");
      return true;
    }
  }
  Serial.println("AVISO: Falha na conexao WiFi.");
  return false;
}

void sincronizarNTP() {
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 5000)) {
    char buf[16];
    strftime(buf, sizeof(buf), "%d/%m %H:%M", &timeinfo);
    Serial.printf("NTP OK: %s\n", buf);
    time_t agora = time(NULL);
    softClockBase = (uint32_t)agora;
    softClockMillisBase = millis();
    
    // Atualiza relógio físico DS3231 para evitar desvios (drift)
    if (rtcOK && agora > 1600000000) {
      rtc.adjust(DateTime(agora));
      gravarLogPermanente("INFO", "RTC DS3231 sincronizado com o NTP.");
    }
  } else {
    Serial.println("AVISO: NTP falhou.");
  }
}

void carregarLastAck() {
  if (!sdOK || !SD.exists(LASTACK_FILE)) return;
  File f = SD.open(LASTACK_FILE, FILE_READ);
  if (!f) return;
  lastAckTs = f.parseInt();
  f.close();
}

void salvarLastAck(uint32_t ts) {
  lastAckTs = ts;
  if (!sdOK) return;
  SD.remove(LASTACK_FILE);
  File f = SD.open(LASTACK_FILE, FILE_WRITE);
  if (f) { f.print(ts); f.close(); }
}

void gravarLogPermanente(const String &nivel, const String &mensagem) {
  if (!sdOK) {
    Serial.printf("[%s] %s (SD offline)\n", nivel.c_str(), mensagem.c_str());
    return;
  }
  File logFile = SD.open(LOG_FILE, FILE_APPEND);
  if (logFile) {
    uint32_t ts = obterTimestamp();
    logFile.printf("[%s] [%s] %s\n", timestampFormatado(ts).c_str(), nivel.c_str(), mensagem.c_str());
    logFile.close();
  }
  Serial.printf("[%s] %s\n", nivel.c_str(), mensagem.c_str());
}

void enfileirar(const String &payload) {
  if (!sdOK) return;
  
  // Enfileira o payload já criptografado
  String aSalvar = payload;
  if (payload.indexOf('{') >= 0) { // Se for JSON em texto claro, encripta antes de salvar
    if (!encryptAES(payload, cfg.chaveCrypto, aSalvar)) {
      gravarLogPermanente("ERRO", "Falha de criptografia na fila offline.");
      return;
    }
  }

  File f = SD.open(QUEUE_FILE, FILE_APPEND);
  if (f) {
    f.println(aSalvar);
    f.close();
    gravarLogPermanente("INFO", "Dado criptografado inserido na fila offline.");
  }
}

// Reenvio seguro linha por linha sem fragmentação de memória Heap
void processarFila(int &reenviados, int &pendentes) {
  reenviados = 0;
  pendentes = 0;
  if (!sdOK || !SD.exists(QUEUE_FILE)) return;

  File f = SD.open(QUEUE_FILE, FILE_READ);
  if (!f) return;

  String tempPath = "/q_tmp.csv";
  if (SD.exists(tempPath)) SD.remove(tempPath);

  File fTemp = SD.open(tempPath, FILE_WRITE);
  if (!fTemp) {
    f.close();
    return;
  }

  while (f.available()) {
    String linha = f.readStringUntil('\n');
    linha.trim();
    if (linha.length() == 0) continue;

    bool ok = false;
    if (cfg.modo == "RADIO") {
      ok = enviarPorRadio(linha, true); // Envia direto (ja criptografado)
    } else {
      ok = enviarHTTP(linha, true);     // Envia direto (ja criptografado)
    }

    if (ok) {
      reenviados++;
    } else {
      fTemp.println(linha);
      pendentes++;
    }
    esp_task_wdt_reset();
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
}

bool enviarHTTP(const String &payload, bool isEncrypted = false) {
  if (WiFi.status() != WL_CONNECTED) return false;

  String toSend = payload;
  if (!isEncrypted) {
    if (!encryptAES(payload, cfg.chaveCrypto, toSend)) {
      gravarLogPermanente("ERRO", "Falha ao encriptar payload HTTP.");
      return false;
    }
  }

  // Prepara envelope JSON: {"data": "Base64EncryptedText"}
  StaticJsonDocument<512> doc;
  doc["data"] = toSend;
  String body;
  serializeJson(doc, body);

  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  String response = http.getString();
  http.end();

  return (code == 200);
}

bool enviarPorRadio(const String &payload, bool isEncrypted = false) {
  if (!loraOK) {
    return false;
  }

  String toSend = payload;
  if (!isEncrypted) {
    if (!encryptAES(payload, cfg.chaveCrypto, toSend)) {
      gravarLogPermanente("ERRO", "Falha ao encriptar payload LoRa.");
      return false;
    }
  }
  toSend += "\n"; // Adiciona quebra de linha para o receptor identificar o fim do pacote

  ResponseStatus rs = e220ttl.sendMessage(toSend);
  if (rs.code != 1) { 
    return false;
  }

  return true;
}



Leituras lerSensores() {
  Leituras l;

  if (bmpOK) {
    l.tBmp = bmp.readTemperature();
    l.pressao = bmp.readPressure() / 100.0;
  }

  if (vemlOK) {
    l.lux = veml.readLux();
  }

  if (dhtOK) {
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (!isnan(h) && !isnan(t)) {
      l.umidade = h;
      l.tDht = t;
    }
  }

  int raw = analogRead(SOLO_PIN);
  if (raw <= 10 || raw >= 4090) {
    l.solo = -1; 
  } else {
    l.solo = map(constrain(raw, 1100, 3200), 3200, 1100, 0, 100);
  }

  // Leitura do sensor DS18B20 (Temperatura do solo)
  if (ds18OK) {
    ds18Sensors.requestTemperatures();
    float temp = ds18Sensors.getTempCByIndex(0);
    if (temp != DEVICE_DISCONNECTED_C) {
      l.tSolo = temp;
    }
  }

  // Realiza os calculos agrometeorologicos baseados nos sensores disponiveis
  float tempRef = l.tDht;
  if (isnan(tempRef)) tempRef = l.tBmp; // Fallback para o sensor secundario de temperatura

  l.po = calcularPontoOrvalho(tempRef, l.umidade);
  l.vpd = calcularVPD(tempRef, l.umidade);
  l.et0 = calcularET0(tempRef, l.umidade, l.pressao, l.lux, cfg.intervaloMin);

  return l;
}

void gravarDadosTXT(const String &jsonPayload) {
  if (!sdOK) return;
  File dataFile = SD.open(DATA_FILE, FILE_APPEND);
  if (dataFile) {
    dataFile.println(jsonPayload);
    dataFile.close();
  }
}

String montarPayload(const Leituras &l, uint32_t ts) {
  StaticJsonDocument<512> doc; 
  doc["codSta"] = cfg.estacao;
  doc["ts"] = ts;

  if (!isnan(l.tBmp))    doc["tB"] = l.tBmp; else doc["tB"] = nullptr;
  if (!isnan(l.pressao)) doc["p"]  = l.pressao; else doc["p"] = nullptr;
  if (!isnan(l.lux))     doc["l"]  = l.lux; else doc["l"] = nullptr;
  if (!isnan(l.umidade)) doc["h"]  = l.umidade; else doc["h"] = nullptr;
  if (!isnan(l.tDht))    doc["tD"] = l.tDht; else doc["tD"] = nullptr;
  if (l.solo >= 0)       doc["s"]  = l.solo; else doc["s"] = nullptr;
  if (!isnan(l.tSolo))   doc["tS"] = l.tSolo; else doc["tS"] = nullptr; 
  
  // Novas variáveis no JSON enviadas ao servidor/receptor
  if (!isnan(l.po))      doc["po"]  = l.po;  else doc["po"]  = nullptr;
  if (!isnan(l.vpd))     doc["vpd"] = l.vpd; else doc["vpd"] = nullptr;
  if (!isnan(l.et0))     doc["et0"] = l.et0; else doc["et0"] = nullptr;

  doc["heapMod"] = ESP.getFreeHeap() % 1000;

  String out;
  serializeJson(doc, out);
  return out;
}

void realizarEnvio() {
  Leituras l = lerSensores();
  uint32_t ts = obterTimestamp();

  String payload = montarPayload(l, ts);
  
  // Salva localmente as leituras formatadas em JSON em "/dados.txt"
  gravarDadosTXT(payload);

  // Registra leitura com as novas métricas calculadas no log local do SD
  char logMsg[128];
  snprintf(logMsg, sizeof(logMsg), "Leitura - Temp:%.1fC Umid:%.1f%% Solo:%d TempSolo:%.1fC VPD:%.2fkPa ET0:%.3fmm", 
           isnan(l.tDht) ? l.tBmp : l.tDht, l.umidade, l.solo, l.tSolo, l.vpd, l.et0);
  gravarLogPermanente("INFO", String(logMsg));

  if (cfg.modo == "RADIO") {
    int reenviados = 0, pendentes = 0;
    processarFila(reenviados, pendentes);

    bool ok = enviarPorRadio(payload, false);
    if (ok) {
      salvarLastAck(ts);
      gravarLogPermanente("SUCESSO", "Pacote LoRa enviado e confirmado.");
      piscarLED(1, 400, 0); // Piscada de sucesso
    } else {
      enfileirar(payload); 
      gravarLogPermanente("FALHA", "Erro LoRa. Pacote salvo na fila offline.");
      piscarLED(3, 100, 100); // 3 piscadas rápidas de erro
    }
  } else {
    int reenviados = 0, pendentes = 0;
    processarFila(reenviados, pendentes);

    bool ok = enviarHTTP(payload, false);
    if (ok) {
      salvarLastAck(ts);
      gravarLogPermanente("SUCESSO", "Pacote HTTP enviado com sucesso.");
      piscarLED(1, 400, 0); // Piscada de sucesso
    } else {
      enfileirar(payload);
      gravarLogPermanente("FALHA", "Erro HTTP. Pacote salvo na fila offline.");
      piscarLED(3, 100, 100); // 3 piscadas rápidas de erro
    }
  }
}

void configurarWatchdog() {
  #if ESP_ARDUINO_VERSION_MAJOR >= 3
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 30000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .trigger_panic = true
    };
    esp_task_wdt_init(&wdt_config);
  #else
    esp_task_wdt_init(30, true);
  #endif
  esp_task_wdt_add(NULL);
}

void irParaDeepSleep(uint32_t segundos) {
  gravarLogPermanente("INFO", "Entrando em Deep Sleep por " + String(segundos) + " segundos.");
  piscarLED(2, 100, 100); // 2 piscadas de desligamento
  delay(100);
  esp_sleep_enable_timer_wakeup((uint64_t)segundos * 1000000ULL);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Define timeout I2C para prevenir travamentos nos sensores
  Wire.begin(21, 22);
  Wire.setTimeOut(100); 

  pinMode(BTN_FORMAT_PIN, INPUT_PULLUP);
  pinMode(BTN_AP_PIN, INPUT_PULLUP);

  configurarWatchdog();
  iniciarSensores();
  iniciarRTC();

  // Pisca o LED uma vez para sinalizar que o ESP32 acordou/iniciou
  piscarLED(1, 200, 0);

  bool configOk = carregarConfig();
  bool forcarAP = (digitalRead(BTN_AP_PIN) == LOW);

  if (!configOk || forcarAP) {
    iniciarModoAP();
  }

  // Offset temporizado real para evitar colisao
  uint64_t chipId = ESP.getEfuseMac();
  offsetSegundos = chipId % (cfg.intervaloMin * 60);
  
  gravarLogPermanente("INFO", "Estacao GAIA acordou. Codigo: " + cfg.estacao);
  
  carregarLastAck();

  if (cfg.modo == "WIFI") {
    wifiOK = conectarWiFi();
    if (wifiOK) {
      sincronizarNTP();
    }
  } else {
    loraOK = iniciarLoRa();
    if (!loraOK) {
      gravarLogPermanente("WARN", "Falha de comunicacao com modulo LoRa.");
    }
  }

  // Realiza a leitura e o envio
  realizarEnvio();

  // Calculo de alinhamento com a grade de tempo (RTC) + offset do dispositivo
  uint32_t segundosParaDormir = 60; // Fallback
  if (rtcOK) {
    DateTime agora = rtc.now();
    uint32_t minutoAtual = agora.minute();
    uint32_t segundoAtual = agora.second();
    
    // Minutos que faltam para o proximo slot de intervalo
    uint32_t minutosRestantes = cfg.intervaloMin - (minutoAtual % cfg.intervaloMin);
    uint32_t totalSegundos = (minutosRestantes * 60) - segundoAtual;
    
    // Aplica o offset temporal
    int32_t finalSleepSec = totalSegundos + offsetSegundos;
    uint32_t intervaloSegundos = cfg.intervaloMin * 60;
    
    if (finalSleepSec <= 0) {
      finalSleepSec += intervaloSegundos;
    } else if (finalSleepSec > (int32_t)intervaloSegundos) {
      finalSleepSec -= (finalSleepSec / intervaloSegundos) * intervaloSegundos;
      if (finalSleepSec <= 0) finalSleepSec += intervaloSegundos;
    }
    segundosParaDormir = finalSleepSec;
  } else {
    segundosParaDormir = (cfg.intervaloMin * 60) + offsetSegundos;
  }

  // Dorme pelo período calculado. O ESP32 reiniciará ao acordar, rodando o setup novamente.
  irParaDeepSleep(segundosParaDormir);
}

void loop() {
  // Sob operacao normal, este loop nunca e alcancado devido ao Deep Sleep no setup().
  // E alcancado apenas se o Deep Sleep falhar ou em modo AP.
  esp_task_wdt_reset();
  delay(100);
}