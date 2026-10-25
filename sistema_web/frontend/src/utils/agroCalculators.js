/**
 * Utilitários Agroclimáticos e Formatação Técnica - Projeto GAIA
 * Padrão de Engenharia Agrícola e Sensoriamento Remoto
 */

export function formatarDataHora(timestampUtc, dataHoraStr) {
  if (dataHoraStr) {
    const d = new Date(dataHoraStr);
    if (!isNaN(d.getTime())) return d.toLocaleString('pt-BR');
  }
  if (timestampUtc) {
    return new Date(timestampUtc * 1000).toLocaleString('pt-BR');
  }
  return 'Aguardando telemetria';
}

export function formatarTempoRelativo(segundos) {
  if (segundos === null || segundos === undefined) return 'Sem dados';
  if (segundos < 60) return `há ${segundos}s`;
  if (segundos < 3600) return `há ${Math.floor(segundos / 60)}m`;
  if (segundos < 86400) return `há ${Math.floor(segundos / 3600)}h`;
  return `há ${Math.floor(segundos / 86400)}d`;
}

export function formatarCoordenadas(lat, lng) {
  if (lat === null || lat === undefined || lng === null || lng === undefined) return '--';
  const latDir = lat >= 0 ? 'N' : 'S';
  const lngDir = lng >= 0 ? 'L' : 'O';
  return `${Math.abs(lat).toFixed(5)}° ${latDir}, ${Math.abs(lng).toFixed(5)}° ${lngDir}`;
}

export function classificarVPD(vpdVal) {
  if (vpdVal === null || vpdVal === undefined || isNaN(vpdVal)) {
    return { status: 'ND', label: 'Indisponível', color: 'text-zinc-400', bg: 'bg-zinc-800' };
  }
  const v = parseFloat(vpdVal);
  if (v < 0.4) {
    return {
      status: 'SATURADO',
      label: 'Saturação / Baixa Transpiração (Risco Fúngico)',
      color: 'text-sky-400',
      bg: 'bg-sky-950/70 border-sky-800/60',
      badge: 'bg-sky-900/60 text-sky-300'
    };
  }
  if (v <= 1.25) {
    return {
      status: 'IDEAL',
      label: 'Transpiração Ótima (Equilíbrio Fotossintético)',
      color: 'text-emerald-400',
      bg: 'bg-emerald-950/70 border-emerald-800/60',
      badge: 'bg-emerald-900/60 text-emerald-300'
    };
  }
  if (v <= 1.6) {
    return {
      status: 'MODERADO',
      label: 'Transpiração Acelerada (Atenção à Irrigação)',
      color: 'text-amber-400',
      bg: 'bg-amber-950/70 border-amber-800/60',
      badge: 'bg-amber-900/60 text-amber-300'
    };
  }
  return {
    status: 'SEVERO',
    label: 'Estresse Hídrico Crítico (Fechamento Estomático)',
    color: 'text-rose-400',
    bg: 'bg-rose-950/70 border-rose-800/60',
    badge: 'bg-rose-900/60 text-rose-300'
  };
}

export function classificarRiscoFungo(rf) {
  if (rf === 2) {
    return { nivel: 'ALTO', label: 'Alto Risco (Folhas Molhadas)', color: 'text-rose-400', bg: 'bg-rose-950/50 border-rose-900' };
  }
  if (rf === 1) {
    return { nivel: 'MODERADO', label: 'Risco Moderado (Atenção)', color: 'text-amber-400', bg: 'bg-amber-950/50 border-amber-900' };
  }
  return { nivel: 'BAIXO', label: 'Baixo Risco (Folhas Secas)', color: 'text-emerald-400', bg: 'bg-zinc-900/70 border-zinc-800' };
}

export function classificarRiscoGeada(rg) {
  if (rg === 2) {
    return { nivel: 'CRITICO', label: 'Perigo: Geada Negra', color: 'text-purple-400', bg: 'bg-purple-950/60 border-purple-900' };
  }
  if (rg === 1) {
    return { nivel: 'ALERTA', label: 'Risco de Geada Branca', color: 'text-sky-400', bg: 'bg-sky-950/50 border-sky-900' };
  }
  return { nivel: 'NENHUM', label: 'Sem Risco de Geada', color: 'text-zinc-300', bg: 'bg-zinc-900/70 border-zinc-800' };
}

export function classificarITU(ituVal) {
  if (!ituVal || isNaN(ituVal)) return { nivel: 'CALCULANDO', label: 'Calculando...', color: 'text-zinc-400', bg: 'bg-zinc-900 border-zinc-800' };
  const itu = parseFloat(ituVal);
  if (itu >= 79) {
    return { nivel: 'CRITICO', label: `Crítico (ITU ${itu.toFixed(1)}) - Estresse Térmico`, color: 'text-rose-400', bg: 'bg-rose-950/60 border-rose-900' };
  }
  if (itu >= 72) {
    return { nivel: 'ALERTA', label: `Alerta (ITU ${itu.toFixed(1)}) - Atenção`, color: 'text-amber-400', bg: 'bg-amber-950/60 border-amber-900' };
  }
  return { nivel: 'CONFORTO', label: `Conforto Pleno (ITU ${itu.toFixed(1)})`, color: 'text-emerald-400', bg: 'bg-zinc-900/70 border-zinc-800' };
}
