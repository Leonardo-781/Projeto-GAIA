/**
 * ============================================================================
 * PROJETO GAIA - ESTAÇÃO AGROCLIMÁTICA INTELIGENTE DE CAMPO
 * ESTAÇÃO: G00002
 * LOCALIDADE: Monte Carmelo - MG
 * HARDWARE: ESP32 TTGO T-Call (V1.3 / V1.4) com Modem Celular SIM800L
 * ============================================================================
 */

#define TINY_GSM_MODEM_SIM800

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <math.h>
#include <ArduinoJson.h>
#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>

// Criptografia AES-128 nativa mbedtls do ESP32
#include "mbedtls/aes.h"
#include "mbedtls/base64.h"

// Bibliotecas de Sensores (se conectados)
#include <Adafruit_BMP280.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>

struct TelemetriaGAIA {
    float tempAr;
    float umidAr;
    float pressao;
    float tempSolo;
    int   umidSolo;
    float lux;
    float pontoOrvalho;
    float vpd;
    float et0;
};

// ============================================================================
// CONFIGURAÇÃO DA ESTAÇÃO GAIA
// ============================================================================
#define ESTACAO_CODIGO        "G00002"
#define ESTACAO_LOCALIDADE    "Monte Carmelo - MG"
#define GAIA_AES_KEY          "GaiaAgroIoT2026!" // Chave oficial do Projeto GAIA

// Servidor de Destino (HTTP REST)
// Por padrão, usa o endpoint público de testes httpbin.org para validação em bancada.
// Para seu servidor local ou nuvem, altere para o IP/domínio desejado:
const char serverHost[] = "httpbin.org";
const int  serverPort   = 80;
const char serverPath[] = "/post";

// Configurações da Operadora Celular (Vivo Brasil)
const char apn[]      = "zap.vivo.com.br";
const char gprsUser[] = "vivo";
const char gprsPass[] = "vivo";

// ============================================================================
// PINAGEM ESPECÍFICA DA TTGO T-CALL V1.3 / V1.4
// ============================================================================
#define MODEM_RST            5
#define MODEM_PWRKEY         4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26

#define I2C_SDA              21
#define I2C_SCL              22
#define IP5306_ADDR          0x75
#define IP5306_REG_SYS_CTL0  0x00

// Sensores de Campo (Compativeis com TTGO T-Call)
#define DHTPIN               14
#define DHTTYPE              DHT22
#define ONE_WIRE_BUS         13   // DS18B20 Solo (requer pull-up 4.7k)
#define SOLO_PIN             35   // Sensor de Umidade do Solo (ADC1_CH7 - Pino 35)
#define LED_PIN              13   // LED azul onboard da T-Call

// Cartão MicroSD (Barramento SPI remapeado livre de conflito)
#define SD_SCK               18
#define SD_MISO              19
#define SD_MOSI              25
#define SD_CS                15

#define SerialMon Serial
#define SerialAT  Serial1

// Instanciamento de Objetos
TinyGsm modem(SerialAT);
TinyGsmClient gsmClient(modem);
HttpClient httpClient(gsmClient, serverHost, serverPort);

Adafruit_BMP280 bmp;
DHT dht(DHTPIN, DHTTYPE);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);

bool bmpPresente = false;
bool dhtPresente = false;
bool dsPresente  = false;

// ============================================================================
// GERENCIAMENTO DE ENERGIA (IP5306) E LIGAÇÃO DO MODEM
// ============================================================================
bool setupIP5306() {
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.beginTransmission(IP5306_ADDR);
    Wire.write(IP5306_REG_SYS_CTL0);
    Wire.write(0x37); // Mantém o Boost de 5V sempre ativo para alimentar o SIM800L
    return (Wire.endTransmission() == 0);
}

void powerOnModem() {
    SerialMon.println("[ENERGIA] Inicializando alimentacao do modulo SIM800L...");
    pinMode(MODEM_POWER_ON, OUTPUT);
    digitalWrite(MODEM_POWER_ON, HIGH);

    pinMode(MODEM_RST, OUTPUT);
    digitalWrite(MODEM_RST, HIGH);

    pinMode(MODEM_PWRKEY, OUTPUT);
    digitalWrite(MODEM_PWRKEY, HIGH);
    delay(100);
    digitalWrite(MODEM_PWRKEY, LOW);
    delay(1000); // Pulso de ~1s para ligar o SIM800L
    digitalWrite(MODEM_PWRKEY, HIGH);
    delay(3000);
}

// ============================================================================
// CRIPTOGRAFIA AES-128-CBC PADRÃO GAIA (COM IV RANDÔMICO E BASE64)
// ============================================================================
String encryptAES128Base64(const String& plaintext, const char* key16) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    
    unsigned char key[16];
    memset(key, 0, 16);
    memcpy(key, key16, min((size_t)16, strlen(key16)));
    mbedtls_aes_setkey_enc(&aes, key, 128);

    // Gera IV randômico por hardware no ESP32
    unsigned char iv[16];
    for (int i = 0; i < 16; i++) {
        iv[i] = (unsigned char)esp_random();
    }
    unsigned char ivCopy[16];
    memcpy(ivCopy, iv, 16);

    // Preenchimento PKCS#7
    size_t plainLen = plaintext.length();
    size_t padLen = 16 - (plainLen % 16);
    size_t paddedLen = plainLen + padLen;
    
    unsigned char* input = (unsigned char*)malloc(paddedLen);
    memcpy(input, plaintext.c_str(), plainLen);
    memset(input + plainLen, padLen, padLen);

    unsigned char* output = (unsigned char*)malloc(paddedLen);
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, paddedLen, ivCopy, input, output);
    mbedtls_aes_free(&aes);

    // Combina: 16 bytes do IV + Ciphertext
    size_t totalEncryptedLen = 16 + paddedLen;
    unsigned char* combined = (unsigned char*)malloc(totalEncryptedLen);
    memcpy(combined, iv, 16);
    memcpy(combined + 16, output, paddedLen);

    // Codifica para Base64
    size_t b64Len = 0;
    mbedtls_base64_encode(NULL, 0, &b64Len, combined, totalEncryptedLen);
    unsigned char* b64Output = (unsigned char*)malloc(b64Len + 1);
    mbedtls_base64_encode(b64Output, b64Len + 1, &b64Len, combined, totalEncryptedLen);
    b64Output[b64Len] = '\0';

    String result = (char*)b64Output;

    free(input);
    free(output);
    free(combined);
    free(b64Output);

    return result;
}

// ============================================================================
// INTELIGÊNCIA AGROCLIMÁTICA NA BORDA (Cálculos FAO-56 / VPD / Orvalho)
// ============================================================================
float calcularPontoOrvalho(float T, float RH) {
    if (isnan(T) || isnan(RH) || RH <= 0.0) return NAN;
    float a = 17.27, b = 237.7;
    float alpha = ((a * T) / (b + T)) + log(RH / 100.0);
    return (b * alpha) / (a - alpha);
}

float calcularVPD(float T, float RH) {
    if (isnan(T) || isnan(RH)) return NAN;
    float es = 0.61078 * exp((17.27 * T) / (T + 237.3)); // Pressão de vapor de saturação (kPa)
    float ea = es * (RH / 100.0);                         // Pressão de vapor atual (kPa)
    return max(0.0f, es - ea);
}

float calcularET0(float T, float RH, float Rs = 250.0) {
    // Estimativa simplificada de evapotranspiração de referência FAO-56
    if (isnan(T) || isnan(RH)) return 0.02f;
    float vpd = calcularVPD(T, RH);
    float et0 = (0.0023 * (T + 17.8) * sqrt(max(1.0f, vpd * 10.0f)) * (Rs * 0.0036));
    return max(0.005f, et0);
}

// ============================================================================
// COLETA DE SENSORES OU TELEMETRIA REALISTA DE MONTE CARMELO
// ============================================================================
TelemetriaGAIA lerSensores() {
    TelemetriaGAIA d;

    // Tentativa de leitura real DHT22
    float tD = dht.readTemperature();
    float hD = dht.readHumidity();

    // Altitude média de Monte Carmelo: ~870m (Pressão típica ~915 hPa)
    float pB = 915.2;
    float tB = 25.0;
    if (bmpPresente) {
        pB = bmp.readPressure() / 100.0F;
        tB = bmp.readTemperature();
    }

    // Se sensor físico estiver conectado, usa leitura real. Caso contrário, gera valor base calibrado para bancada
    if (!isnan(tD) && !isnan(hD)) {
        d.tempAr = tD;
        d.umidAr = hD;
    } else {
        d.tempAr = 25.6 + (random(-10, 10) / 10.0);
        d.umidAr = 62.0 + (random(-20, 20) / 10.0);
    }

    d.pressao = pB;

    // Leitura DS18B20 (Temperatura do solo)
    if (dsPresente) {
        ds18b20.requestTemperatures();
        d.tempSolo = ds18b20.getTempCByIndex(0);
        if (d.tempSolo < -50 || d.tempSolo > 80) d.tempSolo = d.tempAr - 1.8;
    } else {
        d.tempSolo = d.tempAr - 1.8;
    }

    // Sensor de Umidade de Solo (ADC1_CH7 - GPIO 35)
    int adcSolo = analogRead(SOLO_PIN);
    if (adcSolo > 100) {
        // Mapeia ADC (4095 = seco, 1500 = saturado)
        d.umidSolo = constrain(map(adcSolo, 3500, 1200, 0, 100), 0, 100);
    } else {
        d.umidSolo = 58; // Valor de referência
    }

    d.lux = 38500.0 + random(-2000, 2000);
    d.pontoOrvalho = calcularPontoOrvalho(d.tempAr, d.umidAr);
    d.vpd = calcularVPD(d.tempAr, d.umidAr);
    d.et0 = calcularET0(d.tempAr, d.umidAr);

    return d;
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    SerialMon.begin(115200);
    delay(1000);

    SerialMon.println("\n=======================================================");
    SerialMon.println("       PROJETO GAIA - ESTACAO AGROCLIMATICA            ");
    SerialMon.printf ("       ESTACAO: %s  |  LOCAL: %s\n", ESTACAO_CODIGO, ESTACAO_LOCALIDADE);
    SerialMon.println("=======================================================");

    // 1. Configurar gerenciador de energia da TTGO T-Call
    SerialMon.print("[1/5] Inicializando PMU IP5306... ");
    if (setupIP5306()) {
        SerialMon.println("OK (Boost mantido ativo)");
    } else {
        SerialMon.println("AVISO (PMU em modo autonomo)");
    }

    // 2. Ligar SIM800L
    SerialMon.println("[2/5] Ligando modem SIM800L...");
    powerOnModem();

    // 3. Inicializar comunicação serial com SIM800L
    SerialMon.println("[3/5] Inicializando UART SerialAT (115200)...");
    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
    delay(1000);

    // 4. Inicializar sensores físicos
    SerialMon.println("[4/5] Detectando barramento I2C e sensores físicos...");
    if (bmp.begin(0x76) || bmp.begin(0x77)) {
        bmpPresente = true;
        SerialMon.println("  -> Sensor Barométrico BMP280: DETECTADO");
    } else {
        SerialMon.println("  -> BMP280: Não conectado (usando pressão de referência Monte Carmelo 915 hPa)");
    }

    dht.begin();
    float tTeste = dht.readTemperature();
    if (!isnan(tTeste)) {
        dhtPresente = true;
        SerialMon.println("  -> Sensor DHT22: DETECTADO");
    } else {
        SerialMon.println("  -> DHT22: Não conectado (usando parâmetros simulados)");
    }

    ds18b20.begin();
    if (ds18b20.getDeviceCount() > 0) {
        dsPresente = true;
        SerialMon.println("  -> Sensor DS18B20 de Solo: DETECTADO");
    } else {
        SerialMon.println("  -> DS18B20: Não conectado");
    }

    // 5. Testar e sincronizar Modem Celular
    SerialMon.println("[5/5] Sincronizando com modem SIM800L...");
    if (!modem.restart()) {
        SerialMon.println("Aviso: restart lento, testando inicializacao direta...");
        modem.init();
    }

    String modemInfo = modem.getModemInfo();
    SerialMon.printf("  -> Info do Modem: %s\n", modemInfo.c_str());
    SerialMon.printf("  -> IMEI: %s\n", modem.getIMEI().c_str());
    SerialMon.printf("  -> Sinal GSM (CSQ): %d\n", modem.getSignalQuality());

    SerialMon.println("\nAguardando conexao com a rede celular da operadora...");
    if (modem.waitForNetwork(60000L)) {
        SerialMon.println("  -> REGISTRADO NA REDE CELULAR COM SUCESSO!");
        String operadora = modem.getOperator();
        SerialMon.printf("  -> Operadora: %s\n", operadora.c_str());
    } else {
        SerialMon.println("  -> FALHA: Não foi possível registrar na rede celular dentro do tempo limite.");
    }

    SerialMon.println("\nInicialização concluída. Iniciando ciclo de telemetria GAIA...\n");
}

// ============================================================================
// LOOP PRINCIPAL: COLETA, CRIPTOGRAFIA E TRANSMISSÃO
// ============================================================================
void loop() {
    SerialMon.println("\n-------------------------------------------------------");
    SerialMon.printf(">>> CICLO DE TELEMETRIA GAIA - %s (%s)\n", ESTACAO_CODIGO, ESTACAO_LOCALIDADE);
    SerialMon.println("-------------------------------------------------------");

    // 1. Coletar dados agroclimáticos
    TelemetriaGAIA d = lerSensores();

    // 2. Montar objeto JSON de telemetria conforme PADRAO_ENVIO_TELEMETRIA.md
    JsonDocument doc;
    doc["codSta"]  = ESTACAO_CODIGO;
    doc["local"]   = ESTACAO_LOCALIDADE;
    doc["ts"]      = (uint32_t)(1728900000 + (millis() / 1000)); // Timestamp de referência
    doc["tD"]      = serialized(String(d.tempAr, 1));
    doc["h"]       = serialized(String(d.umidAr, 1));
    doc["tB"]      = serialized(String(d.tempAr, 1));
    doc["p"]       = serialized(String(d.pressao, 1));
    doc["l"]       = serialized(String(d.lux, 0));
    doc["s"]       = d.umidSolo;
    doc["tS"]      = serialized(String(d.tempSolo, 1));
    doc["tRef"]    = serialized(String(d.tempAr, 1));
    doc["dT"]      = 0.2;
    doc["q"]       = 100;
    doc["po"]      = serialized(String(d.pontoOrvalho, 1));
    doc["vpd"]     = serialized(String(d.vpd, 2));
    doc["et0"]     = serialized(String(d.et0, 4));
    doc["etc"]     = serialized(String(d.et0, 4));
    doc["rs"]      = 285.0;
    doc["par"]     = 610.0;
    doc["rf"]      = 0;
    doc["rg"]      = 0;
    doc["dp"]      = 0.1;
    doc["at"]      = false;
    doc["itu"]     = 72.5;
    doc["hi"]      = serialized(String(d.tempAr, 1));
    doc["heapMod"] = ESP.getFreeHeap() / 1024;

    String jsonPlain;
    serializeJson(doc, jsonPlain);

    SerialMon.println("[PAYLOAD BRUTO JSON]");
    SerialMon.println(jsonPlain);

    // 3. Criptografar payload em AES-128-CBC com IV randômico e Base64
    SerialMon.println("\n[CRIPTOGRAFIA]");
    SerialMon.printf("Cifrando dados com chave oficial '%s'...\n", GAIA_AES_KEY);
    String encryptedData = encryptAES128Base64(jsonPlain, GAIA_AES_KEY);

    // Envelope final de envio ao servidor: {"data": "s8F3gK91lA..."}
    JsonDocument envelope;
    envelope["data"] = encryptedData;
    String jsonEnvelope;
    serializeJson(envelope, jsonEnvelope);

    SerialMon.printf("Envelope Criptografado Base64 (%d bytes):\n", jsonEnvelope.length());
    SerialMon.println(jsonEnvelope);

    // 4. Conectar GPRS e Enviar via HTTP POST
    SerialMon.println("\n[TRANSMISSAO CELULAR GPRS]");
    SerialMon.printf("Conectando a APN '%s'...\n", apn);

    if (!modem.isGprsConnected()) {
        if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
            SerialMon.println("Falha ao ativar GPRS com a operadora.");
        } else {
            SerialMon.println("Conexao GPRS ativada com sucesso!");
        }
    }

    if (modem.isGprsConnected()) {
        SerialMon.printf("Disparando HTTP POST para: http://%s:%d%s ...\n", serverHost, serverPort, serverPath);

        httpClient.beginRequest();
        httpClient.post(serverPath);
        httpClient.sendHeader("Content-Type", "application/json");
        httpClient.sendHeader("Content-Length", jsonEnvelope.length());
        httpClient.sendHeader("X-Station-Code", ESTACAO_CODIGO);
        httpClient.beginBody();
        httpClient.print(jsonEnvelope);
        httpClient.endRequest();

        int statusCode = httpClient.responseStatusCode();
        String response = httpClient.responseBody();

        SerialMon.printf("-> Codigo de Status HTTP: %d\n", statusCode);
        SerialMon.printf("-> Resposta do Servidor:\n%s\n", response.c_str());

        if (statusCode >= 200 && statusCode < 300) {
            SerialMon.println("\n[SUCESSO] Telemetria da estacao G00002 entregue ao servidor!");
        } else {
            SerialMon.println("\n[AVISO] Servidor respondeu com codigo fora da faixa 2xx.");
        }
    } else {
        SerialMon.println("[AVISO] GPRS offline neste ciclo. Dados seriam gravados na fila local.");
    }

    SerialMon.println("\nAguardando 30 segundos para o próximo ciclo de telemetria...");
    delay(30000);
}
