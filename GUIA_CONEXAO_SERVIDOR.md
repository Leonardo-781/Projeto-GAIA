# 🔌 Guia Prático de Conexão: ESP32 ao Servidor GAIA

Este manual detalha o passo a passo de como integrar e enviar dados de qualquer **ESP32** para o servidor, seja pela **rede local (Wi-Fi de casa)** ou remotamente **pela Internet** (via Cloudflare Tunnel).

---

## 📌 1. Endereços do Servidor para o ESP32

Dependendo de onde a placa ESP32 estiver instalada, utilize o endereço correspondente:

| Cenário de Instalação | Protocolo | Endereço / URL no ESP32 | Porta |
| :--- | :--- | :--- | :--- |
| **Na mesma rede de casa (Bancada / Teste)** | **HTTP REST** | `http://192.168.0.105:3000/api/telemetria` | `3000` |
| **No Campo / Remoto (Internet / 4G / Outro Wi-Fi)** | **HTTPS REST** | `https://stunning-modeling-became-coverage.trycloudflare.com/api/telemetria` | `443` |
| **Rede Local via MQTT (Baixa latência / Tempo Real)** | **MQTT (TCP)** | `192.168.0.105` | `1883` |

> [!NOTE]
> O endpoint `/api/telemetria` aceita tanto pacotes em **JSON puro** quanto pacotes **criptografados com AES-128-CBC**.

---

## 📄 2. Estrutura do Payload de Telemetria

O servidor espera um payload JSON estruturado conforme o padrão do Projeto GAIA:

### 2.1. Tabela de Campos Suportados

| Campo | Tipo | Descrição | Exemplo |
| :--- | :--- | :--- | :--- |
| `codSta` | `string` | Código identificador da estação (6 caracteres) | `"G00001"` |
| `ts` | `uint32` | Timestamp Unix epoch (Relógio Atômico NTP) | `1726830000` |
| `tD` | `float` | Temperatura do Ar via DHT22 (°C) | `24.5` |
| `h` | `float` | Umidade Relativa do Ar via DHT22 (%) | `65.0` |
| `tB` | `float` | Temperatura do Ar via BMP280 (°C) | `24.2` |
| `p` | `float` | Pressão Atmosférica via BMP280 (hPa) | `1013.2` |
| `l` | `float` | Radiação / Luminosidade via VEML7700 (Lux) | `18500.0` |
| `s` | `int` | Umidade do Solo calibrada (0 a 100%) | `55` |
| `tS` | `float` | Temperatura do Solo via DS18B20 1-Wire (°C) | `21.8` |
| `tRef` | `float` | Temperatura de referência calculada (°C) | `24.5` |
| `dT` | `float` | Variação entre sensores térmicos (°C) | `0.3` |
| `q` | `int` | Índice de integridade da medição (0 a 100%) | `100` |
| `po` | `float` | Ponto de Orvalho calculado (°C) | `17.4` |
| `vpd` | `float` | Déficit de Pressão de Vapor (kPa) | `1.08` |
| `et0` | `float` | Evapotranspiração de Referência FAO-56 (mm) | `0.0125` |
| `heapMod`| `int` | Diagnóstico de memória RAM livre do ESP32 | `450` |

---

## 🚀 3. Opção A: Como Configurar no Firmware Existente (`TX Telemetria.c`)

Se você já estiver usando o código da estação de campo do repositório (`Projeto Gaia/TX Telemetria.c` ou `TX_Telemetria.ino`), existem duas formas simples de conectar:

### Método 1: Alterando direto no código (Linha 73)
Abra o arquivo `TX Telemetria.c` na Arduino IDE ou VS Code e altere a linha 73:

```cpp
// Para testar na sua casa (Wi-Fi local):
#define DEFAULT_SERVER_URL "http://192.168.0.105:3000/api/telemetria"

// OU para enviar pela Internet pública (campo):
#define DEFAULT_SERVER_URL "https://stunning-modeling-became-coverage.trycloudflare.com/api/telemetria"
```

### Método 2: Pelo Portal Captivo Wi-Fi do ESP32 (Sem regravar código)
1. Ao ligar o ESP32, mantenha o botão no **GPIO 15** pressionado para forçar o modo de configuração.
2. Conecte seu celular na rede Wi-Fi criada pela placa: **`GAIA-TX-[MAC]`** (senha: `gaiaconfig`).
3. O navegador abrirá automaticamente a tela de configuração (ou acesse `http://192.168.4.1`).
4. No campo **Endpoint do Servidor HTTP**, cole:
   `http://192.168.0.105:3000/api/telemetria`
5. Clique em **Salvar e Reiniciar**. As configurações ficarão salvas na memória permanente (NVS) do microcontrolador.

---

## 💻 4. Opção B: Exemplo de Código Simples (HTTP REST Direto)

Se você quiser criar um novo firmware do zero ou testar um ESP32 secundário enviando medições para o painel em tempo real:

```cpp
/**
 * Exemplo de Envio Direto via HTTP REST para o Servidor GAIA
 * Placa: ESP32 Dev Module
 * Requer biblioteca: ArduinoJson (v6 ou v7)
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Configurações de Wi-Fi
const char* ssid = "SEU_WIFI_AQUI";
const char* password = "SUA_SENHA_AQUI";

// URL do Servidor (Local ou Cloudflare)
const char* serverUrl = "http://192.168.0.105:3000/api/telemetria";
// const char* serverUrl = "https://stunning-modeling-became-coverage.trycloudflare.com/api/telemetria";

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.print("Conectando ao Wi-Fi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi Conectado! IP do ESP32: " + WiFi.localIP().toString());
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(8000);

    // 1. Coleta ou Simulação de Sensores
    float tempAr = 24.5;
    float umidAr = 68.0;
    float pressao = 1013.25;
    float lux = 22000.0;
    int solo = 55;
    float tempSolo = 22.0;

    // 2. Cálculos Agroclimáticos Básicos (VPD e Orvalho)
    float es = 0.61078 * exp((17.27 * tempAr) / (tempAr + 237.3));
    float ea = es * (umidAr / 100.0);
    float vpd = es - ea;

    float alpha = ((17.27 * tempAr) / (237.7 + tempAr)) + log(umidAr / 100.0);
    float pontoOrvalho = (237.7 * alpha) / (17.27 - alpha);

    // 3. Montagem do JSON
    JsonDocument doc;
    doc["codSta"] = "G00001";
    doc["ts"] = (uint32_t)(millis() / 1000); // Em produção, utilize NTP
    doc["tD"] = tempAr;
    doc["h"] = umidAr;
    doc["p"] = pressao;
    doc["l"] = lux;
    doc["s"] = solo;
    doc["tS"] = tempSolo;
    doc["tRef"] = tempAr;
    doc["vpd"] = vpd;
    doc["po"] = pontoOrvalho;
    doc["et0"] = 0.012;
    doc["q"] = 100;
    doc["heapMod"] = ESP.getFreeHeap() % 1000;

    String requestBody;
    serializeJson(doc, requestBody);

    Serial.println("\nEnviando pacote ao servidor...");
    int httpResponseCode = http.POST(requestBody);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.printf("Resposta do Servidor [%d]: %s\n", httpResponseCode, response.c_str());
    } else {
      Serial.printf("Falha no envio HTTP. Código de erro: %d (%s)\n", 
                    httpResponseCode, http.errorToString(httpResponseCode).c_str());
    }

    http.end();
  } else {
    Serial.println("Wi-Fi desconectado. Tentando reconectar...");
    WiFi.reconnect();
  }

  // Intervalo de envio (ex: a cada 10 segundos para teste)
  delay(10000);
}
```

---

## 📡 5. Opção C: Exemplo de Código via MQTT (Mosquitto)

Caso deseje comunicação contínua bidirecional de altíssima velocidade e baixíssimo consumo:

```cpp
/**
 * Exemplo de Envio via MQTT para o Servidor
 * Requer biblioteca: PubSubClient
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

const char* ssid = "SEU_WIFI";
const char* password = "SUA_SENHA";

// IP do notebook servidor na rede de casa
const char* mqttServer = "192.168.0.105";
const int   mqttPort = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
  
  client.setServer(mqttServer, mqttPort);
}

void reconectar() {
  while (!client.connected()) {
    Serial.print("Conectando ao broker MQTT...");
    if (client.connect("ESP32_GAIA_G00001")) {
      Serial.println("Conectado com sucesso!");
    } else {
      Serial.print("Falha rc=");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconectar();
  }
  client.loop();

  // Publica dados a cada 5 segundos
  static unsigned long lastMsg = 0;
  if (millis() - lastMsg > 5000) {
    lastMsg = millis();

    JsonDocument doc;
    doc["estacao"] = "G00001";
    doc["temp"] = 25.3;
    doc["umid"] = 62.0;
    doc["solo"] = 48;

    char buffer[256];
    serializeJson(doc, buffer);

    client.publish("gaia/telemetria", buffer);
    Serial.println("Mensagem publicada via MQTT!");
  }
}
```

---

## 🔍 6. Como Monitorar e Validar os Envios em Tempo Real

Para ter certeza absoluta de que o servidor está recebendo e gravando os dados enviados pelo seu ESP32:

### 1. Monitorar os logs do servidor pelo terminal (PowerShell no Windows)
Abra o PowerShell e execute:
```powershell
ssh leo@192.168.0.105 "docker logs -f gaia-server"
```
Assim que o ESP32 enviar um pacote, aparecerá instantaneamente no terminal:
```text
[TELEMETRIA] Recebida de G00001 | T:24.5°C | U:65% | Solo:55% | VPD:1.08kPa | ET0:0.012mm
```

### 2. Pelo Dashboard Web
Basta abrir o link:
👉 **[https://stunning-modeling-became-coverage.trycloudflare.com](https://stunning-modeling-became-coverage.trycloudflare.com)**  
O badge superior exibirá **"Tempo Real Conectado"** e todos os gráficos e cards se moverão no momento exato em que o ESP32 transmitir.

---

## 🛠️ 7. Guia Rápido de Resolução de Problemas (Troubleshooting)

| Sintoma no ESP32 | Causa Mais Provável | O que fazer |
| :--- | :--- | :--- |
| `HTTP Error -1 (Connection Refused)` | IP incorreto ou porta errada | Verifique se o notebook está ligado no IP `192.168.0.105` e porta `3000`. |
| `Falha na descriptografia AES-128` | Chave divergente | Certifique-se de que a chave no ESP32 seja exatamente `MARIACRYPTOKEY12`. |
| `WiFi.status() != WL_CONNECTED` | Falha de sinal ou senha do roteador | Aproxime o ESP32 do roteador de casa. O ESP32 opera apenas em redes **2.4 GHz**. |
| Envio via Internet falha com HTTPS | Validação de certificado SSL no ESP32 | No código com `HTTPClient`, adicione `http.setInsecure();` para ignorar verificação de root certificate da Cloudflare. |
