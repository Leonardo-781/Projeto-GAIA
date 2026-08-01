/**
 * ============================================================================
 * ARQUIVO: teste_sd_rx.ino
 * VERSÃO: 1.0
 * DATA: 2026-08-01
 * DESCRIÇÃO: Código de teste exclusivo para o leitor de cartão SD do receptor.
 *            Tenta inicializar o SD no GPIO 15, ler o tipo do cartão,
 *            escrever um arquivo de teste e lê-lo de volta.
 * COMUNICAÇÃO: ESP32 conectado na porta COM25.
 * ============================================================================
 */

#include <SPI.h>
#include <SD.h>

#define SD_CS 15

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("\n=============================================");
  Serial.println("       INICIANDO TESTE EXCLUSIVO DO SD       ");
  Serial.println("=============================================");

  // Inicializa barramento SPI com pinos padrão do ESP32
  // SCK = 18, MISO = 19, MOSI = 23
  SPI.begin(18, 19, 23);
  delay(100);

  Serial.print("Tentando inicializar o cartão SD no pino CS ");
  Serial.println(SD_CS);

  // Tenta iniciar a 4MHz (velocidade segura para testes)
  if (!SD.begin(SD_CS, SPI, 4000000)) {
    Serial.println("❌ FALHA: O cartão SD não pôde ser inicializado!");
    
    // Análise de diagnóstico
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
      Serial.println("➔ Causa provável: Cartão NÃO detectado no slot física ou erro de fiação (MISO/MOSI/SCK/CS).");
    } else {
      Serial.print("➔ Cartão detectado (Tipo ");
      Serial.print(cardType);
      Serial.println("), mas falhou ao montar a partição FAT32.");
      Serial.println("➔ Causa provável: Formatação incorreta (tente reformatar em FAT32 lenta) ou contatos sujos.");
    }
    return;
  }

  Serial.println("✅ SUCESSO: Cartão SD inicializado com sucesso!");

  // Exibe tipo do cartão
  uint8_t cardType = SD.cardType();
  Serial.print("➔ Tipo do Cartão: ");
  if (cardType == CARD_MMC) Serial.println("MMC");
  else if (cardType == CARD_SD) Serial.println("SDSC (Padrão/Antigo)");
  else if (cardType == CARD_SDHC) Serial.println("SDHC (Alta Capacidade)");
  else Serial.println("Desconhecido");

  // Exibe tamanho
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("➔ Tamanho do Cartão: %llu MB\n", cardSize);

  // Teste de Escrita de Arquivo
  Serial.println("\n[Teste 1] Escrevendo no cartão SD...");
  File file = SD.open("/teste_gaia.txt", FILE_WRITE);
  if (file) {
    file.println("Projeto GAIA - Teste de gravacao do SD OK!");
    file.close();
    Serial.println("➔ Escrita realizada com sucesso!");
  } else {
    Serial.println("❌ ERRO ao abrir arquivo para escrita!");
  }

  // Teste de Leitura de Arquivo
  Serial.println("\n[Teste 2] Lendo do cartão SD...");
  file = SD.open("/teste_gaia.txt", FILE_READ);
  if (file) {
    Serial.print("➔ Conteúdo do arquivo: ");
    while (file.available()) {
      Serial.write(file.read());
    }
    file.close();
    Serial.println("\n➔ Leitura concluída com sucesso!");
  } else {
    Serial.println("❌ ERRO ao abrir arquivo para leitura!");
  }

  // Limpa o arquivo de teste
  SD.remove("/teste_gaia.txt");
  Serial.println("\n=============================================");
  Serial.println("          TESTE DO SD FINALIZADO             ");
  Serial.println("=============================================");
}

void loop() {
  // Nada aqui
}
