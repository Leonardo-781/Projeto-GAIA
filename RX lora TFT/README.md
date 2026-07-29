# Testes TFT - ESP32

Pasta criada para guardar sketches de diagnostico do display e reutilizar depois.

## Arquivos

- `teste_tft_horimetro.ino`  
  Teste visual baseado no layout do horimetro.

- `teste_tft_upload.ino`  
  Teste simples em TFT_eSPI com atualizacao de tela.

- `tft_diag_ili9341.ino`  
  Diagnostico com pinos explicitos via Adafruit_ILI9341 (funcionou no seu display).

## Upload rapido com Arduino CLI

No PowerShell, entre na pasta do sketch escolhido e rode:

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32 NOME_DO_SKETCH.ino
arduino-cli upload -p COM25 --fqbn esp32:esp32:esp32 NOME_DO_SKETCH.ino
```

Exemplo:

```powershell
Set-Location "c:\Users\Leonardo\OneDrive\Documentos\VS Code\Estagio Gh2o\Horimetro\Testes_TFT"
arduino-cli compile --fqbn esp32:esp32:esp32 tft_diag_ili9341.ino
arduino-cli upload -p COM25 --fqbn esp32:esp32:esp32 tft_diag_ili9341.ino
```
