/**
 * ============================================================================
 * ARQUIVO: TX Telemetria.c / TX_Telemetria.ino
 * PROJETO: Projeto GAIA - Estação Agroclimática de Campo
 * VERSÃO: 3.0
 * DATA: 2026-09-18
 * MODIFICAÇÕES:
 * 1. Remoção do RTC físico (DS3231) e eliminação do rádio LoRa.
 * 2. Validação e precisão temporal de timestamp (ts) via Relógio Atômico (NTP.br: a.st1.ntp.br).
 * 3. Comunicação 100% via Internet / Wi-Fi com criptografia simétrica AES-128-CBC.
 * 4. Alinhamento de Deep Sleep baseado no tempo atômico mantido pelo RTC interno do ESP32.
 * 5. Portal Web AP com diagnóstico em tempo real de sensores e status do Relógio Atômico.
 * 6. Suporte completo ao Sensor de Temperatura do Solo DS18B20 (1-Wire) e Umidade (ADC).
 * 7. Gestão de persistência resiliente híbrida (Flash NVS Preferences + Backup em SD).
 * ============================================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <time.h>
#include <esp_task_wdt.h>
#include <math.h>

#include <Adafruit_BMP280.h>
#include <Adafruit_VEML7700.h>
#include <DHT.h>
#include <ArduinoJson.h>

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
  
  // Fisiologia Vegetal e Manejo Hidrico
  float po = NAN;        // Ponto de Orvalho (°C)
  float vpd = NAN;       // Vapor Pressure Deficit (kPa)
  float et0 = NAN;       // Evapotranspiracao de referencia FAO-56 (mm)
  float etc = NAN;       // Evapotranspiracao da cultura (mm, Kc = 1.0)
  
  // Radiacao Solar e Fotossintese
  float rs = NAN;        // Radiacao Solar Global estimada (W/m²)
  float par = NAN;       // Radiacao Fotossinteticamente Ativa PPFD (µmol/m²·s)
  
  // Fitossanidade e Riscos
  int   riscoFungo = 0;  // 0: Baixo, 1: Moderado, 2: Alto (Alerta)
  float horasMolhadas = 0.0; // Horas continuas de folha molhada
  int   riscoGeada = 0;  // 0: Sem Risco, 1: Geada Branca, 2: Geada Negra
  
  // Barometria e Previsao Local
  float deltaP = 0.0;    // Variacao de pressao 3h (hPa)
  bool  alertaTempestade = false; // Queda brusca de pressao (< -2.0 hPa)
  
  // Bioclimatologia e Zootecnia
  float itu = NAN;       // Indice de Temperatura e Umidade (Conforto Animal)
  float hi = NAN;        // Sensacao Termica / Heat Index (°C)
  
  float tempRef = NAN;
  float deltaTemp = NAN;
  int   qualidade = 0;
};

// Variaveis preservadas na memoria RTC RAM durante o Deep Sleep (sem gravar na flash)
RTC_DATA_ATTR float    rtcHistoricoPressao[12] = {0};
RTC_DATA_ATTR int      rtcContadorPressao = 0;
RTC_DATA_ATTR float    rtcHorasMolhadas = 0.0;
RTC_DATA_ATTR uint32_t rtcUltimoTimestamp = 0;

// pinos
#define SOLO_PIN        34
#define SD_CS           5
#define DHTPIN          14
#define DHTTYPE         DHT22
#define BTN_FORMAT_PIN  4    // segure 5s no boot ou em uso para formatar o SD
#define BTN_AP_PIN      15   // segure no boot para forcar modo AP de configuracao
#define BTN_BOOT_PIN    0    // botao BOOT da placa ESP32 tambem forca modo AP

#define LED_PIN         2    // LED interno do ESP32 para feedback visual

// Pino de dados do DS18B20
#define ONE_WIRE_BUS    13   

#define LOCAL_SERVER_URL    "http://192.168.0.105:3000/api/telemetria"
#define DEFAULT_SERVER_URL  "https://server1.taila7d06b.ts.net/api/telemetria"
#define DEFAULT_CRYPTO_KEY  "GaiaAgroIoT2026!"
#define GAIA_PROJECT_TOKEN  "GAIA_TOKEN_2026_AGRO"

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
WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;

// Objeto de persistencia interna (NVS)
static const char* PREF_NAMESPACE = "gaia_cfg";
Preferences preferences;

// Setup da rede 1-Wire para o DS18B20
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18Sensors(&oneWire);

bool sdOK             = false;
bool bmpOK            = false;
bool vemlOK           = false;
bool dhtOK            = false;
bool wifiOK           = false;
bool ds18OK           = false;
bool ntpSincronizado  = false;
uint32_t ultimoTsValidado = 0;

struct Config {
  String modo = "WIFI";       
  String ssid;
  String senha;
  int    intervaloMin = 5;  // 1, 5, 10 ou 15
  String estacao = "G00001";
  String serverUrl = DEFAULT_SERVER_URL;
  String chaveCrypto = DEFAULT_CRYPTO_KEY; // chave de 16 caracteres para criptografia
  bool   valida = false;
};
Config cfg;

long offsetSegundos = 0;
uint32_t lastAckTs = 0;
uint32_t softClockBase = 0;
unsigned long softClockMillisBase = 0;

// Cache HTML para redes escaneadas
String redesScanHtml = "";

// Protótipos das funções auxiliares
void formatarCartaoSD();
void verificarBotaoFormatar();
bool enviarHTTP(const String &payload, bool isEncrypted);
void gravarLogPermanente(const String &nivel, const String &mensagem);
Leituras lerSensores();
void scanWifiNetworks();
void tratarScan();
void tratarApiSensores();

void piscarLED(int piscadas, int tempoLigado, int tempoDesligado) {
  for (int i = 0; i < piscadas; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(tempoLigado);
    digitalWrite(LED_PIN, LOW);
    if (i < piscadas - 1) delay(tempoDesligado);
  }
}

bool isTimestampValido() {
  time_t agora;
  time(&agora);
  return (agora > 1700000000);
}

uint32_t obterTimestamp() {
  time_t agora;
  time(&agora);
  if (agora > 1700000000) { 
    ultimoTsValidado = (uint32_t)agora;
    return (uint32_t)agora;
  }
  if (ultimoTsValidado > 1700000000) {
    return ultimoTsValidado + (millis() / 1000);
  }
  return softClockBase + (millis() - softClockMillisBase) / 1000;
}

String timestampFormatado(uint32_t ts) {
  time_t t = ts;
  struct tm *tmInfo = localtime(&t);
  char buf[80];
  snprintf(buf, sizeof(buf), "%02d/%02d/%04d %02d:%02d:%02d",
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

void calcularRadiacao(float lux, float &rs, float &par) {
  if (isnan(lux) || lux < 0) {
    rs = NAN;
    par = NAN;
    return;
  }
  // Conversao empirica de luz solar natural: 1 Lux ≈ 0.0079 W/m² de radiacao solar global
  rs = lux * 0.0079f;
  // Radiacao Fotossinteticamente Ativa (PAR / PPFD): ~2.1 µmol/m²·s por W/m²
  par = rs * 2.1f;
}

void calcularBioclima(float temp, float rh, float &itu, float &hi) {
  if (isnan(temp) || isnan(rh)) {
    itu = NAN;
    hi = NAN;
    return;
  }

  // ITU (Indice de Temperatura e Umidade para Pecuaria - Thom / NRC)
  itu = 0.8f * temp + (rh / 100.0f) * (temp - 14.4f) + 46.4f;

  // Sensacao Termica / Indice de Calor Humano (Rothfusz / NOAA)
  if (temp >= 26.0f && rh >= 40.0f) {
    float t2 = temp * temp;
    float rh2 = rh * rh;
    hi = -8.78469475556f + 1.61139411f * temp + 2.33854883889f * rh
         - 0.14611605f * temp * rh - 0.012308094f * t2 - 0.0164248277778f * rh2
         + 0.002211732f * t2 * rh + 0.00072546f * temp * rh2 - 0.000003582f * t2 * rh2;
  } else {
    hi = temp;
  }
}

void avaliarFitossanidade(float temp, float rh, float po, int intervaloMin, int &riscoFungo, float &horasMolhadas, int &riscoGeada) {
  if (isnan(temp) || isnan(rh) || isnan(po)) {
    riscoFungo = 0;
    horasMolhadas = 0.0f;
    riscoGeada = 0;
    return;
  }

  // Condicao de molhamento foliar (orvalho/condensacao na folha)
  bool folhaMolhada = ((temp - po) <= 1.5f && rh >= 85.0f);
  float passoHoras = intervaloMin / 60.0f;

  if (folhaMolhada) {
    rtcHorasMolhadas += passoHoras;
  } else if (rh < 75.0f && (temp - po) > 2.5f) {
    rtcHorasMolhadas = max(0.0f, rtcHorasMolhadas - (passoHoras * 0.5f));
  }
  horasMolhadas = rtcHorasMolhadas;

  // Risco Fungico (Mildio, Ferrugem, Requeima): horas de molhamento continuo em temperatura favoravel
  bool tempFavoravelFungo = (temp >= 15.0f && temp <= 28.0f);
  if (rtcHorasMolhadas >= 8.0f && tempFavoravelFungo) {
    riscoFungo = 2; // Risco Alto / Alerta de Aplicacao Preventiva
  } else if (rtcHorasMolhadas >= 4.0f || (folhaMolhada && tempFavoravelFungo)) {
    riscoFungo = 1; // Risco Moderado
  } else {
    riscoFungo = 0; // Risco Baixo
  }

  // Avaliacao de Risco de Geada
  if (temp <= 0.0f && rh < 50.0f) {
    riscoGeada = 2; // Geada Negra (congelamento interno da seiva sem gelo aparente)
  } else if (temp <= 3.0f && po <= 0.0f) {
    riscoGeada = 1; // Geada Branca (congelamento do orvalho)
  } else {
    riscoGeada = 0; // Sem risco
  }
}

void avaliarTendenciaBarometrica(float pressaoAtual, float &deltaP, bool &alertaTempestade) {
  if (isnan(pressaoAtual) || pressaoAtual < 300.0f || pressaoAtual > 1100.0f) {
    deltaP = 0.0f;
    alertaTempestade = false;
    return;
  }

  // Desloca o historico na RTC RAM e insere a leitura atual
  if (rtcContadorPressao < 12) {
    rtcHistoricoPressao[rtcContadorPressao++] = pressaoAtual;
  } else {
    for (int i = 0; i < 11; i++) {
      rtcHistoricoPressao[i] = rtcHistoricoPressao[i + 1];
    }
    rtcHistoricoPressao[11] = pressaoAtual;
  }

  // Calcula a diferenca entre a pressao atual e a leitura mais antiga do historico
  if (rtcContadorPressao >= 2) {
    deltaP = pressaoAtual - rtcHistoricoPressao[0];
  } else {
    deltaP = 0.0f;
  }

  // Alerta de tempestade / frente fria severa: queda de mais de 2.0 hPa
  alertaTempestade = (deltaP <= -2.0f);
}

/**
 * Função utilitária para encriptação AES-128-CBC
 */
bool encryptAES(const String &plainText, const String &keyStr, String &cipherTextB64) {
  char key[17] = DEFAULT_CRYPTO_KEY; // Padrão oficial do Projeto GAIA
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
  // 1. Tenta carregar da memoria interna NVS (Preferences)
  preferences.begin(PREF_NAMESPACE, true);
  String pSsid = preferences.getString("ssid", "");
  String pSenha = preferences.getString("senha", "");
  String pEstacao = preferences.getString("estacao", "");
  int pIntervalo = preferences.getInt("intervalo", 0);
  String pServerUrl = preferences.getString("server_url", "");
  String pChave = preferences.getString("chave_crypto", "");
  preferences.end();

  if (pSsid.length() > 0 && nomeEstacaoValido(pEstacao)) {
    cfg.modo = "WIFI";
    cfg.ssid = pSsid;
    cfg.senha = pSenha;
    cfg.intervaloMin = (pIntervalo == 1 || pIntervalo == 5 || pIntervalo == 10 || pIntervalo == 15) ? pIntervalo : 5;
    cfg.estacao = pEstacao;
    if (pServerUrl.length() == 0 || pServerUrl.indexOf("trycloudflare.com") >= 0) {
      cfg.serverUrl = DEFAULT_SERVER_URL;
      preferences.begin(PREF_NAMESPACE, false);
      preferences.putString("server_url", DEFAULT_SERVER_URL);
      preferences.end();
    } else {
      cfg.serverUrl = pServerUrl;
    }
    cfg.chaveCrypto = (pChave.length() >= 16) ? pChave.substring(0, 16) : DEFAULT_CRYPTO_KEY;
    cfg.valida = true;
    Serial.println("Configuracao carregada da memoria interna (NVS) com sucesso.");
    return true;
  }

  // 2. Se nao encontrou no NVS, tenta carregar do cartao SD (compatibilidade legado)
  if (sdOK && SD.exists(CONFIG_FILE)) {
    File f = SD.open(CONFIG_FILE, FILE_READ);
    if (f) {
      while (f.available()) {
        String linha = f.readStringUntil('\n');
        linha.trim();
        int idx = linha.indexOf('=');
        if (idx < 0) continue;
        String chave = linha.substring(0, idx);
        String valor = linha.substring(idx + 1);
        valor.trim();

        if (chave == "modo")           cfg.modo = valor;
        else if (chave == "ssid")      cfg.ssid = valor;
        else if (chave == "senha")     cfg.senha = valor;
        else if (chave == "intervalo") cfg.intervaloMin = valor.toInt();
        else if (chave == "estacao")   cfg.estacao = valor;
        else if (chave == "server_url") cfg.serverUrl = valor;
        else if (chave == "chave_crypto") cfg.chaveCrypto = valor;
      }
      f.close();

      cfg.modo = "WIFI";
      if (cfg.intervaloMin != 1 && cfg.intervaloMin != 5 &&
          cfg.intervaloMin != 10 && cfg.intervaloMin != 15) cfg.intervaloMin = 5;
      if (!nomeEstacaoValido(cfg.estacao)) cfg.estacao = "G00001";
      if (cfg.serverUrl.length() == 0) cfg.serverUrl = DEFAULT_SERVER_URL;
      if (cfg.chaveCrypto.length() < 16) cfg.chaveCrypto = DEFAULT_CRYPTO_KEY;

      // Migra para NVS
      salvarConfig();
      cfg.valida = true;
      Serial.println("Configuracao importada do SD para NVS com sucesso.");
      return true;
    }
  }

  Serial.println("AVISO: Nenhuma configuracao previa encontrada. Modo AP sera solicitado.");
  return false;
}

bool salvarConfig() {
  // 1. Grava na memoria interna NVS (garantido mesmo sem cartao SD)
  preferences.begin(PREF_NAMESPACE, false);
  preferences.putString("ssid", cfg.ssid);
  preferences.putString("senha", cfg.senha);
  preferences.putString("estacao", cfg.estacao);
  preferences.putInt("intervalo", cfg.intervaloMin);
  preferences.putString("server_url", cfg.serverUrl);
  preferences.putString("chave_crypto", cfg.chaveCrypto);
  preferences.end();

  // 2. Espelha no cartao SD caso esteja inserido e operacional
  if (sdOK) {
    SD.remove(CONFIG_FILE);
    File f = SD.open(CONFIG_FILE, FILE_WRITE);
    if (f) {
      f.printf("modo=%s\n", cfg.modo.c_str());
      f.printf("ssid=%s\n", cfg.ssid.c_str());
      f.printf("senha=%s\n", cfg.senha.c_str());
      f.printf("intervalo=%d\n", cfg.intervaloMin);
      f.printf("estacao=%s\n", cfg.estacao.c_str());
      f.printf("server_url=%s\n", cfg.serverUrl.c_str());
      f.printf("chave_crypto=%s\n", cfg.chaveCrypto.c_str());
      f.close();
      Serial.println("Configuracao espelhada no cartao SD com sucesso.");
    }
  }

  cfg.valida = true;
  return true;
}

void scanWifiNetworks() {
  esp_task_wdt_reset();
  Serial.println("Escaneando redes Wi-Fi proximas...");
  WiFi.mode(WIFI_AP_STA);
  int n = WiFi.scanNetworks(false, true);
  esp_task_wdt_reset();
  Serial.printf("Escaneamento concluido: %d rede(s) encontrada(s).\n", n);
  redesScanHtml = "";
  if (n <= 0) {
    redesScanHtml = "<option value=\"\">Nenhuma rede Wi-Fi encontrada</option>";
    WiFi.scanDelete();
    return;
  }

  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    ssid.trim();
    if (ssid.length() == 0) continue;

    bool duplicate = false;
    for (int j = 0; j < i; j++) {
      if (WiFi.SSID(j) == ssid) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) continue;

    int32_t rssi = WiFi.RSSI(i);
    String auth = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Aberta" : "Protegida";
    String selected = (ssid == cfg.ssid) ? "selected" : "";
    redesScanHtml += "<option value=\"" + ssid + "\" " + selected + ">" +
                     ssid + " (" + String(rssi) + " dBm, " + auth + ")</option>";
  }

  WiFi.scanDelete();
}

void tratarScan() {
  scanWifiNetworks();
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

void tratarApiSensores() {
  Leituras l = lerSensores();
  #if ARDUINOJSON_VERSION_MAJOR >= 7
    JsonDocument doc;
  #else
    StaticJsonDocument<1024> doc;
  #endif

  doc["estacao"] = cfg.estacao;
  doc["dht_ok"] = dhtOK;
  doc["t_dht"] = isnan(l.tDht) ? (char*)nullptr : String(l.tDht, 1);
  doc["umidade"] = isnan(l.umidade) ? (char*)nullptr : String(l.umidade, 1);

  doc["bmp_ok"] = bmpOK;
  doc["t_bmp"] = isnan(l.tBmp) ? (char*)nullptr : String(l.tBmp, 1);
  doc["pressao"] = isnan(l.pressao) ? (char*)nullptr : String(l.pressao, 1);

  doc["veml_ok"] = vemlOK;
  doc["lux"] = isnan(l.lux) ? (char*)nullptr : String(l.lux, 1);

  doc["ds18_ok"] = ds18OK;
  doc["t_solo"] = isnan(l.tSolo) ? (char*)nullptr : String(l.tSolo, 1);

  doc["solo_raw"] = analogRead(SOLO_PIN);
  doc["solo_pct"] = l.solo;

  // Calculos e Diagnosticos Agrometeorologicos
  doc["po"] = isnan(l.po) ? (char*)nullptr : String(l.po, 1);
  doc["vpd"] = isnan(l.vpd) ? (char*)nullptr : String(l.vpd, 2);
  String vpdDesc = "Normal";
  if (!isnan(l.vpd)) {
    if (l.vpd < 0.4) vpdDesc = "Estomatos fechados (Baixa transp.)";
    else if (l.vpd <= 1.4) vpdDesc = "Transpiracao ideal";
    else vpdDesc = "Estresse hidrico severo";
  }
  doc["vpd_desc"] = vpdDesc;

  doc["et0"] = isnan(l.et0) ? (char*)nullptr : String(l.et0, 2);
  doc["etc"] = isnan(l.etc) ? (char*)nullptr : String(l.etc, 2);

  doc["rs"] = isnan(l.rs) ? (char*)nullptr : String(l.rs, 1);
  doc["par"] = isnan(l.par) ? (char*)nullptr : String(l.par, 1);

  doc["rf"] = l.riscoFungo;
  doc["rf_desc"] = (l.riscoFungo == 2) ? "ALTO RISCO (Esporulacao)" : ((l.riscoFungo == 1) ? "Moderado" : "Baixo");
  doc["horas_molhadas"] = String(l.horasMolhadas, 1);

  doc["rg"] = l.riscoGeada;
  doc["rg_desc"] = (l.riscoGeada == 2) ? "GEADA NEGRA (Perigo)" : ((l.riscoGeada == 1) ? "Geada Branca" : "Sem Risco");

  doc["dp"] = String(l.deltaP, 1);
  doc["at"] = l.alertaTempestade;
  doc["dp_desc"] = l.alertaTempestade ? "ALERTA TEMPESTADE (Queda brusca)" : (l.deltaP > 1.0 ? "Tempo Bom / Alta" : "Estavel");

  doc["itu"] = isnan(l.itu) ? (char*)nullptr : String(l.itu, 1);
  String ituDesc = "Conforto";
  if (!isnan(l.itu)) {
    if (l.itu >= 79) ituDesc = "Critico / Emergencia";
    else if (l.itu >= 72) ituDesc = "Alerta / Estresse";
    else ituDesc = "Conforto Termico";
  }
  doc["itu_desc"] = ituDesc;
  doc["hi"] = isnan(l.hi) ? (char*)nullptr : String(l.hi, 1);

  doc["qualidade"] = l.qualidade;

  doc["sd_ok"] = sdOK;
  bool timeValido = isTimestampValido();
  uint32_t tsAtual = obterTimestamp();
  doc["rtc_ok"] = timeValido;
  doc["relogio_tipo"] = "Atomico (NTP)";
  doc["rtc_str"] = timeValido ? timestampFormatado(tsAtual) : (ntpSincronizado ? "Sincronizado NTP" : "Aguardando Sincronizacao NTP");
  doc["rtc_ts"] = tsAtual;

  doc["heap"] = ESP.getFreeHeap();
  doc["uptime"] = millis() / 1000;

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

String paginaConfigHTML() {
  if (redesScanHtml.length() == 0) {
    scanWifiNetworks();
  }

  String html = R"raw(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Setup - Projeto GAIA TX</title>
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
      --primary-blue: #3b82f6;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: 'Outfit', sans-serif;
      background-color: var(--bg-color);
      background-image: radial-gradient(circle at top right, rgba(16, 185, 129, 0.10), transparent 45%),
                        radial-gradient(circle at bottom left, rgba(59, 130, 246, 0.08), transparent 45%);
      color: var(--text-color);
      display: flex;
      justify-content: center;
      align-items: center;
      min-height: 100vh;
      padding: 16px;
    }
    .container {
      width: 100%;
      max-width: 520px;
      background: var(--card-bg);
      backdrop-filter: blur(16px);
      -webkit-backdrop-filter: blur(16px);
      border: 1px solid var(--card-border);
      border-radius: 24px;
      padding: 28px 24px;
      box-shadow: 0 10px 40px rgba(0, 0, 0, 0.6);
    }
    h2 {
      font-weight: 700;
      font-size: 1.8rem;
      margin-bottom: 6px;
      background: linear-gradient(135deg, #10b981, #3b82f6);
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
      flex-wrap: wrap;
    }
    .chip {
      background: rgba(16, 185, 129, 0.12);
      border: 1px solid rgba(16, 185, 129, 0.3);
      color: #34d399;
      font-size: 0.75rem;
      font-weight: 600;
      padding: 5px 12px;
      border-radius: 999px;
    }
    .diag-card {
      background: rgba(0, 0, 0, 0.25);
      border: 1px solid var(--card-border);
      border-radius: 16px;
      padding: 16px;
      margin-bottom: 24px;
    }
    .diag-title {
      font-size: 0.85rem;
      font-weight: 700;
      color: #34d399;
      text-transform: uppercase;
      letter-spacing: 0.05em;
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 12px;
    }
    .diag-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 8px;
      font-size: 0.82rem;
    }
    .diag-item {
      background: rgba(255, 255, 255, 0.02);
      padding: 8px 10px;
      border-radius: 8px;
      border-left: 3px solid var(--primary-blue);
    }
    .diag-item.ok { border-left-color: #10b981; }
    .diag-item.warn { border-left-color: #f59e0b; }
    .diag-item.err { border-left-color: #ef4444; }
    .diag-lbl { color: var(--text-muted); font-size: 0.72rem; display: block; }
    .diag-val { font-weight: 600; color: #f9fafb; }
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
      box-shadow: 0 0 10px rgba(16, 185, 129, 0.25);
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
      box-shadow: 0 6px 18px rgba(16, 185, 129, 0.35);
    }
    .btn-secondary {
      background: rgba(255, 255, 255, 0.08);
      color: #f3f4f6;
      font-size: 0.82rem;
      padding: 8px 12px;
      border-radius: 8px;
      width: auto;
      margin: 0;
    }
    .btn-secondary:hover { background: rgba(255, 255, 255, 0.15); }
    .btn-danger {
      background: rgba(239, 68, 68, 0.12);
      border: 1px solid rgba(239, 68, 68, 0.3);
      color: var(--danger-color);
    }
    .btn-danger:hover {
      background: var(--danger-color);
      color: #fff;
    }
    hr {
      border: 0;
      height: 1px;
      background: var(--card-border);
      margin: 24px 0;
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
    <h2>Projeto GAIA - TX</h2>
    <p class="subtitle">Estação Agroclimática de Campo</p>

    <div class="chips">
      <span class="chip">AP: %AP_NOME%</span>
      <span class="chip">IP: 192.168.4.1</span>
      <span class="chip">Portal Captivo Ativo</span>
    </div>

    <!-- Painel de Telemetria e Diagnóstico dos Sensores -->
    <div class="diag-card">
      <div class="diag-title">
        <span>Diagnóstico em Tempo Real</span>
        <button type="button" class="btn-secondary" onclick="carregarDiagnostico()">Atualizar</button>
      </div>
      <div class="diag-grid">
        <div class="diag-item" id="diag_dht"><span class="diag-lbl">DHT22 (Ar)</span><span class="diag-val" id="v_dht">Lendo...</span></div>
        <div class="diag-item" id="diag_bmp"><span class="diag-lbl">BMP280 (Pressao)</span><span class="diag-val" id="v_bmp">Lendo...</span></div>
        <div class="diag-item" id="diag_veml"><span class="diag-lbl">VEML7700 (Luz)</span><span class="diag-val" id="v_veml">Lendo...</span></div>
        <div class="diag-item" id="diag_ds18"><span class="diag-lbl">DS18B20 (Solo)</span><span class="diag-val" id="v_ds18">Lendo...</span></div>
        <div class="diag-item" id="diag_solo"><span class="diag-lbl">Umidade Solo (ADC)</span><span class="diag-val" id="v_solo">Lendo...</span></div>
        <div class="diag-item" id="diag_vpd"><span class="diag-lbl">Orvalho / VPD</span><span class="diag-val" id="v_vpd">Lendo...</span></div>
        <div class="diag-item" id="diag_et"><span class="diag-lbl">Evapotranspiracao (ET0/ETc)</span><span class="diag-val" id="v_et">Lendo...</span></div>
        <div class="diag-item" id="diag_solar"><span class="diag-lbl">Radiacao Solar / PAR</span><span class="diag-val" id="v_solar">Lendo...</span></div>
        <div class="diag-item" id="diag_fito"><span class="diag-lbl">Fitossanidade (Fungos)</span><span class="diag-val" id="v_fito">Lendo...</span></div>
        <div class="diag-item" id="diag_geada"><span class="diag-lbl">Risco de Geada</span><span class="diag-val" id="v_geada">Lendo...</span></div>
        <div class="diag-item" id="diag_baro"><span class="diag-lbl">Barometria / Clima</span><span class="diag-val" id="v_baro">Lendo...</span></div>
        <div class="diag-item" id="diag_itu"><span class="diag-lbl">Zootecnia (ITU / HI)</span><span class="diag-val" id="v_itu">Lendo...</span></div>
        <div class="diag-item" id="diag_rtc"><span class="diag-lbl">Relogio Atomico (NTP)</span><span class="diag-val" id="v_rtc">Lendo...</span></div>
        <div class="diag-item" id="diag_sys"><span class="diag-lbl">Cartao SD / RAM</span><span class="diag-val" id="v_sys">Lendo...</span></div>
      </div>
    </div>

    <form action="/salvar" method="POST" onsubmit="return prepararForm()">
      <input type="hidden" name="browser_ts" id="browser_ts">

      <div class="form-group">
        <label for="ssid_list">Redes Wi-Fi Detectadas</label>
        <div class="input-row">
          <select id="ssid_list" onchange="selecionarRede()">
            <option value="">Selecione uma rede ou digite abaixo...</option>
            %REDES_WIFI%
          </select>
          <a href="/scan" class="btn btn-secondary" style="text-decoration:none; display:flex; align-items:center;">Escanear</a>
        </div>
      </div>

      <div class="form-group">
        <label for="ssid">SSID do Roteador / Access Point</label>
        <input type="text" id="ssid" name="ssid" value="%SSID%" required placeholder="Nome da sua rede Wi-Fi">
      </div>

      <div class="form-group">
        <label for="senha">Senha do Wi-Fi</label>
        <div class="input-row">
          <input type="password" id="senha" name="senha" value="%SENHA%" placeholder="Deixe em branco se for aberta">
          <button type="button" class="btn-secondary" onclick="toggleSenha()">Ver</button>
        </div>
      </div>

      <div class="form-group">
        <label for="estacao">Código da Estação (6 caracteres)</label>
        <input type="text" id="estacao" name="estacao" value="%ESTACAO%" maxlength="6" required placeholder="Ex: G00001">
        <div class="hint">Formato padrão: 1 letra seguida de 5 dígitos (ex: G00001).</div>
      </div>

      <div class="form-group">
        <label for="intervalo">Intervalo de Transmissão</label>
        <select id="intervalo" name="intervalo">
          <option value="1" %INT_1%>1 minuto (Modo Teste)</option>
          <option value="5" %INT_5%>5 minutos (Padrão)</option>
          <option value="10" %INT_10%>10 minutos</option>
          <option value="15" %INT_15%>15 minutos (Econômico)</option>
        </select>
      </div>

      <div class="form-group">
        <label for="server_url">Endpoint do Servidor HTTP / HTTPS</label>
        <div style="display:flex; gap:8px; margin-bottom:8px;">
          <button type="button" class="btn-secondary" style="flex:1;" onclick="document.getElementById('server_url').value='http://192.168.0.105:3000/api/telemetria'">🏠 Rede Local</button>
          <button type="button" class="btn-secondary" style="flex:1;" onclick="document.getElementById('server_url').value='https://server1.taila7d06b.ts.net/api/telemetria'">☁️ Remoto Fixo</button>
        </div>
        <input type="text" id="server_url" name="server_url" value="%SERVER_URL%" required placeholder="https://server1.taila7d06b.ts.net/api/telemetria">
        <div class="hint">URL fixa de recebimento de telemetria REST do Projeto GAIA.</div>
      </div>

      <div class="form-group">
        <label for="chave_crypto">Chave Criptográfica AES-128 (16 caracteres)</label>
        <input type="text" id="chave_crypto" name="chave_crypto" value="%CHAVE%" minlength="16" maxlength="16" required placeholder="GaiaAgroIoT2026!">
        <div class="hint">Chave de 16 caracteres para cifragem simétrica (Padrão: GaiaAgroIoT2026!).</div>
      </div>

      <button type="submit" class="btn btn-primary">Salvar Configurações e Iniciar</button>
    </form>

    <hr>

    <div style="display:grid; grid-template-columns: 1fr 1fr; gap:10px;">
      <form action="/formatar" method="POST" onsubmit="return confirm('ATENCAO: Deseja realmente apagar todos os dados e fila do cartao SD?');">
        <button type="submit" class="btn btn-danger" style="margin-top:0;">Formatar SD</button>
      </form>
      <form action="/reiniciar" method="POST">
        <button type="submit" class="btn btn-secondary" style="margin-top:0; width:100%;">Reiniciar</button>
      </form>
    </div>
  </div>

  <script>
    function selecionarRede() {
      var s = document.getElementById('ssid_list').value;
      if (s) {
        document.getElementById('ssid').value = s;
      }
    }
    function toggleSenha() {
      var p = document.getElementById('senha');
      p.type = (p.type === 'password') ? 'text' : 'password';
    }
    function prepararForm() {
      var key = document.getElementById('chave_crypto').value;
      if (key.length !== 16) {
        alert('Erro: A chave criptográfica deve ter exatamente 16 caracteres!');
        return false;
      }
      var est = document.getElementById('estacao').value.trim();
      if (est.length !== 6) {
        alert('Erro: O código da estação deve ter exatamente 6 caracteres (Ex: G00001)!');
        return false;
      }
      document.getElementById('browser_ts').value = Math.floor(Date.now() / 1000);
      return true;
    }
    function carregarDiagnostico() {
      fetch('/api/sensores')
        .then(function(res) { return res.json(); })
        .then(function(d) {
          document.getElementById('v_dht').innerText = d.dht_ok ? (d.t_dht + '°C | ' + d.umidade + '%') : 'OFFLINE';
          document.getElementById('diag_dht').className = 'diag-item ' + (d.dht_ok ? 'ok' : 'err');

          document.getElementById('v_bmp').innerText = d.bmp_ok ? (d.t_bmp + '°C | ' + d.pressao + ' hPa') : 'OFFLINE';
          document.getElementById('diag_bmp').className = 'diag-item ' + (d.bmp_ok ? 'ok' : 'err');

          document.getElementById('v_veml').innerText = d.veml_ok ? (d.lux + ' lx') : 'OFFLINE';
          document.getElementById('diag_veml').className = 'diag-item ' + (d.veml_ok ? 'ok' : 'err');

          document.getElementById('v_ds18').innerText = d.ds18_ok ? (d.t_solo + '°C') : 'OFFLINE';
          document.getElementById('diag_ds18').className = 'diag-item ' + (d.ds18_ok ? 'ok' : 'err');

          document.getElementById('v_solo').innerText = (d.solo_pct >= 0) ? (d.solo_pct + '% (ADC: ' + d.solo_raw + ')') : 'OFF / S/Leitura';
          document.getElementById('diag_solo').className = 'diag-item ' + (d.solo_pct >= 0 ? 'ok' : 'err');

          document.getElementById('v_vpd').innerText = 'PO: ' + (d.po || '--') + '°C | VPD: ' + (d.vpd || '--') + ' kPa (' + (d.vpd_desc || '') + ')';
          var vpdClass = 'ok';
          if (d.vpd && (parseFloat(d.vpd) > 1.4 || parseFloat(d.vpd) < 0.4)) vpdClass = 'warn';
          document.getElementById('diag_vpd').className = 'diag-item ' + vpdClass;

          document.getElementById('v_et').innerText = 'ET0: ' + (d.et0 || '--') + ' mm/d | ETc: ' + (d.etc || '--') + ' mm';
          document.getElementById('diag_et').className = 'diag-item ok';

          document.getElementById('v_solar').innerText = 'Rs: ' + (d.rs || '--') + ' W/m² | PAR: ' + (d.par || '--') + ' µmol';
          document.getElementById('diag_solar').className = 'diag-item ok';

          document.getElementById('v_fito').innerText = d.rf_desc + ' (' + d.horas_molhadas + 'h molhadas)';
          var fitoClass = (d.rf === 2) ? 'err' : (d.rf === 1 ? 'warn' : 'ok');
          document.getElementById('diag_fito').className = 'diag-item ' + fitoClass;

          document.getElementById('v_geada').innerText = d.rg_desc;
          var geadaClass = (d.rg === 2) ? 'err' : (d.rg === 1 ? 'warn' : 'ok');
          document.getElementById('diag_geada').className = 'diag-item ' + geadaClass;

          document.getElementById('v_baro').innerText = 'ΔP: ' + (parseFloat(d.dp) > 0 ? '+' : '') + d.dp + ' hPa | ' + d.dp_desc;
          document.getElementById('diag_baro').className = 'diag-item ' + (d.at ? 'err' : 'ok');

          document.getElementById('v_itu').innerText = 'ITU: ' + (d.itu || '--') + ' (' + d.itu_desc + ') | HI: ' + (d.hi || '--') + '°C';
          var ituClass = (d.itu && parseFloat(d.itu) >= 79) ? 'err' : ((d.itu && parseFloat(d.itu) >= 72) ? 'warn' : 'ok');
          document.getElementById('diag_itu').className = 'diag-item ' + ituClass;

          document.getElementById('v_rtc').innerText = d.rtc_str;
          document.getElementById('diag_rtc').className = 'diag-item ' + (d.rtc_ok ? 'ok' : 'err');

          document.getElementById('v_sys').innerText = (d.sd_ok ? 'SD OK' : 'SD ERRO') + ' | ' + Math.round(d.heap / 1024) + ' KB Livre';
          document.getElementById('diag_sys').className = 'diag-item ' + (d.sd_ok ? 'ok' : 'err');
        })
        .catch(function(e) {
          console.log('Erro ao ler sensores:', e);
        });
    }
    carregarDiagnostico();
    setInterval(carregarDiagnostico, 5000);
  </script>
</body>
</html>
)raw";

  String apNome = "GAIA-TX-" + String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF), HEX);
  apNome.toUpperCase();

  html.replace("%AP_NOME%", apNome);
  html.replace("%ESTACAO%", cfg.estacao);
  html.replace("%SSID%", cfg.ssid);
  html.replace("%SENHA%", cfg.senha);
  html.replace("%REDES_WIFI%", redesScanHtml);
  html.replace("%SERVER_URL%", cfg.serverUrl);
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
  estacao.trim();

  if (!nomeEstacaoValido(estacao)) {
    server.send(400, "text/plain", "Codigo de estacao invalido. Deve conter 6 caracteres (1 letra e 5 digitos, ex: G00001).");
    return;
  }

  String ssid = server.arg("ssid");
  ssid.trim();
  String senha = server.arg("senha");
  senha.trim();
  String sUrl = server.arg("server_url");
  sUrl.trim();
  String chave = server.arg("chave_crypto");
  chave.trim();
  int intervalo = server.arg("intervalo").toInt();

  if (chave.length() != 16) {
    server.send(400, "text/plain", "A chave criptografica deve ter exatamente 16 caracteres!");
    return;
  }

  cfg.estacao      = estacao;
  cfg.modo         = "WIFI";
  cfg.ssid         = ssid;
  cfg.senha        = senha;
  cfg.intervaloMin = (intervalo == 1 || intervalo == 5 || intervalo == 10 || intervalo == 15) ? intervalo : 5;
  cfg.serverUrl    = (sUrl.length() > 0) ? sUrl : DEFAULT_SERVER_URL;
  cfg.chaveCrypto  = chave;

  // Ajuste do relogio interno do ESP32 via navegador do usuario (backup antes do NTP)
  if (server.hasArg("browser_ts")) {
    uint32_t ts = server.arg("browser_ts").toInt();
    if (ts > 1700000000) {
      struct timeval tv = { .tv_sec = (time_t)ts, .tv_usec = 0 };
      settimeofday(&tv, NULL);
      ultimoTsValidado = ts;
      softClockBase = ts;
      softClockMillisBase = millis();
      gravarLogPermanente("INFO", "Relogio interno alinhado via AP com horario do browser: " + timestampFormatado(ts));
    }
  }

  bool ok = salvarConfig();
  server.send(200, "text/plain", ok ? "Configuracao salva com sucesso! Reiniciando a estacao..." :
                                       "Erro ao salvar configuracao.");
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
  esp_task_wdt_reset();
  String apNome = "GAIA-TX-" + String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF), HEX);
  apNome.toUpperCase();

  // Desativa reconexao automatica do STA para nao ficar trocando de canal e derrubando o sinal do AP!
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(true, true);
  delay(100);

  // Escaneia redes antes de fixar o AP para preencher a lista sem interferir no beacon do AP
  esp_task_wdt_reset();
  scanWifiNetworks();
  esp_task_wdt_reset();

  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect(false, true);
  esp_task_wdt_reset();
  WiFi.softAP(apNome.c_str(), "gaiaconfig", 1, 0, 4);
  delay(100);
  IPAddress apIP = WiFi.softAPIP();

  // Inicia servidor DNS na porta 53 para Portal Captivo
  esp_task_wdt_reset();
  dnsServer.start(DNS_PORT, "*", apIP);

  Serial.println("=========================================");
  Serial.println("PORTAL DE CONFIGURACAO ATIVO - GAIA TX");
  Serial.print("SSID do AP: ");
  Serial.println(apNome);
  Serial.println("Senha: gaiaconfig");
  Serial.print("IP: ");
  Serial.println(apIP);
  Serial.println("Acesse: http://192.168.4.1");
  Serial.println("=========================================");

  server.on("/", HTTP_GET, tratarRaiz);
  server.on("/salvar", HTTP_POST, tratarSalvar);
  server.on("/formatar", HTTP_POST, tratarFormatar);
  server.on("/scan", HTTP_GET, tratarScan);
  server.on("/api/sensores", HTTP_GET, tratarApiSensores);
  server.on("/reiniciar", HTTP_POST, []() {
    server.send(200, "text/plain", "Reiniciando transmissor...");
    delay(1000);
    ESP.restart();
  });

  // Endpoints padrao de deteccao de Portal Captivo (Android, iOS/macOS, Windows)
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
  Serial.println("Servidor Web e DNS iniciados com sucesso!");
  Serial.println("Aguardando conexao no portal http://192.168.4.1");

  unsigned long inicioAP = millis();
  
  // Loop de atendimento do Portal Captivo e WebServer
  while (true) {
    dnsServer.processNextRequest();
    server.handleClient();
    esp_task_wdt_reset();
    verificarBotaoFormatar();
    
    // Pisca o LED lentamente (500ms) para indicar AP ativo
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
    delay(5);
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
  esp_task_wdt_reset();
  sdOK = SD.begin(SD_CS);
  if (!sdOK) {
    Serial.println("ERRO: Falha no cartao SD. Rodando sem persistência local.");
  } else {
    Serial.println("Cartao SD inicializado com sucesso.");
  }

  esp_task_wdt_reset();
  bmpOK = bmp.begin(0x76);
  if (!bmpOK) {
    bmpOK = bmp.begin(0x77);
    if (!bmpOK) {
      Serial.println("AVISO: BMP280 nao detectado em 0x76 ou 0x77.");
    } else {
      Serial.printf("BMP280 OK (0x77). Temp: %.2f °C | Pressao: %.1f hPa\n", bmp.readTemperature(), bmp.readPressure() / 100.0);
    }
  } else {
    Serial.printf("BMP280 OK (0x76). Temp: %.2f °C | Pressao: %.1f hPa\n", bmp.readTemperature(), bmp.readPressure() / 100.0);
  }

  esp_task_wdt_reset();
  vemlOK = veml.begin();
  if (!vemlOK) {
    Serial.println("AVISO: VEML7700 nao detectado no I2C.");
  } else {
    Serial.printf("VEML7700 OK. Luminosidade: %.1f lx\n", veml.readLux());
  }

  esp_task_wdt_reset();
  dht.begin();
  // Validacao real da conexao fisica do DHT22
  float tTest = dht.readTemperature();
  float hTest = dht.readHumidity();
  if (isnan(tTest) || isnan(hTest)) {
    dhtOK = false;
    Serial.println("AVISO: Sensor DHT22 desconectado ou inoperante.");
  } else {
    dhtOK = true;
    Serial.printf("DHT22 OK. Temp: %.2f °C | Umidade: %.1f %%\n", tTest, hTest);
  }

  esp_task_wdt_reset();
  // Inicializacao do sensor de solo DS18B20
  ds18Sensors.begin();
  ds18Sensors.requestTemperatures();
  float tSoloTest = ds18Sensors.getTempCByIndex(0);
  if (tSoloTest == DEVICE_DISCONNECTED_C) {
    ds18OK = false;
    Serial.println("AVISO: Sensor DS18B20 nao detectado no barramento 1-Wire.");
  } else {
    ds18OK = true;
    Serial.printf("DS18B20 OK. Temp Solo: %.2f °C\n", tSoloTest);
  }

  // Leitura do sensor de solo analógico
  int rawSolo = analogRead(SOLO_PIN);
  Serial.printf("Umidade Solo Analogica (GPIO 34): Leitura raw = %d\n", rawSolo);
  esp_task_wdt_reset();
}

bool conectarWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.ssid.c_str(), cfg.senha.c_str());

  for (int tentativa = 1; tentativa <= 3; tentativa++) {
    Serial.printf("Tentando WiFi (%s) %d/3\n", cfg.ssid.c_str(), tentativa);
    unsigned long inicio = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - inicio < 3500) {
      if (digitalRead(BTN_AP_PIN) == LOW || digitalRead(BTN_BOOT_PIN) == LOW) {
        Serial.println("Botao AP/BOOT pressionado durante conexao! Abrindo Modo AP...");
        iniciarModoAP();
      }
      delay(150);
      esp_task_wdt_reset();
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi conectado com sucesso.");
      return true;
    }
  }
  Serial.println("AVISO: Falha na conexao WiFi.");
  return false;
}

void sincronizarNTP() {
  esp_task_wdt_reset();
  Serial.println("Sincronizando relogio com servidores atomicos NTP.br...");
  // Fuso de Brasilia: UTC-3 (-10800s). Servidores de estrato 1 com relogios atomicos (NTP.br / Observatorio Nacional)
  configTime(-3 * 3600, 0, "a.st1.ntp.br", "b.st1.ntp.br", "pool.ntp.org");
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 6000)) {
    char buf[32];
    strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &timeinfo);
    time_t agora = time(NULL);
    ultimoTsValidado = (uint32_t)agora;
    ntpSincronizado = true;
    softClockBase = (uint32_t)agora;
    softClockMillisBase = millis();
    Serial.printf("Relogio Atomico OK! Data/Hora: %s (TS: %u)\n", buf, (uint32_t)agora);
    gravarLogPermanente("INFO", "Relogio atomico sincronizado com sucesso via NTP: " + String(buf));
  } else {
    Serial.println("AVISO: Timeout ao sincronizar com servidor NTP. Mantendo relogio interno.");
    gravarLogPermanente("WARN", "Falha ao sincronizar com servidores de relogio atomico.");
  }
  esp_task_wdt_reset();
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
    ok = enviarHTTP(linha, true); // Envia direto (ja criptografado)

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

bool enviarParaEndpoint(const String &url, const String &body) {
  HTTPClient http;
  bool isHttps = url.startsWith("https://");
  WiFiClientSecure secureClient;
  WiFiClient regularClient;

  Serial.printf("[HTTP] Conectando ao servidor: %s (%s)\n", url.c_str(), isHttps ? "HTTPS" : "HTTP");

  if (isHttps) {
    secureClient.setInsecure(); // Permite SSL/TLS sem necessidade de certificado raiz embutido
    http.begin(secureClient, url);
  } else {
    http.begin(regularClient, url);
  }

  http.setConnectTimeout(4000);
  http.setTimeout(8000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Station-Token", GAIA_PROJECT_TOKEN);

  int code = http.POST(body);
  String response = http.getString();
  Serial.printf("[HTTP] Codigo de Resposta: %d (%s)\n", code, http.errorToString(code).c_str());
  if (code > 0) {
    Serial.printf("[HTTP] Corpo da Resposta: %s\n", response.c_str());
  }
  http.end();

  return (code >= 200 && code < 300);
}

bool enviarHTTP(const String &payload, bool isEncrypted = false) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[HTTP] Erro: Wi-Fi desconectado ao tentar enviar telemetria.");
    return false;
  }

  String toSend = payload;
  if (!isEncrypted) {
    if (!encryptAES(payload, cfg.chaveCrypto, toSend)) {
      gravarLogPermanente("ERRO", "Falha ao encriptar payload HTTP.");
      Serial.println("[HTTP] Erro: Falha na encriptacao AES-128 do payload.");
      return false;
    }
  }

  // Prepara envelope JSON: {"data": "Base64EncryptedText"}
  #if ARDUINOJSON_VERSION_MAJOR >= 7
    JsonDocument doc;
  #else
    StaticJsonDocument<512> doc;
  #endif
  doc["data"] = toSend;
  String body;
  serializeJson(doc, body);

  // 1. Tenta envio primário diretamente no servidor da rede local
  Serial.println("[HTTP] [Tentativa 1] Enviando para servidor local...");
  if (enviarParaEndpoint(LOCAL_SERVER_URL, body)) {
    Serial.println("[HTTP] Sucesso via servidor local (192.168.0.105:3000)!");
    return true;
  }

  // 2. Fallback: Se o servidor local não responder, tenta a URL remota (túnel Cloudflare)
  Serial.println("[HTTP] [Tentativa 2] Servidor local nao respondeu. Enviando via tunel remoto...");
  String urlRemota = (cfg.serverUrl.length() > 0) ? cfg.serverUrl : DEFAULT_SERVER_URL;
  if (enviarParaEndpoint(urlRemota, body)) {
    Serial.println("[HTTP] Sucesso via tunel remoto Cloudflare!");
    return true;
  }

  Serial.println("[HTTP] Falha no envio para ambos os servidores.");
  return false;
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

  // Cruza as duas temperaturas do ar. Divergencia alta reduz a confianca
  // e usa o DHT22 como referencia de umidade relativa.
  float tempRef = NAN;
  float deltaTemp = NAN;
  if (!isnan(l.tDht) && !isnan(l.tBmp)) {
    deltaTemp = fabs(l.tDht - l.tBmp);
    tempRef = (deltaTemp <= 5.0) ? (l.tDht + l.tBmp) / 2.0 : l.tDht;
  } else if (!isnan(l.tDht)) {
    tempRef = l.tDht;
  } else {
    tempRef = l.tBmp;
  }

  // Pontuacao simples para impedir que o servidor trate leitura incompleta
  // como uma medicao plenamente confiavel.
  int leiturasValidas = 0;
  if (!isnan(l.tDht)) leiturasValidas++;
  if (!isnan(l.tBmp)) leiturasValidas++;
  if (!isnan(l.umidade)) leiturasValidas++;
  if (!isnan(l.pressao)) leiturasValidas++;
  if (!isnan(l.lux)) leiturasValidas++;
  if (!isnan(l.tSolo)) leiturasValidas++;
  if (l.solo >= 0) leiturasValidas++;
  l.qualidade = (leiturasValidas * 100) / 7;
  if (!isnan(deltaTemp) && deltaTemp > 5.0) l.qualidade -= 15;
  if (l.qualidade < 0) l.qualidade = 0;
  l.tempRef = tempRef;
  l.deltaTemp = deltaTemp;

  l.po = calcularPontoOrvalho(tempRef, l.umidade);
  l.vpd = calcularVPD(tempRef, l.umidade);
  l.et0 = calcularET0(tempRef, l.umidade, l.pressao, l.lux, cfg.intervaloMin);
  l.etc = isnan(l.et0) ? NAN : (l.et0 * 1.0f); // Kc padrao 1.0

  calcularRadiacao(l.lux, l.rs, l.par);
  calcularBioclima(tempRef, l.umidade, l.itu, l.hi);
  avaliarFitossanidade(tempRef, l.umidade, l.po, cfg.intervaloMin, l.riscoFungo, l.horasMolhadas, l.riscoGeada);
  avaliarTendenciaBarometrica(l.pressao, l.deltaP, l.alertaTempestade);

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
  #if ARDUINOJSON_VERSION_MAJOR >= 7
    JsonDocument doc;
  #else
    StaticJsonDocument<1024> doc;
  #endif
  doc["codSta"] = cfg.estacao;
  doc["token"]  = GAIA_PROJECT_TOKEN;
  doc["ts"] = ts;

  if (!isnan(l.tBmp))    doc["tB"] = l.tBmp; else doc["tB"] = nullptr;
  if (!isnan(l.pressao)) doc["p"]  = l.pressao; else doc["p"] = nullptr;
  if (!isnan(l.lux))     doc["l"]  = l.lux; else doc["l"] = nullptr;
  if (!isnan(l.umidade)) doc["h"]  = l.umidade; else doc["h"] = nullptr;
  if (!isnan(l.tDht))    doc["tD"] = l.tDht; else doc["tD"] = nullptr;
  if (l.solo >= 0)       doc["s"]  = l.solo; else doc["s"] = nullptr;
  if (!isnan(l.tSolo))   doc["tS"] = l.tSolo; else doc["tS"] = nullptr; 
  if (!isnan(l.tempRef)) doc["tRef"] = l.tempRef; else doc["tRef"] = nullptr;
  if (!isnan(l.deltaTemp)) doc["dT"] = l.deltaTemp; else doc["dT"] = nullptr;
  doc["q"] = l.qualidade;
  
  // Variaveis calculadas enviadas ao servidor/receptor
  if (!isnan(l.po))      doc["po"]  = l.po;  else doc["po"]  = nullptr;
  if (!isnan(l.vpd))     doc["vpd"] = l.vpd; else doc["vpd"] = nullptr;
  if (!isnan(l.et0))     doc["et0"] = l.et0; else doc["et0"] = nullptr;
  if (!isnan(l.etc))     doc["etc"] = l.etc; else doc["etc"] = nullptr;
  if (!isnan(l.rs))      doc["rs"]  = l.rs;  else doc["rs"]  = nullptr;
  if (!isnan(l.par))     doc["par"] = l.par; else doc["par"] = nullptr;
  if (!isnan(l.itu))     doc["itu"] = l.itu; else doc["itu"] = nullptr;
  if (!isnan(l.hi))      doc["hi"]  = l.hi;  else doc["hi"]  = nullptr;
  doc["rf"] = l.riscoFungo;
  doc["rg"] = l.riscoGeada;
  doc["dp"] = l.deltaP;
  doc["at"] = l.alertaTempestade;

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

  // Registra leitura com as novas metricas calculadas no log local do SD
  char logMsg[160];
  snprintf(logMsg, sizeof(logMsg), "Leitura - Temp:%.1fC Umid:%.1f%% VPD:%.2fkPa ET0:%.2fmm Rs:%.0fW/m2 ITU:%.1f Fungo:%d Geada:%d", 
           isnan(l.tDht) ? l.tBmp : l.tDht, l.umidade, l.vpd, l.et0, isnan(l.rs) ? 0.0f : l.rs, isnan(l.itu) ? 0.0f : l.itu, l.riscoFungo, l.riscoGeada);
  gravarLogPermanente("INFO", String(logMsg));

  int reenviados = 0, pendentes = 0;
  processarFila(reenviados, pendentes);

  bool ok = enviarHTTP(payload, false);
  if (ok) {
    salvarLastAck(ts);
    gravarLogPermanente("SUCESSO", "Pacote WiFi/HTTP enviado com sucesso.");
    piscarLED(1, 400, 0); // Piscada de sucesso
  } else {
    enfileirar(payload);
    gravarLogPermanente("FALHA", "Erro WiFi/HTTP. Pacote salvo na fila offline.");
    piscarLED(3, 100, 100); // 3 piscadas rápidas de erro
  }
}

void configurarWatchdog() {
  #if ESP_ARDUINO_VERSION_MAJOR >= 3
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 45000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .trigger_panic = true
    };
    if (esp_task_wdt_reconfigure(&wdt_config) != ESP_OK) {
      esp_task_wdt_init(&wdt_config);
    }
  #else
    esp_task_wdt_init(45, true);
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

  configurarWatchdog();
  esp_task_wdt_reset();

  // Define timeout I2C para prevenir travamentos nos sensores
  Wire.begin(21, 22);
  Wire.setTimeOut(100); 

  pinMode(BTN_FORMAT_PIN, INPUT_PULLUP);
  pinMode(BTN_AP_PIN, INPUT_PULLUP);
  pinMode(BTN_BOOT_PIN, INPUT_PULLUP);

  iniciarSensores();
  esp_task_wdt_reset();

  // Pisca o LED uma vez para sinalizar que o ESP32 acordou/iniciou
  piscarLED(1, 200, 0);

  bool configOk = carregarConfig();
  esp_task_wdt_reset();

  bool forcarAP = (digitalRead(BTN_AP_PIN) == LOW || digitalRead(BTN_BOOT_PIN) == LOW);

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
    } else {
      esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
      if (wakeup_reason != ESP_SLEEP_WAKEUP_TIMER || !sdOK) {
        Serial.println("Rede Wi-Fi salva nao encontrada ao ligar! Abrindo Modo AP automaticamente...");
        iniciarModoAP();
      }
    }
  }

  // Realiza a leitura e o envio
  realizarEnvio();

  // Calculo de alinhamento com a grade de tempo (Relogio Atomico / NTP) + offset do dispositivo
  uint32_t segundosParaDormir = 60; // Fallback
  time_t agora = time(NULL);
  if (agora > 1700000000) {
    struct tm *tmInfo = localtime(&agora);
    uint32_t minutoAtual = tmInfo->tm_min;
    uint32_t segundoAtual = tmInfo->tm_sec;
    
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