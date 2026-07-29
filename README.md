# Projeto GAIA - Estação Agroclimática e Meteorológica Inteligente

O **Projeto GAIA** é um sistema de monitoramento agrícola e telemetria de campo autônomo baseado no microcontrolador ESP32. Ele é composto por duas unidades que se comunicam de forma segura e criptografada: a **Estação de Campo (Transmissora - TX)** e a **Central Receptora (RX)** com display colorido TFT e controle físico via Encoder Rotativo.

---

## 🚀 Principais Características

* **Criptografia Militar:** Comunicação segura (LoRa ou Wi-Fi) criptografada via hardware com **AES-128-CBC** (usando MbedTLS do ESP32) e IV randômico, evitando interceptação ou injeção de dados falsos na rede.
* **Inteligência na Borda (Física Climática):** A própria estação de campo calcula em tempo real o **Ponto de Orvalho**, o **Déficit de Pressão de Vapor (VPD)** e a **Evapotranspiração de Referência ($ET_0$)** usando o modelo Penman-Monteith (FAO-56) simplificado.
* **Resiliência a Falhas (Fila SD Otimizada):** Se a rede ou rádio caírem, os dados são enfileirados de forma segura e compacta no Cartão SD e reenviados automaticamente quando a conexão retornar, impedindo estouro de memória (Heap) ou perda de histórico.
* **Deep Sleep com Alinhamento Temporal:** A estação entra em sono profundo e acorda em slots de tempo exatos ajustados pelo relógio do RTC DS3231 (minutos múltiplos de 5, 10 ou 15), aplicando um offset anti-colisão específico para cada placa.
* **Interface Responsiva Premium:** Portais de configuração Web (*Dark Mode Glassmorphism*) integrados para ajustar credenciais de rede, sincronizar o RTC via navegador do celular e parear a chave criptográfica simétrica (validada em tempo real com comprimento exato de 16 caracteres).

---

## 🛠️ Esquema de Pinos - Estação de Campo (TX)

| Componente | Sinal / Função | Pino no ESP32 |
| :--- | :--- | :--- |
| **Módulo LoRa E220** | RXD / TXD (Dados) | **GPIO 17** (TX2) / **GPIO 16** (RX2) |
| | M0 / M1 / AUX (Controles) | **GPIO 25** / **GPIO 26** / **GPIO 27** |
| **Leitor Cartão SD** | CS / MOSI / MISO / SCK (SPI) | **GPIO 5** / **GPIO 23** / **GPIO 19** / **GPIO 18** |
| **Barramento I2C** | SDA / SCL (BMP280, VEML7700, RTC) | **GPIO 21** / **GPIO 22** |
| **DHT22 (Ar)** | DATA | **GPIO 14** |
| **DS18B20 (Solo)** | DATA (1-Wire) | **GPIO 13** *(Requer resistor pull-up 4.7kΩ)* |
| **Umidade Solo** | A0 (Leitura Analógica) | **GPIO 34** (ADC1) |
| **Botões** | Formatação SD / Modo AP | **GPIO 4** / **GPIO 15** *(Lógica Pull-up ativa)* |
| **LED Status** | Indicador Visual | **GPIO 2** |

---

## 🖥️ Esquema de Pinos - Central Receptora (RX)

| Componente | Sinal / Função | Pino no ESP32 |
| :--- | :--- | :--- |
| **Display TFT ILI9341** | CS / RESET / D/C (Controle) | **GPIO 5** / **GPIO 4** / **GPIO 2** |
| | MOSI / SCK / LED (Backlight) | **GPIO 23** / **GPIO 18** / **3.3V** |
| **Módulo SD Extra** | CS / MOSI / MISO / SCK (SPI) | **GPIO 15** / **GPIO 23** / **GPIO 19** / **GPIO 18** |
| **Módulo LoRa E220** | RXD / TXD | **GPIO 17** (TX2) / **GPIO 16** (RX2) *(M0/M1 ligados no GND)* |
| **Encoder EC11** | CLK / DT / SW (Giro e Clique) | **GPIO 25** / **GPIO 26** / **GPIO 27** *(Navegação anti-bouncing)* |

---

## 📁 Estrutura do Repositório

```text
├── teste_diagnostico_tx/          # Código de teste unitário dos sensores da estação (TX)
│   └── teste_diagnostico_tx.ino   # Sketch de diagnóstico serial na COM25
├── RX lora TFT/
│   └── LoRa_RX_TFT_Database/      # Firmware da Central Receptora (RX)
│       ├── LoRa_RX_TFT_Database.ino
│       ├── TFT_UI.h
│       ├── TFT_UI.cpp
│       └── logo_montebot.h
└── TX Telemetria.c                # Firmware da Estação Transmissora (TX)
```

---

## 📝 Instruções de Uso
1. **Configuração Física:** Conecte os sensores e periféricos nas respectivas portas do ESP32 seguindo a fiação descrita nas tabelas.
2. **Bibliotecas Necessárias:** Instale na IDE do Arduino / PlatformIO: `OneWire`, `DallasTemperature`, `Adafruit_BMP280`, `Adafruit_VEML7700`, `DHT sensor library`, `RTClib`, `ArduinoJson` e `LoRa_E220`.
3. **Primeiro Boot (Configuração):** Segure o botão conectado no **GPIO 15** ao energizar qualquer uma das placas para forçar o início em modo **Access Point (AP)**.
   * Conecte-se à rede Wi-Fi `GAIA-TX-[MAC]` ou `GAIA-RX-SETUP` usando a senha `gaiaconfig`.
   * Acesse `http://192.168.4.1` no seu celular ou computador.
   * Defina a **chave criptográfica de 16 caracteres** (que deve ser exatamente igual na Estação e no Receptor), credenciais do seu roteador Wi-Fi local e salve. As placas reiniciarão e começarão a telemetria segura automaticamente.
