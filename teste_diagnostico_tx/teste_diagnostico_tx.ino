/**
 * ============================================================================
 * ARQUIVO: teste_diagnostico_tx.ino
 * VERSÃO: 1.0
 * DATA: 2026-07-28
 * DESCRIÇÃO: Código de teste e diagnóstico completo para a Estação de Campo (TX).
 *            Varre o barramento I2C, testa o cartão SD, sensores (DHT22, BMP280, 
 *            VEML7700, DS18B20, Umidade do Solo), relógio RTC e o módulo LoRa E220.
 * COMUNICAÇÃO: ESP32 conectado na porta COM25.
 * ============================================================================
 */

#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_VEML7700.h>
#include <DHT.h>
#include <RTClib.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "LoRa_E220.h"

// Definições de pinos da Estação
#define SOLO_PIN        34
#define SD_CS           5
#define DHTPIN          14
#define DHTTYPE         DHT22
#define BTN_FORMAT_PIN  4
#define BTN_AP_PIN      15

#define LORA_RX_PIN     16
#define LORA_TX_PIN     17
#define LORA_M0_PIN     25
#define LORA_M1_PIN     26
#define LORA_AUX_PIN    27

#define LED_PIN         2
#define ONE_WIRE_BUS    13

// Instanciamento dos objetos dos sensores
Adafruit_BMP280 bmp;
Adafruit_VEML7700 veml;
DHT dht(DHTPIN, DHTTYPE);
RTC_DS3231 rtc;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18Sensors(&oneWire);

HardwareSerial loraSerial(2);
LoRa_E220 e220ttl(&loraSerial, LORA_AUX_PIN, LORA_M0_PIN, LORA_M1_PIN);

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10); // Aguarda abertura do Monitor Serial
  }
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // Liga o LED durante o teste inicial

  pinMode(BTN_FORMAT_PIN, INPUT_PULLUP);
  pinMode(BTN_AP_PIN, INPUT_PULLUP);

  Serial.println("\n==================================================");
  Serial.println("         INICIANDO DIAGNÓSTICO DA ESTAÇÃO         ");
  Serial.println("==================================================");

  // 1. Varredura do Barramento I2C (RTC, BMP280, VEML7700)
  Serial.println("\n[1] Varrendo barramento I2C (GPIO 21 - SDA, GPIO 22 - SCL)...");
  Wire.begin(21, 22);
  Wire.setTimeOut(100);
  
  byte error, address;
  int nDevices = 0;
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.printf(" ➔ Dispositivo I2C encontrado no endereço 0x%02X ", address);
      if (address == 0x68) Serial.println("(RTC DS3231)");
      else if (address == 0x76 || address == 0x77) Serial.println("(Barômetro BMP280)");
      else if (address == 0x10) Serial.println("(Sensor de Luz VEML7700)");
      else Serial.println("(Desconhecido)");
      nDevices++;
    }
  }
  if (nDevices == 0) {
    Serial.println(" ❌ NENHUM dispositivo I2C encontrado! Verifique fios SDA/SCL e alimentação.");
  } else {
    Serial.printf(" ➔ Total de dispositivos I2C detectados: %d\n", nDevices);
  }

  // 2. Teste do Leitor de Cartão SD (SPI)
  Serial.println("\n[2] Inicializando Cartão SD (CS no GPIO 5)...");
  if (!SD.begin(SD_CS)) {
    Serial.println(" ❌ Erro ao inicializar Cartão SD. Verifique contatos, fiação SPI ou formate o cartão em FAT32.");
  } else {
    uint8_t cardType = SD.cardType();
    Serial.print(" ➔ Cartão SD detectado com sucesso. Tipo: ");
    if (cardType == CARD_MMC) Serial.println("MMC");
    else if (cardType == CARD_SD) Serial.println("SDSC");
    else if (cardType == CARD_SDHC) Serial.println("SDHC");
    else Serial.println("Desconhecido");
    
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf(" ➔ Capacidade do SD: %llu MB\n", cardSize);
  }

  // 3. Teste do DHT22
  Serial.println("\n[3] Inicializando Sensor de Ar DHT22 (GPIO 14)...");
  dht.begin();
  float tDht = dht.readTemperature();
  float hDht = dht.readHumidity();
  if (isnan(tDht) || isnan(hDht)) {
    Serial.println(" ❌ Erro ao ler DHT22! Verifique a fiação e se há um resistor de 10k entre DATA e VCC.");
  } else {
    Serial.printf(" ➔ DHT22 OK. Temperatura: %.2f °C | Umidade: %.2f %%\n", tDht, hDht);
  }

  // 4. Teste do BMP280
  Serial.println("\n[4] Inicializando Barômetro BMP280 (Endereço I2C 0x76)...");
  if (!bmp.begin(0x76)) {
    Serial.println(" ❌ Erro ao iniciar BMP280! Verifique se está soldado no endereço 0x76 (ou mude para 0x77 no código).");
  } else {
    Serial.printf(" ➔ BMP280 OK. Temp Interna: %.2f °C | Pressão: %.2f hPa\n", bmp.readTemperature(), bmp.readPressure() / 100.0);
  }

  // 5. Teste do VEML7700
  Serial.println("\n[5] Inicializando Sensor de Luz VEML7700 (I2C)...");
  if (!veml.begin()) {
    Serial.println(" ❌ Erro ao iniciar VEML7700! Verifique fiação I2C.");
  } else {
    Serial.printf(" ➔ VEML7700 OK. Luminosidade: %.2f Lux\n", veml.readLux());
  }

  // 6. Teste do Relógio RTC DS3231
  Serial.println("\n[6] Inicializando RTC DS3231 (I2C)...");
  if (!rtc.begin()) {
    Serial.println(" ❌ Erro ao iniciar RTC! Verifique fiação.");
  } else {
    DateTime agora = rtc.now();
    Serial.printf(" ➔ RTC OK. Data/Hora atual: %02d/%02d/%04d %02d:%02d:%02d\n", 
                  agora.day(), agora.month(), agora.year(), 
                  agora.hour(), agora.minute(), agora.second());
  }

  // 7. Teste do DS18B20 (Temperatura do Solo)
  Serial.println("\n[7] Inicializando DS18B20 (GPIO 13)...");
  ds18Sensors.begin();
  ds18Sensors.requestTemperatures();
  float tSolo = ds18Sensors.getTempCByIndex(0);
  if (tSolo == DEVICE_DISCONNECTED_C) {
    Serial.println(" ❌ Erro: DS18B20 não encontrado! Verifique a fiação (Amarelo no GPIO 13) e se o resistor de 4.7k está conectado.");
  } else {
    Serial.printf(" ➔ DS18B20 OK. Temp. do Solo: %.2f °C\n", tSolo);
  }

  // 8. Teste do Módulo LoRa E220
  Serial.println("\n[8] Testando Comunicação Serial com LoRa E220 (GPIO 16 - RX, GPIO 17 - TX)...");
  loraSerial.begin(9600, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);
  e220ttl.begin();
  ResponseStructContainer c = e220ttl.getConfiguration();
  if (c.status.code != 1) {
    Serial.printf(" ❌ Falha ao comunicar com o LoRa E220! Código: %s\n", c.status.getResponseDescription().c_str());
  } else {
    Serial.println(" ➔ LoRa E220 OK. Comunicação Serial estabelecida.");
  }
  c.close();

  // 9. Teste do Sensor de Umidade do Solo Analógico (GPIO 34)
  Serial.println("\n[9] Lendo Sensor de Umidade do Solo Analógico (GPIO 34)...");
  int rawSolo = analogRead(SOLO_PIN);
  Serial.printf(" ➔ Leitura analógica bruta (ADC1): %d / 4095\n", rawSolo);
  if (rawSolo < 100 || rawSolo > 4000) {
    Serial.println("   * Nota: Valor muito baixo ou muito alto. Sensor pode estar desconectado ou fora da terra.");
  }

  // 10. Teste dos Botões
  Serial.println("\n[10] Verificando Botões (pressione-os para ver mudança no Monitor)...");
  Serial.printf(" ➔ Botão Formatação (GPIO 4): %s\n", (digitalRead(BTN_FORMAT_PIN) == LOW) ? "PRESSIONADO" : "SOLTO");
  Serial.printf(" ➔ Botão Modo AP (GPIO 15): %s\n", (digitalRead(BTN_AP_PIN) == LOW) ? "PRESSIONADO" : "SOLTO");

  Serial.println("\n==================================================");
  Serial.println("  DIAGNÓSTICO CONCLUÍDO. INICIANDO LEITURA CONTÍNUA ");
  Serial.println("==================================================");
  digitalWrite(LED_PIN, LOW); // Apaga o LED de teste inicial
}

void loop() {
  // Pisca o LED indicador a cada ciclo
  digitalWrite(LED_PIN, HIGH);
  delay(100);
  digitalWrite(LED_PIN, LOW);

  // Realiza novas leituras dos sensores
  float tDht = dht.readTemperature();
  float hDht = dht.readHumidity();
  float tBmp = bmp.readTemperature();
  float pBmp = bmp.readPressure() / 100.0;
  float luxVal = veml.readLux();
  
  ds18Sensors.requestTemperatures();
  float tSolo = ds18Sensors.getTempCByIndex(0);
  int rawSolo = analogRead(SOLO_PIN);
  
  DateTime agora = rtc.now();

  // Imprime no Monitor Serial
  Serial.printf("[%02d:%02d:%02d] ", agora.hour(), agora.minute(), agora.second());
  Serial.printf("DHT: %.1f°C/%.1f%% | ", tDht, hDht);
  Serial.printf("BMP: %.1f°C/%.1fHPa | ", tBmp, pBmp);
  Serial.printf("Luz: %.1f Lux | ", luxVal);
  Serial.printf("SoloTemp: %.1f°C | ", tSolo);
  Serial.printf("SoloUmid: %d | ", rawSolo);
  Serial.printf("Botoes: Fmt:%d AP:%d\n", digitalRead(BTN_FORMAT_PIN), digitalRead(BTN_AP_PIN));

  delay(3000); // Aguarda 3 segundos para a próxima rodada
}
