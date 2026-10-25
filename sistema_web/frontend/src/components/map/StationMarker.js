import L from 'leaflet';

/**
 * Cria marcador técnico de telemetria agroclimática (Estilo Baliza/Torre de Campo)
 * Livre de estética de IA: design sóbrio, funcional e de alta legibilidade cartográfica
 */
export function createStationMarkerIcon(st, isSelected = false) {
  const isOnline = st.status === 'ONLINE';
  const isSleep = st.status === 'DEEP_SLEEP';
  
  // Cores de status técnico (Padrão Industrial)
  const statusColor = isOnline ? '#22c55e' : (isSleep ? '#f59e0b' : '#ef4444');
  const ringColor = isSelected ? '#38bdf8' : (isOnline ? '#22c55e' : '#71717a');
  const isPrivate = st.visibilidade === 'PRIVADA';
  const isPending = st.status_financeiro === 'PENDENTE';
  
  // Abreviação do código da estação (ex: G00001 -> G01)
  const shortCode = (st.cod_estacao || 'ST').replace(/^G0*/i, 'G');

  const html = `
    <div class="technical-marker-container ${isSelected ? 'selected' : ''}" style="--status-color: ${statusColor}; --ring-color: ${ringColor}">
      <div class="station-beacon">
        <span class="beacon-led" style="background-color: ${statusColor};"></span>
        <span class="beacon-code">${shortCode}</span>
        ${isPrivate ? '<span class="beacon-badge" title="Estação Privada">🔒</span>' : ''}
        ${isPending ? '<span class="beacon-badge" title="Faturamento Pendente">⚠️</span>' : ''}
      </div>
      <div class="beacon-stem"></div>
      <div class="beacon-base"></div>
    </div>
  `;

  return L.divIcon({
    className: 'gaia-custom-leaflet-marker',
    html: html,
    iconSize: [48, 52],
    iconAnchor: [24, 52],
    popupAnchor: [0, -54]
  });
}
