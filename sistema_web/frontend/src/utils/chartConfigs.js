/**
 * Configurações Gráficas Chart.js - Projeto GAIA
 * Tema Utilitário de Engenharia Agroclimática
 */

export const chartGridColor = 'rgba(255, 255, 255, 0.05)';
export const chartBorderColor = 'rgba(255, 255, 255, 0.12)';
export const chartTextColor = '#a1a1aa'; // zinc-400

export function getBasePlugins(history) {
  return {
    legend: {
      position: 'top',
      align: 'end',
      labels: {
        color: chartTextColor,
        font: { size: 11, family: 'monospace', weight: 'bold' },
        boxWidth: 8,
        boxHeight: 8,
        usePointStyle: true,
        pointStyle: 'circle'
      }
    },
    tooltip: {
      backgroundColor: '#18181b', // zinc-900
      titleColor: '#e4e4e7',       // zinc-200
      bodyColor: '#a1a1aa',        // zinc-400
      borderColor: '#27272a',      // zinc-800
      borderWidth: 1,
      padding: 10,
      cornerRadius: 6,
      displayColors: true,
      boxWidth: 6,
      boxHeight: 6,
      titleFont: { size: 11, family: 'monospace' },
      bodyFont: { size: 11, family: 'monospace' },
      callbacks: {
        title: (items) => {
          if (!items || !items.length) return '';
          const idx = items[0].dataIndex;
          const h = history[idx];
          if (!h) return items[0].label;
          const d = h.data_hora ? new Date(h.data_hora) : new Date(h.timestamp_utc * 1000);
          return d.toLocaleString('pt-BR');
        }
      }
    }
  };
}

export function formatTemporalLabels(history, selectedPeriod) {
  return history.map(h => {
    const d = h.data_hora ? new Date(h.data_hora) : new Date(h.timestamp_utc * 1000);
    if (selectedPeriod === '24h' || selectedPeriod === '1d') {
      return d.toLocaleTimeString('pt-BR', { hour: '2-digit', minute: '2-digit' });
    }
    if (selectedPeriod === '7d') {
      const diaSemana = d.toLocaleDateString('pt-BR', { weekday: 'short' });
      return diaSemana.slice(0, 3) + ' ' + d.toLocaleTimeString('pt-BR', { hour: '2-digit', minute: '2-digit' });
    }
    if (selectedPeriod === '30d') {
      return d.toLocaleDateString('pt-BR', { day: '2-digit', month: '2-digit' });
    }
    const mes = d.toLocaleDateString('pt-BR', { month: 'short' });
    return d.getDate() + '/' + mes.slice(0, 3);
  });
}
