# Projeto GAIA - Estação Agroclimática e Meteorológica Inteligente

O **Projeto GAIA** é um sistema de monitoramento agrícola e telemetria de campo autônomo baseado no microcontrolador ESP32. Ele é composto por duas unidades integradas: a **Estação de Campo (Transmissora - TX)** para coleta ambiental e envio seguro via Internet/Wi-Fi com inteligência agronômica de borda e validação de timestamp via Relógio Atômico (NTP.br), e a **Central Receptora (RX)** com display colorido TFT ILI9341, Gateway REST HTTP over Wi-Fi, armazenamento em Cartão SD e navegação física via Encoder Rotativo.

---

## 🚀 Principais Características

* **Comunicação 100% via Internet / Wi-Fi:** Transmissão direta por meio de rede Wi-Fi e endpoints HTTP REST seguros, eliminando a dependência de rádios LoRa e simplificando o alcance e integração com a nuvem.
* **Sincronização com Relógio Atômico (NTP.br):** Validação precisa e temporal de timestamp (`ts`) diretamente através de servidores de estrato 1 sincronizados com o Relógio Atômico do Observatório Nacional (`a.st1.ntp.br`, `b.st1.ntp.br`, `pool.ntp.org`), dispensando completamente módulos físicos de RTC sujeitos a falha de bateria.
* **Criptografia Militar de Borda:** Comunicação segura criptografada via hardware com **AES-128-CBC** (usando MbedTLS nativo do ESP32) e vetor de inicialização (IV) randômico em cada pacote, prevenindo injeção de dados falsos.
* **Inteligência Agroclimática na Borda:** Cálculo local e em tempo real do **Ponto de Orvalho**, **Déficit de Pressão de Vapor (VPD)** e **Evapotranspiração de Referência ($ET_0$)** baseado no modelo FAO-56 simplificado.
* **Modo AP com Portal Captivo Real (DNSServer):** Ao entrar em configuração, a placa cria um Access Point (`GAIA-TX-[MAC]` ou `GAIA-RX-SETUP`) com servidor DNS integrado na porta 53. Qualquer celular ou computador que conectar abre a interface de configuração automaticamente, sem necessidade de digitar IP manual.
* **Persistência Confiável em Memória NVS (Preferences):** Credenciais de Wi-Fi, endpoint de servidor, código da estação e chave criptográfica são armazenados na memória flash interna permanente do microcontrolador e espelhados no Cartão SD quando presente. A estação nunca perde configurações se o cartão for removido ou formatado.
* **Diagnóstico de Sensores em Tempo Real via Web:** O portal AP da estação TX disponibiliza leitura instantânea de todos os sensores físicos (DHT22, BMP280, VEML7700, DS18B20, umidade analógica do solo, Relógio Atômico NTP, status do Cartão SD e memória Heap), permitindo auditoria completa em campo direto na tela do smartphone.
* **Resiliência a Falhas (Fila SD Offline):** Em caso de indisponibilidade de rede, as medições criptografadas são armazenadas sequencialmente no Cartão SD (`/fila.csv`) e transmitidas assim que a conectividade for restabelecida.
* **Deep Sleep com Alinhamento Temporal Atômico:** A estação de campo entra em sono profundo e desperta em horários exatos alinhados pelo RTC interno do ESP32 sincronizado com o Relógio Atômico (intervalos de 1, 5, 10 ou 15 minutos), com offset pseudo-aleatório baseado no MAC para evitar colisões de tráfego na rede.
* **Central RX com Gateway REST HTTP e Interface Gráfica:** Central receptora atua como servidor HTTP (`/api/telemetria`) recebendo telemetria via Wi-Fi das estações de campo, exibindo logs em tempo real na tela TFT com IP local, gravando no SD e reencaminhando ao banco de dados em nuvem.

---

## 🛠️ Esquema de Pinos - Estação de Campo (TX)

| Componente | Sinal / Função | Pino no ESP32 |
| :--- | :--- | :--- |
| **Wi-Fi / Internet** | Conexão local, NTP Atômico e HTTP REST | Integrado ao ESP32 |
| **Leitor Cartão SD** | CS / MOSI / MISO / SCK (SPI) | **GPIO 5** / **GPIO 23** / **GPIO 19** / **GPIO 18** |
| **Barramento I2C** | SDA / SCL (BMP280, VEML7700) | **GPIO 21** / **GPIO 22** |
| **DHT22 (Ar)** | DATA | **GPIO 14** |
| **DS18B20 (Solo)** | DATA (1-Wire) | **GPIO 13** *(Requer resistor pull-up 4.7kΩ)* |
| **Umidade do Solo** | A0 (Leitura Analógica ADC) | **GPIO 34** (ADC1) |
| **Botões Físicos** | Formatação SD / Forçar Modo AP | **GPIO 4** / **GPIO 15** *(Lógica Pull-up ativa)* |
| **LED Status** | Indicador Visual de Operação | **GPIO 2** |

---

## 🖥️ Esquema de Pinos - Central Receptora (RX)

| Componente | Sinal / Função | Pino no ESP32 |
| :--- | :--- | :--- |
| **Wi-Fi / Internet** | Gateway WebServer HTTP (`:80`) e sincronização NTP | Integrado ao ESP32 |
| **Display TFT ILI9341** | CS / RESET / D/C (Controle) | **GPIO 5** / **GPIO 4** / **GPIO 2** |
| | MOSI / SCK / LED (Backlight) | **GPIO 23** / **GPIO 18** / **3.3V** |
| **Módulo Cartão SD** | CS / MOSI / MISO / SCK (SPI) | **GPIO 15** / **GPIO 23** / **GPIO 19** / **GPIO 18** |
| **Encoder EC11** | CLK / DT / SW (Giro e Clique) | **GPIO 25** / **GPIO 26** / **GPIO 27** *(Anti-bouncing)* |

---

## 📁 Estrutura do Repositório

```text
├── sistema_web/                   # Plataforma Web & Servidor REST/WebSocket
│   ├── backend/                   # API Node.js (Express, PostgreSQL, Modelos FAO-56/VPD)
│   ├── frontend/                  # Dashboard Web React/Vite com Mapas e Catálogo Agronômico
│   ├── docker-compose.yml         # Orquestração do banco PostgreSQL e aplicação web
│   └── README.md                  # Instruções de execução local e via Docker
├── TX_Telemetria/                 # Firmware da Estação Transmissora Wi-Fi (TX)
│   └── TX_Telemetria.ino          # Envio Wi-Fi/HTTP com Portal Captivo, NTP Atômico e NVS
├── TX_G00002_TCall/               # Firmware da Estação Transmissora Celular GPRS (SIM800L)
│   └── TX_G00002_TCall.ino        # Envio autônomo 100% celular (GPRS/2G)
├── TX Telemetria.c                # Cópia espelho de referência do transmissor
├── RX lora TFT/
│   └── LoRa_RX_TFT_Database/      # Firmware da Central Receptora (RX Gateway Web)
│       ├── LoRa_RX_TFT_Database.ino
│       ├── TFT_UI.h
│       ├── TFT_UI.cpp
│       └── logo_montebot.h
├── teste_diagnostico_tx/          # Diagnóstico dos sensores da estação (TX)
│   └── teste_diagnostico_tx.ino
├── teste_diagnostico_rx/          # Diagnóstico dos periféricos do receptor (RX)
│   └── teste_diagnostico_rx.ino
└── teste_sd_rx/                   # Teste de desempenho SPI do cartão SD
    └── teste_sd_rx.ino
```

---

## 📝 Instruções de Uso e Configuração

### 1. Bibliotecas Necessárias
Instale na IDE do Arduino ou PlatformIO:
- `OneWire` e `DallasTemperature`
- `Adafruit_BMP280` e `Adafruit_VEML7700`
- `DHT sensor library`
- `ArduinoJson` (v6 ou v7)
- `Adafruit_GFX` e `Adafruit_ILI9341`

### 2. Primeiro Boot e Modo de Configuração AP (Portal Captivo)
- **Estação de Campo (TX):**
  - Se for o primeiro boot ou se o botão no **GPIO 15** estiver pressionado ao ligar, o TX iniciará o AP `GAIA-TX-[MAC]`.
  - Conecte o smartphone à rede Wi-Fi com a senha `gaiaconfig`.
  - O portal captivo abrirá automaticamente (ou acesse `http://192.168.4.1`).
  - No portal, visualize a auditoria de sensores em tempo real, selecione sua rede Wi-Fi, defina a chave AES-128 e o código da estação (ex: `G00001`). O relógio interno sincronizará automaticamente via NTP atômico assim que conectar à internet.
- **Central Receptora (RX):**
  - Segure o botão do Encoder (**GPIO 27**) ao ligar, ou segure-o por 5 segundos durante o uso, ou selecione "Configurar WiFi" no menu.
  - A tela TFT exibirá o painel "CONFIGURACAO AP" com o SSID `GAIA-RX-SETUP`, senha `gaiaconfig` e a contagem de aparelhos conectados.
  - Conecte pelo smartphone, defina as credenciais Wi-Fi e a URL do banco de dados/API.
  - Ao conectar ao Wi-Fi, a Central RX inicia o Gateway Web na porta 80 e fica pronta para receber telemetria via `/api/telemetria`.

