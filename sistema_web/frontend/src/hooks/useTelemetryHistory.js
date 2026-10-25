import { useState, useCallback } from 'react';

export function useTelemetryHistory(getAuthHeaders) {
  const [stationHistory, setStationHistory] = useState([]);
  const [selectedPeriod, setSelectedPeriod] = useState('24h');
  const [loadingHistory, setLoadingHistory] = useState(false);

  const carregarHistorico = useCallback(async (cod, periodo = selectedPeriod) => {
    if (!cod) return;
    setLoadingHistory(true);
    try {
      const res = await fetch('/api/telemetria/historico?estacao=' + cod + '&periodo=' + periodo, {
        headers: getAuthHeaders ? getAuthHeaders() : {}
      });
      if (res.ok) {
        const data = await res.json();
        setStationHistory(Array.isArray(data) ? data : []);
      } else {
        setStationHistory([]);
      }
    } catch (err) {
      console.error('Erro ao carregar histórico:', err);
      setStationHistory([]);
    } finally {
      setLoadingHistory(false);
    }
  }, [getAuthHeaders, selectedPeriod]);

  const mudarPeriodo = useCallback((novoPeriodo, codEstacao) => {
    setSelectedPeriod(novoPeriodo);
    if (codEstacao) {
      carregarHistorico(codEstacao, novoPeriodo);
    }
  }, [carregarHistorico]);

  const exportarCSV = (station) => {
    if (!stationHistory || stationHistory.length === 0 || !station) {
      alert('Não há dados telemétricos de campo registrados para exportação.');
      return;
    }
    const headers = [
      'ID', 'Data_Hora', 'Timestamp_UTC', 'Cod_Estacao',
      'Temp_Ar_C', 'Umid_Ar_Pct', 'Umid_Solo_Pct', 'Temp_Solo_C', 'Pressao_hPa', 'Lux',
      'VPD_kPa', 'ET0_mm', 'ETc_mm', 'Rad_Solar_Rs_Wm2', 'PAR_umol', 'Risco_Fungo', 'Risco_Geada',
      'Delta_P_hPa', 'Alerta_Tempestade', 'ITU', 'Heat_Index_HI', 'Qualidade_Pct', 'Heap_RAM_KB'
    ];
    const rows = stationHistory.map(h => [
      h.id,
      h.data_hora || (h.timestamp_utc ? new Date(h.timestamp_utc * 1000).toISOString() : ''),
      h.timestamp_utc || '',
      h.cod_estacao,
      h.temp_ref || h.temp_dht || '',
      h.umidade || '',
      h.umidade_solo !== null && h.umidade_solo !== undefined ? h.umidade_solo : '',
      h.temp_solo || '',
      h.pressao || '',
      h.lux || '',
      h.vpd || '',
      h.et0 || '',
      h.etc || '',
      h.rs || '',
      h.par || '',
      h.rf || 0,
      h.rg || 0,
      h.dp || '',
      h.at ? 1 : 0,
      h.itu || '',
      h.hi || '',
      h.qualidade || 100,
      h.heap_mod || ''
    ]);

    const csvContent = 'data:text/csv;charset=utf-8,' + [headers.join(';'), ...rows.map(e => e.join(';'))].join('\n');
    const encodedUri = encodeURI(csvContent);
    const link = document.createElement('a');
    link.setAttribute('href', encodedUri);
    link.setAttribute('download', 'telemetria_' + station.cod_estacao + '_' + new Date().toISOString().slice(0, 10) + '.csv');
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
  };

  return {
    stationHistory,
    setStationHistory,
    selectedPeriod,
    loadingHistory,
    carregarHistorico,
    mudarPeriodo,
    exportarCSV
  };
}
