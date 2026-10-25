# 📡 Padrão de Comunicação: ESP32 (GAIA TX) ➔ Servidor

Este documento especifica o padrão exato de comunicação enviado pelo microcontrolador ESP32 para o servidor, detalhando o protocolo, o envelope criptográfico, o algoritmo de decodificação e o dicionário de campos do JSON de telemetria.

---

## 1. Protocolo de Rede & Requisição HTTP

O ESP32 realiza uma requisição HTTP padrão para o endpoint configurado na estação:

* **Método HTTP:** `POST`
* **Cabeçalho obrigatório:** `Content-Type: application/json`
* **Timeout configurado no ESP32:** 5 segundos para conexão / 10 segundos para resposta
* **Endpoint de Destino:** Definido no ESP32 (Exemplo: `http://SEU_SERVIDOR/api/telemetria` ou `http://SEU_SERVIDOR/maria-web/`)

---

## 2. O que Chega no Servidor (Corpo da Requisição)

O corpo (`body`) da requisição HTTP recebida pelo servidor é um objeto JSON contendo o campo `"data"` com uma string em Base64:

```json
{
  "data": "s8F3gK91lA...[STRING BASE64]...=="
}
```

Essa string contém o **Vetor de Inicialização (IV)** concatenado com os **dados criptografados em AES-128-CBC**.

---

## 3. Como o Servidor Deve Processar e Decifrar

### Parâmetros de Criptografia:
* **Algoritmo:** `AES-128-CBC`
* **Tamanho da Chave:** 16 bytes (128 bits) — Chave Oficial do Projeto GAIA: `"GaiaAgroIoT2026!"` (com retrocompatibilidade para `"MARIACRYPTOKEY12"`).
* **Preenchimento (Padding):** `PKCS#7`
* **Vetor de Inicialização (IV):** 16 bytes gerados aleatoriamente por hardware no ESP32 a cada envio.
* **Token de Autenticação da Estação:** `"token": "gaia_sec_G00001_..."` (incluso dentro do JSON cifrado ou no cabeçalho `X-Station-Token`).

### Passo a Passo da Decodificação:
1. O servidor recebe o JSON e obtém o valor da chave `"data"`.
2. Decodifica a string Base64 para um buffer de bytes brutos.
3. **Extrai os primeiros 16 bytes:** este é o **IV** (`iv = buffer[0..15]`).
4. **Extrai os bytes restantes:** este é o **Ciphertext** (`ciphertext = buffer[16..fim]`).
5. Decifra o `ciphertext` com AES-128-CBC usando a chave `"GaiaAgroIoT2026!"` e o `IV` extraído.
6. Remove o padding PKCS#7 para obter a string JSON em texto claro.
7. Valida o `token` da estação e grava a telemetria.

---

## 4. Exemplos de Decodificação no Servidor

### Em Python
```python
import base64, json
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives import padding

def decifrar_pacote_gaia(data_b64, chave_16_caracteres="GaiaAgroIoT2026!"):
    raw = base64.b64decode(data_b64)
    iv = raw[:16]           # Primeiros 16 bytes = IV
    ciphertext = raw[16:]   # Restante = dados cifrados
    
    key = chave_16_caracteres.encode('utf-8')[:16].ljust(16, b'0')
    cipher = Cipher(algorithms.AES(key), modes.CBC(iv))
    decryptor = cipher.decryptor()
    padded_plain = decryptor.update(ciphertext) + decryptor.finalize()
    
    unpadder = padding.PKCS7(128).unpadder()
    plain_text = (unpadder.update(padded_plain) + unpadder.finalize()).decode('utf-8')
    return json.loads(plain_text)
```

### Em Node.js / JavaScript
```javascript
const crypto = require('crypto');

function decifrarPacoteGaia(dataB64, chave16 = 'GaiaAgroIoT2026!') {
  const raw = Buffer.from(dataB64, 'base64');
  const iv = raw.subarray(0, 16);           // Primeiros 16 bytes
  const ciphertext = raw.subarray(16);      // Restante
  const key = Buffer.from(chave16.padEnd(16, '0').slice(0, 16), 'utf8');

  const decipher = crypto.createDecipheriv('aes-128-cbc', key, iv);
  let decrypted = decipher.update(ciphertext, null, 'utf8');
  decrypted += decipher.final('utf8');
  return JSON.parse(decrypted);
}
```

### Em PHP
```php
function decifrarPacoteGaia($dataB64, $chave16 = "MARIACRYPTOKEY12") {
    $raw = base64_decode($dataB64);
    $iv = substr($raw, 0, 16);             // Primeiros 16 bytes
    $ciphertext = substr($raw, 16);        // Restante
    $key = substr(str_pad($chave16, 16, "0"), 0, 16);

    $decrypted = openssl_decrypt($ciphertext, 'aes-128-cbc', $key, OPENSSL_RAW_DATA, $iv);
    return json_decode($decrypted, true);
}
```

---

## 5. Estrutura do JSON Decifrado (Payload Real)

Após a decifragem, o servidor obtém o seguinte objeto JSON:

```json
{
  "codSta": "G00001",
  "ts": 1726830000,
  "tD": 24.5,
  "h": 68.0,
  "tB": 24.3,
  "p": 1013.2,
  "l": 35200.0,
  "s": 58,
  "tS": 22.1,
  "tRef": 24.4,
  "dT": 0.2,
  "q": 100,
  "po": 18.1,
  "vpd": 0.98,
  "et0": 0.038,
  "etc": 0.038,
  "rs": 278.1,
  "par": 584.0,
  "rf": 0,
  "rg": 0,
  "dp": 0.3,
  "at": false,
  "itu": 73.1,
  "hi": 25.4,
  "heapMod": 412
}
```

---

## 6. Dicionário de Campos e Tipos de Dados

| Campo | Tipo | Origem | Descrição e Unidade |
| :--- | :---: | :--- | :--- |
| **`codSta`** | `string` | NVS | Código identificador da estação (6 caracteres, ex: `"G00001"`). |
| **`ts`** | `uint32` | Relógio Atômico | Timestamp Unix Epoch em segundos UTC (sincronizado via NTP.br). |
| **`tD`** | `float` | Sensor DHT22 | Temperatura do ar (°C). Se desconectado, envia `null`. |
| **`h`** | `float` | Sensor DHT22 | Umidade relativa do ar (%). Se desconectado, envia `null`. |
| **`tB`** | `float` | Sensor BMP280 | Temperatura barométrica (°C). Se desconectado, envia `null`. |
| **`p`** | `float` | Sensor BMP280 | Pressão atmosférica real na estação (hPa). |
| **`l`** | `float` | Sensor VEML7700 | Luminosidade solar em Lux. Se desconectado, envia `null`. |
| **`s`** | `int` | Sensor de Solo (ADC) | Umidade do solo calibrada (0 a 100%). Se desconectado, envia `null`. |
| **`tS`** | `float` | Sensor DS18B20 | Temperatura do solo a nível radicular (°C). Se desconectado: `null`. |
| **`tRef`** | `float` | Algoritmo | Temperatura de referência do ar consolidada (°C). |
| **`dT`** | `float` | Algoritmo | Divergência térmica entre DHT22 e BMP280: $\|tD - tB\|$ (°C). |
| **`q`** | `int` | Algoritmo | Índice de confiabilidade e integridade das leituras (0 a 100%). |
| **`po`** | `float` | Cálculo | Ponto de Orvalho (°C) via Magnus-Tetens. |
| **`vpd`** | `float` | Cálculo | Déficit de Pressão de Vapor (kPa). (*Ideal: 0.4 a 1.4 kPa*). |
| **`et0`** | `float` | Cálculo | Evapotranspiração de Referência FAO-56 Penman-Monteith (mm). |
| **`etc`** | `float` | Cálculo | Evapotranspiração da Cultura ($ET_c = ET_0 \times K_c$). |
| **`rs`** | `float` | Cálculo | Radiação Solar Global estimada ($W/m^2$). |
| **`par`** | `float` | Cálculo | Fluxo Fotossinteticamente Ativo / PPFD ($\mu mol/m^2\cdot s$). |
| **`rf`** | `int` | Fitossanidade | Risco Fúngico de Folha Molhada: `0` = Baixo, `1` = Moderado, `2` = **Alto / Alerta**. |
| **`rg`** | `int` | Fitossanidade | Risco de Geada: `0` = Sem Risco, `1` = Geada Branca, `2` = **Geada Negra**. |
| **`dp`** | `float` | Barometria | Tendência barométrica das últimas 3 horas $\Delta P_{3h}$ (hPa). |
| **`at`** | `bool` | Barometria | Alerta de Tempestade / Queda brusca de pressão (`true` ou `false`). |
| **`itu`** | `float` | Bioclima | Índice de Conforto Zootécnico (Thom / NRC). (*Alerta $\ge 72$, Crítico $\ge 79$*). |
| **`hi`** | `float` | Bioclima | Sensação Térmica / Índice de Calor Humano (°C). |
| **`heapMod`**| `int` | ESP32 | Diagnóstico de integridade da memória RAM livre (`freeHeap % 1000`). |

---

## 7. Resposta Esperada pelo ESP32

Para que o ESP32 confirme que o dado foi entregue e possa entrar em modo de economia de energia (**Deep Sleep**):

* O servidor **deve responder com status HTTP entre `200` e `299`** (exemplo: `HTTP 200 OK`).
* O corpo da resposta pode ser um JSON simples, como:
  ```json
  {"status": "sucesso"}
  ```
* **Comportamento em caso de falha:** Se o servidor retornar qualquer outro código (ex: `500`, `404`, `400`) ou demorar mais de 10 segundos para responder, o ESP32 entenderá que a transmissão falhou e salvará o pacote na **fila offline do Cartão SD (`/fila.txt`)** para reenviar assim que a conexão for restabelecida.
