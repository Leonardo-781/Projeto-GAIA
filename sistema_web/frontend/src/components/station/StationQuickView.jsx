import React, { useState } from 'react';
import {
  Thermometer,
  Droplets,
  Sun,
  Gauge,
  Activity,
  Layers,
  BarChart3,
  Settings,
  Download,
  X,
  ShieldCheck,
  AlertTriangle,
  Cpu,
  Sprout,
  Compass,
  ChevronDown,
  ChevronUp
} from 'lucide-react';
import {
  formatarDataHora,
  formatarTempoRelativo,
  formatarCoordenadas,
  classificarVPD,
  classificarRiscoFungo,
  classificarRiscoGeada,
  classificarITU
} from '../../utils/agroCalculators';

export default function StationQuickView({
  station,
  onClose,
  onOpenDashboard,
  onOpenSettings,
  onExportCSV,
  user
}) {
  if (!station) return null;

  const [expandedMobile, setExpandedMobile] = useState(true);

  const hasTelemetry = station.timestamp_utc !== null && station.timestamp_utc !== undefined;
  const vpdInfo = hasTelemetry ? classificarVPD(station.vpd) : { status: 'STANDBY', label: 'Aguardando telemetria da sonda', color: 'text-zinc-500', bg: 'bg-zinc-900/50 border-zinc-800' };
  const fungoInfo = hasTelemetry ? classificarRiscoFungo(station.rf) : { nivel: 'STANDBY', color: 'text-zinc-500' };
  const geadaInfo = hasTelemetry ? classificarRiscoGeada(station.rg) : { nivel: 'STANDBY', color: 'text-zinc-500' };

  const isOnline = station.status === 'ONLINE';
  const isSleep = station.status === 'DEEP_SLEEP';

  return (
    <div className="fixed bottom-0 inset-x-0 z-[1000] md:absolute md:bottom-4 md:right-4 md:left-auto md:w-[480px] md:rounded-lg rounded-t-xl border-t md:border border-zinc-800 bg-zinc-950/98 backdrop-blur-md max-h-[85vh] flex flex-col shadow-2xl overflow-hidden select-none animate-in fade-in slide-in-from-bottom-4 duration-200">
      
      {/* Indicador de barra de arrasto / mobile handle */}
      <div 
        onClick={() => setExpandedMobile(!expandedMobile)} 
        className="w-full flex flex-col items-center pt-2 pb-1 cursor-pointer md:hidden hover:bg-zinc-900/50"
      >
        <div className="w-10 h-1 bg-zinc-700 rounded-full" />
      </div>

      {/* Top Header do Dock */}
      <div className="p-3 bg-zinc-900/80 border-b border-zinc-800 flex items-start justify-between gap-2">
        <div className="min-w-0">
          <div className="flex items-center gap-1.5 flex-wrap">
            <span
              className={`w-2.5 h-2.5 rounded-full ${
                isOnline ? 'bg-emerald-400 shadow-sm shadow-emerald-400/50' : isSleep ? 'bg-amber-400' : 'bg-rose-500'
              }`}
            />
            <span className="font-mono font-bold text-sm text-zinc-100 tracking-wider">
              {station.cod_estacao}
            </span>
            <span className="text-[10px] font-mono px-2 py-0.5 rounded bg-zinc-800 text-zinc-300 border border-zinc-700">
              {station.status}
            </span>
            <span className="text-[10px] font-mono px-1.5 py-0.5 rounded bg-zinc-800 text-zinc-400 hidden xs:inline">
              {station.visibilidade}
            </span>
          </div>

          <h3 className="text-xs sm:text-sm font-semibold text-zinc-100 mt-1 truncate">
            {station.nome || 'Estação Agroclimática'}
          </h3>

          <div className="flex items-center gap-2 text-[10px] sm:text-[11px] font-mono text-zinc-400 mt-0.5">
            <span className="flex items-center gap-1 text-emerald-400">
              <Sprout className="w-3 h-3" />
              {station.cultura || 'Cultura Geral'}
              {station.cultura_perfil?.kc_medio && (
                <span className="text-[9px] text-emerald-500/80">
                  (Kc {station.cultura_perfil.kc_medio})
                </span>
              )}
            </span>
            <span>•</span>
            <span className="truncate">{formatarCoordenadas(station.latitude, station.longitude)}</span>
          </div>
        </div>

        <div className="flex items-center gap-1">
          <button
            onClick={() => setExpandedMobile(!expandedMobile)}
            className="p-1 rounded text-zinc-400 hover:text-zinc-100 hover:bg-zinc-800 transition cursor-pointer md:hidden"
            title={expandedMobile ? 'Recolher detalhes' : 'Expandir detalhes'}
          >
            {expandedMobile ? <ChevronDown className="w-4 h-4" /> : <ChevronUp className="w-4 h-4" />}
          </button>
          <button
            onClick={onClose}
            className="p-1 rounded text-zinc-400 hover:text-zinc-100 hover:bg-zinc-800 transition cursor-pointer"
            title="Fechar painel"
          >
            <X className="w-4 h-4" />
          </button>
        </div>
      </div>

      {/* Conteúdo Expansível */}
      <div className={`overflow-y-auto flex-1 ${expandedMobile ? 'block' : 'hidden md:block'}`}>
        
        {/* Grid de Sensores e Telemetria em Tempo Real */}
        <div className="p-3 grid grid-cols-2 sm:grid-cols-4 gap-2 bg-zinc-950 text-xs font-mono">
          
          {/* Microclima: Temperatura */}
          <div className="p-2 rounded bg-zinc-900/60 border border-zinc-800/80">
            <div className="text-[10px] text-zinc-500 flex items-center justify-between">
              <span>TEMP AR</span>
              <Thermometer className="w-3 h-3 text-rose-400" />
            </div>
            <div className="text-base font-bold text-zinc-100 mt-0.5">
              {station.temp_ref || station.temp_dht ? `${station.temp_ref || station.temp_dht}°C` : '--'}
            </div>
            <div className="text-[10px] text-zinc-400 truncate">
              Sens: {station.hi ? `${station.hi}°C` : '--'}
            </div>
          </div>

          {/* Umidade Relativa */}
          <div className="p-2 rounded bg-zinc-900/60 border border-zinc-800/80">
            <div className="text-[10px] text-zinc-500 flex items-center justify-between">
              <span>UMID AR</span>
              <Droplets className="w-3 h-3 text-sky-400" />
            </div>
            <div className="text-base font-bold text-zinc-100 mt-0.5">
              {station.umidade ? `${station.umidade}%` : '--'}
            </div>
            <div className="text-[10px] text-zinc-400 truncate">
              {station.pressao ? `${station.pressao}hPa` : '--'}
            </div>
          </div>

          {/* Solo (Capacitivo) */}
          <div className="p-2 rounded bg-zinc-900/60 border border-zinc-800/80">
            <div className="text-[10px] text-zinc-500 flex items-center justify-between">
              <span>UMID SOLO</span>
              <Layers className="w-3 h-3 text-emerald-400" />
            </div>
            <div className="text-base font-bold text-emerald-400 mt-0.5">
              {station.umidade_solo !== null && station.umidade_solo !== undefined ? `${station.umidade_solo}%` : '--'}
            </div>
            <div className="text-[10px] text-zinc-400 truncate">
              T.Solo: {station.temp_solo ? `${station.temp_solo}°C` : '--'}
            </div>
          </div>

          {/* Radiação / Luz */}
          <div className="p-2 rounded bg-zinc-900/60 border border-zinc-800/80">
            <div className="text-[10px] text-zinc-500 flex items-center justify-between">
              <span>RADIAÇÃO</span>
              <Sun className="w-3 h-3 text-amber-400" />
            </div>
            <div className="text-base font-bold text-amber-400 mt-0.5">
              {station.rs ? `${station.rs} W/m²` : (station.lux ? `${station.lux} lx` : '--')}
            </div>
            <div className="text-[10px] text-zinc-400 truncate">
              PAR: {station.par ? `${station.par}` : '--'}
            </div>
          </div>

        </div>

        {/* Faixa Agronômica: VPD & Riscos */}
        <div className="px-3 pb-3 pt-1 grid grid-cols-1 sm:grid-cols-2 gap-2 text-xs font-mono">
          <div className={`p-2 rounded border ${vpdInfo.bg}`}>
            <div className="text-[10px] text-zinc-400 flex items-center justify-between">
              <span>VPD (DÉFICIT VAPOR)</span>
              <span className={`text-[10px] font-bold ${vpdInfo.color}`}>{station.vpd ? `${station.vpd} kPa` : '--'}</span>
            </div>
            <div className={`text-[10px] font-bold mt-1 truncate ${vpdInfo.color}`}>
              {vpdInfo.label}
            </div>
          </div>

          <div className="p-2 rounded bg-zinc-900/60 border border-zinc-800 flex flex-col justify-between">
            <div className="flex items-center justify-between text-[10px]">
              <span className="text-zinc-500">FITOSSANITÁRIO:</span>
              <span className={fungoInfo.color}>{fungoInfo.nivel}</span>
            </div>
            <div className="flex items-center justify-between text-[10px] mt-1">
              <span className="text-zinc-500">RISCO GEADA:</span>
              <span className={geadaInfo.color}>{geadaInfo.nivel}</span>
            </div>
          </div>
        </div>

      </div>

      {/* Rodapé de Ações de Engenharia (Apenas ferramentas reais) */}
      <div className="p-2.5 bg-zinc-900/90 border-t border-zinc-800 flex items-center justify-between gap-2 shrink-0">
        <div className="flex items-center gap-1.5">
          <button
            onClick={() => onExportCSV(station)}
            title="Exportar registros telemétricos em CSV"
            className="p-2 rounded bg-zinc-800 hover:bg-zinc-700 text-zinc-300 hover:text-white border border-zinc-700 text-xs font-mono transition cursor-pointer"
          >
            <Download className="w-3.5 h-3.5" />
          </button>

          {user && (
            <button
              onClick={() => onOpenSettings(station)}
              title="Configurações técnicas da estação"
              className="p-2 rounded bg-zinc-800 hover:bg-zinc-700 text-zinc-300 hover:text-white border border-zinc-700 text-xs font-mono transition cursor-pointer"
            >
              <Settings className="w-3.5 h-3.5" />
            </button>
          )}
        </div>

        <button
          onClick={() => onOpenDashboard(station)}
          className="flex-1 sm:flex-none flex items-center justify-center gap-2 px-3 py-2 rounded bg-emerald-600 hover:bg-emerald-500 text-white font-mono font-bold text-xs transition shadow-sm cursor-pointer"
        >
          <BarChart3 className="w-4 h-4" />
          <span>PAINEL ANALÍTICO</span>
        </button>
      </div>

    </div>
  );
}
