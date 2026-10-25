#!/usr/bin/env bash

LOG_FILE="/home/leo/server/data/cloudflared.log"
URL_FILE="/home/leo/server/URL_PUBLICA_GAIA.txt"

# Limpa o log anterior para garantir leitura da URL atual
> "$LOG_FILE"

# Executa cloudflared e grava os logs
cloudflared tunnel --url http://localhost:3000 --logfile "$LOG_FILE" &
PID=$!

# Aguarda a URL aparecer no log
for i in {1..30}; do
  if [ -f "$LOG_FILE" ]; then
    URL=$(grep -oE "https://[a-zA-Z0-9-]+\.trycloudflare\.com" "$LOG_FILE" | tail -n 1)
    if [ -n "$URL" ]; then
      echo "$URL" > "$URL_FILE"
      echo "[GAIA TUNNEL] URL ativa: $URL"
      break
    fi
  fi
  sleep 1
done

wait $PID
