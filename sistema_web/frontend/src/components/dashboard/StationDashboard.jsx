import React from 'react';
import {
  ArrowLeft,
  Calendar,
  Download,
  Settings,
  RefreshCw,
  Radio,
  Sprout,
  Activity,
  Bug,
  Snowflake,
  Flame
} from 'lucide-react';
import MetricCards from './MetricCards';
import SubsystemCharts from './SubsystemCharts';
import StationSecurity from '../station/StationSecurity';
import { formatarCoordenadas, formatarDataHora } from '../../utils/agroCalculators';

export default function StationDashboard({
  station,
  stationHistory,
  selectedPeriod,
  onPeriodChange,
  loadingHistory,
  onBackToMap,
  onExportCSV,
  onOpenSettings,
  user,
  onOpenAuth
}) {
  if (!station) {
    return (
      <div className="flex-1 flex flex-col items-center justify-center p-6 text-center bg-zinc-950 font-mono text-zinc-400">
        <Radio className="w-12 h-12 text-zinc-600 mb-3" />
        <h2 className="text-base font-bold text-zinc-200">NENHUMA ESTAÇÃO SELECIONADA</h2>
        <p className="text-xs text-zinc-500 mt-1 max-w-sm">Selecione uma estação no mapa ou na lista de frota lateral para inspecionar os subsistemas telemétricos.</p>
        <button
          onClick={onBackToMap}
          className="mt-4 flex items-center gap-2 px-3 py-2 rounded bg-zinc-900 border border-zinc-700 text-xs text-zinc-300 hover:text-white hover:bg-zinc-800 transition cursor-pointer"
        >
          <ArrowLeft className="w-3.5 h-3.5" />
          <span>RETORNAR AO MAPA</span>
        </button>
      </div>
    );
  }

  // Se a estação for privada ou pendente e o usuário não estiver logado
  const isLocked = (station.visibilidade === 'PRIVADA' || station.status_financeiro === 'PENDENTE') && !user;

  const periods = [
    { id: '24h', label: '24H' },
    { id: '7d', label: '7D' },
    { id: '30d', label: '30D' },
    { id: '90d', label: '90D' },
    { id: '1a', label: '1 ANO' }
  ];

  return (
    <div className="flex-1 flex flex-col overflow-y-auto bg-zinc-950 text-zinc-100 select-none">
      
      {/* Barra de Comando do Dashboard */}
      <div className="p-3 sm:px-6 bg-zinc-900/90 border-b border-zinc-800 flex flex-col md:flex-row md:items-center justify-between gap-2.5 sticky top-0 z-10 backdrop-blur-md">
        
        {/* Esquerda: Voltar + Info Estação */}
        <div className="flex items-center gap-2.5 min-w-0">
          <button
            onClick={onBackToMap}
            className="flex items-center gap-1.5 px-2.5 py-1.5 rounded bg-zinc-800 hover:bg-zinc-700 text-zinc-300 hover:text-white border border-zinc-700 text-xs font-mono transition cursor-pointer shrink-0"
            title="Voltar ao Mapa Geral"
          >
            <ArrowLeft className="w-3.5 h-3.5" />
            <span className="text-xs">MAPA</span>
          </button>

          <div className="border-l border-zinc-700 pl-2.5 min-w-0">
            <div className="flex items-center gap-2 flex-wrap">
              <span className="font-mono font-bold text-sm text-zinc-100 tracking-wider">
                {station.cod_estacao}
              </span>
              <span className="text-zinc-300 text-xs truncate max-w-[150px] sm:max-w-none">
                {station.nome || 'Estação'}
              </span>
              <span className="text-[10px] font-mono px-1.5 py-0.2 rounded bg-emerald-950 text-emerald-300 border border-emerald-800">
                {station.cultura || 'Café'}
              </span>
              <span className={`text-[9px] font-mono px-1.5 py-0.2 rounded border ${
                station.status === 'ONLINE' ? 'bg-emerald-950/60 border-emerald-700 text-emerald-400' : 'bg-zinc-800 border-zinc-700 text-zinc-400'
              }`}>
                {station.status}
              </span>
            </div>
            <div className="text-[10px] font-mono text-zinc-500 mt-0.5 truncate">
              {formatarDataHora(station.timestamp_utc, station.data_hora)}
            </div>
          </div>
        </div>

        {/* Direita: Seletor de Período Temporal + Ações de Engenharia */}
        <div className="flex items-center justify-between md:justify-end gap-2 overflow-x-auto no-scrollbar py-0.5">
          
          <div className="flex items-center bg-zinc-950 p-0.5 rounded border border-zinc-800 font-mono text-xs shrink-0">
            {periods.map(p => (
              <button
                key={p.id}
                onClick={() => onPeriodChange(p.id, station.cod_estacao)}
                className={`px-2 py-1 rounded text-[10px] sm:text-[11px] transition cursor-pointer ${
                  selectedPeriod === p.id
                    ? 'bg-zinc-800 text-emerald-400 font-bold border border-zinc-700'
                    : 'text-zinc-400 hover:text-zinc-200'
                }`}
              >
                {p.label}
              </button>
            ))}
          </div>

          {/* Botões de Ação */}
          <div className="flex items-center gap-1 shrink-0">
            <button
              onClick={() => onExportCSV(station)}
              title="Exportar dados do período em formato CSV"
              className="p-1.5 sm:p-2 rounded bg-zinc-800 hover:bg-zinc-700 text-zinc-300 hover:text-white border border-zinc-700 text-xs font-mono transition cursor-pointer flex items-center gap-1.5"
            >
              <Download className="w-3.5 h-3.5" />
              <span className="hidden sm:inline text-xs font-semibold">CSV</span>
            </button>

            {user && (
              <button
                onClick={() => onOpenSettings(station)}
                title="Configurações da Estação"
                className="p-1.5 sm:p-2 rounded bg-zinc-800 hover:bg-zinc-700 text-zinc-300 hover:text-white border border-zinc-700 text-xs font-mono transition cursor-pointer"
              >
                <Settings className="w-3.5 h-3.5" />
              </button>
            )}
          </div>

        </div>

      </div>

      {/* Conteúdo Principal do Dashboard */}
      <div className="p-3 sm:p-6 space-y-5">
        
        {isLocked ? (
          <StationSecurity station={station} onLoginClick={onOpenAuth} />
        ) : (
          <>
            {/* Banner de Calibração Agronômica Vinculada (FAO-56 / EMBRAPA) */}
            {station.cultura_perfil && (
              <div className="bg-zinc-900/90 border border-emerald-900/50 rounded-lg p-3 sm:p-4 font-mono shadow-sm">
                <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-1.5 border-b border-zinc-800 pb-2.5 mb-3">
                  <div className="flex items-center gap-2">
                    <Sprout className="w-4 h-4 text-emerald-400 shrink-0" />
                    <span className="font-bold text-xs uppercase tracking-wider text-zinc-100">
                      CALIBRAÇÃO AGRONÔMICA: {station.cultura_perfil.nome}
                    </span>
                    <span className="text-[10px] px-1.5 py-0.2 rounded bg-emerald-950 text-emerald-300 border border-emerald-800 font-bold">
                      {station.cultura_perfil.categoria}
                    </span>
                  </div>
                  <span className="text-[10px] text-zinc-400 font-semibold">
                    NORMA FAO-56 / EMBRAPA
                  </span>
                </div>

                <div className="grid grid-cols-2 sm:grid-cols-5 gap-2.5 text-xs">
                  <div className="bg-zinc-950 p-2.5 rounded border border-zinc-800/80">
                    <span className="text-[10px] text-zinc-500 block">KC DE MANEJO</span>
                    <span className="font-bold text-emerald-400 text-sm">{station.cultura_perfil.kc_medio}</span>
                    <span className="text-[9px] text-zinc-500 block mt-0.5">Ini: {station.cultura_perfil.kc_inicial} | Fim: {station.cultura_perfil.kc_final}</span>
                  </div>

                  <div className="bg-zinc-950 p-2.5 rounded border border-zinc-800/80">
                    <span className="text-[10px] text-zinc-500 block">FAIXA TÉRMICA ÓTIMA</span>
                    <span className="font-bold text-zinc-200 text-sm">
                      {station.cultura_perfil.temp_otima_min}°C - {station.cultura_perfil.temp_otima_max}°C
                    </span>
                    <span className="text-[9px] text-zinc-500 block mt-0.5">Calor Crítico: &gt; {station.cultura_perfil.temp_estresse_calor}°C</span>
                  </div>

                  <div className="bg-zinc-950 p-2.5 rounded border border-zinc-800/80">
                    <span className="text-[10px] text-zinc-500 block">ALERTA DE GEADA</span>
                    <span className="font-bold text-sky-400 text-sm">
                      &lt; {station.cultura_perfil.temp_critica_geada}°C
                    </span>
                    <span className="text-[9px] text-zinc-500 block mt-0.5">Risco de Queima Foliar</span>
                  </div>

                  <div className="bg-zinc-950 p-2.5 rounded border border-zinc-800/80">
                    <span className="text-[10px] text-zinc-500 block">SONDA DE SOLO</span>
                    <span className="font-bold text-amber-400 text-sm">
                      {station.cultura_perfil.profundidade_raiz_cm || 40} cm
                    </span>
                    <span className="text-[9px] text-zinc-500 block mt-0.5">Zona Radicular Efetiva</span>
                  </div>

                  <div className="bg-zinc-950 p-2.5 rounded border border-zinc-800/80 col-span-2 sm:col-span-1">
                    <span className="text-[10px] text-zinc-500 block">FITOSSANIDADE</span>
                    <span className="font-bold text-rose-300 text-[11px] truncate block mt-0.5" title={station.cultura_perfil.doencas_monitoradas}>
                      {station.cultura_perfil.doencas_monitoradas || 'Fitopatologia'}
                    </span>
                    <span className="text-[9px] text-zinc-500 block mt-0.5">Alerta Fúngico Automático</span>
                  </div>
                </div>
              </div>
            )}

            {/* Seção 1: Indicadores e Métricas Instantâneas */}
            <div>
              <div className="flex items-center justify-between mb-2 sm:mb-3">
                <h3 className="font-mono text-[11px] sm:text-xs font-bold uppercase tracking-wider text-zinc-400">
                  ESTADO ATUAL DOS SENSORES EM CAMPO
                </h3>
                <span className="font-mono text-[10px] text-zinc-500 hidden sm:inline">
                  {formatarCoordenadas(station.latitude, station.longitude)}
                </span>
              </div>
              <MetricCards station={station} />
            </div>

            {/* Seção 2: Séries Temporais Analíticas dos Subsistemas */}
            <div>
              <div className="flex items-center justify-between mb-2 sm:mb-3">
                <h3 className="font-mono text-[11px] sm:text-xs font-bold uppercase tracking-wider text-zinc-400">
                  SUBSISTEMAS TELEMÉTRICOS ({selectedPeriod.toUpperCase()})
                </h3>
                {loadingHistory && (
                  <span className="font-mono text-[10px] text-emerald-400 animate-pulse flex items-center gap-1">
                    <RefreshCw className="w-3 h-3 animate-spin" /> Carregando registros...
                  </span>
                )}
              </div>
              <SubsystemCharts history={stationHistory} selectedPeriod={selectedPeriod} />
            </div>
          </>
        )}

      </div>

    </div>
  );
}
