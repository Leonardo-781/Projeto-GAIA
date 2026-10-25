import { useState, useEffect, useRef } from 'react';

export function useLiveSocket(onNovaLeitura) {
  const [wsConnected, setWsConnected] = useState(false);
  const [latencyMs, setLatencyMs] = useState(null);
  const wsRef = useRef(null);

  useEffect(() => {
    let reconnectTimeout;
    const connect = () => {
      const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
      const wsUrl = protocol + '//' + window.location.host;
      const ws = new WebSocket(wsUrl);
      wsRef.current = ws;

      const t0 = performance.now();

      ws.onopen = () => {
        setWsConnected(true);
        setLatencyMs(Math.round(performance.now() - t0));
      };

      ws.onclose = () => {
        setWsConnected(false);
        setLatencyMs(null);
        reconnectTimeout = setTimeout(connect, 3000);
      };

      ws.onerror = () => {
        setWsConnected(false);
      };

      ws.onmessage = (event) => {
        try {
          const msg = JSON.parse(event.data);
          if (msg.type === 'NOVA_LEITURA' && onNovaLeitura) {
            onNovaLeitura(msg.data);
          }
        } catch (e) {
          console.error('Erro ao processar mensagem WS:', e);
        }
      };
    };

    connect();

    return () => {
      if (reconnectTimeout) clearTimeout(reconnectTimeout);
      if (wsRef.current) wsRef.current.close();
    };
  }, [onNovaLeitura]);

  return { wsConnected, latencyMs };
}
