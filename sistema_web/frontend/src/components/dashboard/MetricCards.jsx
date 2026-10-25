import React from 'react';
import {
  Thermometer,
  Droplets,
  Layers,
  Sun,
  Activity,
  Gauge,
  Cpu,
  CloudLightning,
  AlertTriangle,
  Flame,
  BatteryCharging,
  Zap,
  Sprout
} from 'lucide-react';
import {
  classificarVPD,
  classificarRiscoFungo,
  classificarRiscoGeada,
  classificarITU
} from '../../utils/agroCalculators';

export default function MetricCards({ station }) {
  if (!station) return null;

  const cp = station.cultura_perfil || null;
  const hasTelemetry = station.timestamp_utc !== null && station.timestamp_utc !== undefined;
  const tempAtual = station.temp_ref !== null && station.temp_ref !== undefined ? station.temp_ref : station.temp_dht;
  const hasTemp = tempAtual !== null && tempAtual !== undefined;
  const isHeatStress = cp && hasTemp && tempAtual > cp.temp_estresse_calor;
  const isFrostAlert = cp && hasTemp && tempAtual < cp.temp_critica_geada;

  const vpdInfo = hasTelemetry 
    ? classificarVPD(station.vpd) 
    : { status: 'STANDBY', label: 'Aguardando primeiro pacote telemétrico', color: 'text-zinc-500', bg: 'bg-zinc-900/60 border-zinc-800' };

  return (
    <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-3">
      
      {/* 1. ATMOSFERA: Temperatura e Sensação */}
      <div className="bg-zinc-900/80 border border-zinc-800 rounded-lg p-3.5 flex flex-col justify-between">
        <div className="flex items-center justify-between text-zinc-400 text-xs font-mono">
          <span className="uppercase tracking-wider">MICROCLIMA AR</span>
          <Thermometer className="w-4 h-4 text-rose-400" />
        </div>
        <div className="my-2">
          <div className="text-2xl font-black font-mono text-zinc-100 flex items-center justify-between">
            <span>{tempAtual ? `${tempAtual}°C` : '--'}</span>
            {isHeatStress && (
              <span className="text-[9px] px-1.5 py-0.5 rounded bg-rose-950 text-rose-300 border border-rose-800 font-bold">
                ESTRESSE &gt; {cp.temp_estresse_calor}°C
              </span>
            )}
            {isFrostAlert && (
              <span className="text-[9px] px-1.5 py-0.5 rounded bg-sky-950 text-sky-300 border border-sky-800 font-bold">
                GEADA &lt; {cp.temp_critica_geada}°C
              </span>
            )}
          </div>
          <div className="text-xs text-zinc-400 font-mono mt-0.5">
            Sensação Térmica: <span className="text-zinc-200">{station.hi ? `${station.hi}°C` : '--'}</span>
          </div>
        </div>
        <div className="text-[10px] font-mono text-zinc-500 border-t border-zinc-800/80 pt-1.5 flex justify-between">
          <span>{cp ? `Faixa Ótima: ${cp.temp_otima_min}-${cp.temp_otima_max}°C` : 'Sensor DHT/SHT'}</span>
          <span className={isHeatStress ? 'text-rose-400 font-bold' : isFrostAlert ? 'text-sky-400 font-bold' : hasTelemetry ? 'text-emerald-400' : 'text-zinc-500'}>
            {isHeatStress ? 'Calor Excessivo' : isFrostAlert ? 'Alerta Geada' : hasTelemetry ? 'Conforto' : 'Aguardando'}
          </span>
        </div>
      </div>

      {/* 2. ATMOSFERA: Umidade e Ponto de Orvalho */}
      <div className="bg-zinc-900/80 border border-zinc-800 rounded-lg p-3.5 flex flex-col justify-between">
        <div className="flex items-center justify-between text-zinc-400 text-xs font-mono">
          <span className="uppercase tracking-wider">UMIDADE RELATIVA</span>
          <Droplets className="w-4 h-4 text-sky-400" />
        </div>
        <div className="my-2">
          <div className="text-2xl font-black font-mono text-sky-400">
            {station.umidade ? `${station.umidade}%` : '--'}
          </div>
          <div className="text-xs text-zinc-400 font-mono mt-0.5">
            Ponto de Orvalho: <span className="text-zinc-200">{station.dew_point ? `${station.dew_point}°C` : '--'}</span>
          </div>
        </div>
        <div className="text-[10px] font-mono text-zinc-500 border-t border-zinc-800/80 pt-1.5 flex justify-between">
          <span>Saturação de Vapor</span>
          <span className={station.umidade > 85 ? 'text-amber-400' : 'text-zinc-500'}>
            {station.umidade ? (station.umidade > 85 ? 'Elevada' : 'Normal') : 'Aguardando'}
          </span>
        </div>
      </div>

      {/* 3. SOLO: Umidade e Temperatura Radicular */}
      <div className="bg-zinc-900/80 border border-zinc-800 rounded-lg p-3.5 flex flex-col justify-between">
        <div className="flex items-center justify-between text-zinc-400 text-xs font-mono">
          <span className="uppercase tracking-wider">PERFIL DE SOLO</span>
          <Layers className="w-4 h-4 text-emerald-400" />
        </div>
        <div className="my-2">
          <div className="text-2xl font-black font-mono text-emerald-400">
            {station.umidade_solo !== null && station.umidade_solo !== undefined ? `${station.umidade_solo}%` : '--'}
          </div>
          <div className="text-xs text-zinc-400 font-mono mt-0.5">
            Temp Radicular: <span className="text-zinc-200">{station.temp_solo ? `${station.temp_solo}°C` : '--'}</span>
          </div>
        </div>
        <div className="text-[10px] font-mono text-zinc-500 border-t border-zinc-800/80 pt-1.5 flex justify-between">
          <span>{cp ? `Sonda ${cp.profundidade_raiz_cm || 40}cm` : 'Sonda Capacitiva 20cm'}</span>
          <span className={hasTelemetry ? 'text-emerald-400' : 'text-zinc-500'}>
            {hasTelemetry ? 'Calibrado' : 'Aguardando'}
          </span>
        </div>
      </div>

      {/* 4. FISIOLOGIA VEGETAL: VPD (Déficit de Pressão de Vapor) */}
      <div className={`border rounded-lg p-3.5 flex flex-col justify-between ${vpdInfo.bg}`}>
        <div className="flex items-center justify-between text-zinc-300 text-xs font-mono">
          <span className="uppercase tracking-wider">VPD ESTOMÁTICO</span>
          <Activity className={`w-4 h-4 ${vpdInfo.color}`} />
        </div>
        <div className="my-2">
          <div className={`text-2xl font-black font-mono ${vpdInfo.color}`}>
            {station.vpd ? `${station.vpd} kPa` : '--'}
          </div>
          <div className="text-xs font-medium text-zinc-200 mt-0.5 truncate">
            {vpdInfo.label}
          </div>
        </div>
        <div className="text-[10px] font-mono text-zinc-400 border-t border-zinc-800/80 pt-1.5 flex justify-between">
          <span>Faixa Ótima: {cp ? `${cp.vpd_otimo_min} - ${cp.vpd_otimo_max} kPa` : '0.8 - 1.2 kPa'}</span>
          <span className={`font-bold ${vpdInfo.color}`}>{vpdInfo.status}</span>
        </div>
      </div>

      {/* 5. RADIAÇÃO GLOBAL E PAR */}
      <div className="bg-zinc-900/80 border border-zinc-800 rounded-lg p-3.5 flex flex-col justify-between">
        <div className="flex items-center justify-between text-zinc-400 text-xs font-mono">
          <span className="uppercase tracking-wider">RADIAÇÃO SOLAR (Rs)</span>
          <Sun className="w-4 h-4 text-amber-400" />
        </div>
        <div className="my-2">
          <div className="text-2xl font-black font-mono text-amber-400">
            {station.rs ? `${station.rs} W/m²` : '--'}
          </div>
          <div className="text-xs text-zinc-400 font-mono mt-0.5">
            PAR Fotossintético: <span className="text-zinc-200">{station.par ? `${station.par} μmol/m²/s` : '--'}</span>
          </div>
        </div>
        <div className="text-[10px] font-mono text-zinc-500 border-t border-zinc-800/80 pt-1.5 flex justify-between">
          <span>Iluminância: {station.lux ? `${station.lux} lx` : '--'}</span>
          <span className="text-zinc-500">{hasTelemetry ? 'Fotoperíodo' : 'Aguardando'}</span>
        </div>
      </div>

      {/* 6. EVAPOTRANSPIRAÇÃO (ET0 / ETc) */}
      <div className="bg-zinc-900/80 border border-zinc-800 rounded-lg p-3.5 flex flex-col justify-between">
        <div className="flex items-center justify-between text-zinc-400 text-xs font-mono">
          <span className="uppercase tracking-wider">EVAPOTRANSPIRAÇÃO</span>
          <Sprout className="w-4 h-4 text-emerald-400" />
        </div>
        <div className="my-2">
          <div className="text-2xl font-black font-mono text-zinc-100">
            {station.et0 ? `${station.et0} mm/dia` : '--'}
          </div>
          <div className="text-xs text-zinc-400 font-mono mt-0.5">
            ETc ({station.cultura || 'Cultura'}{cp ? ` Kc ${cp.kc_medio}` : ''}): <span className="text-emerald-400 font-bold">{station.etc ? `${station.etc} mm` : '--'}</span>
          </div>
        </div>
        <div className="text-[10px] font-mono text-zinc-500 border-t border-zinc-800/80 pt-1.5 flex justify-between">
          <span>Penman-Monteith FAO</span>
          <span className="text-zinc-400">{cp ? `Kc ${cp.kc_medio}` : (hasTelemetry ? 'Demanda Hídrica' : 'Aguardando')}</span>
        </div>
      </div>

      {/* 7. BAROMETRIA & ESTABILIDADE ATMOSFÉRICA */}
      <div className="bg-zinc-900/80 border border-zinc-800 rounded-lg p-3.5 flex flex-col justify-between">
        <div className="flex items-center justify-between text-zinc-400 text-xs font-mono">
          <span className="uppercase tracking-wider">PRESSÃO BAROMÉTRICA</span>
          <Gauge className="w-4 h-4 text-purple-400" />
        </div>
        <div className="my-2">
          <div className="text-2xl font-black font-mono text-zinc-100">
            {station.pressao ? `${station.pressao} hPa` : '--'}
          </div>
          <div className="text-xs text-zinc-400 font-mono mt-0.5 flex items-center gap-1.5">
            Tendência (ΔP 3h): <span className="text-zinc-200">{station.dp ? `${station.dp} hPa` : '--'}</span>
            {station.at && (
              <span className="px-1.5 py-0.2 rounded bg-rose-950 text-rose-300 border border-rose-800 text-[10px] font-bold">
                TEMPESTADE
              </span>
            )}
          </div>
        </div>
        <div className="text-[10px] font-mono text-zinc-500 border-t border-zinc-800/80 pt-1.5 flex justify-between">
          <span>{cp ? `Geada: < ${cp.temp_critica_geada}°C` : 'BMP280 Altitude Corrigida'}</span>
          <span className={station.at ? 'text-rose-400 font-bold' : isFrostAlert ? 'text-sky-400 font-bold' : (hasTelemetry ? 'Estável' : 'Aguardando')}>
            {station.at ? 'Queda Severa' : isFrostAlert ? 'Risco Geada' : (hasTelemetry ? 'Estável' : 'Aguardando')}
          </span>
        </div>
      </div>

      {/* 8. TELEMETRIA DO HARDWARE (ESP32-C3) */}
      <div className="bg-zinc-900/80 border border-zinc-800 rounded-lg p-3.5 flex flex-col justify-between">
        <div className="flex items-center justify-between text-zinc-400 text-xs font-mono">
          <span className="uppercase tracking-wider">SISTEMA & REDE</span>
          <Cpu className="w-4 h-4 text-teal-400" />
        </div>
        <div className="my-2">
          <div className="text-2xl font-black font-mono text-teal-400">
            {station.qualidade ? `${station.qualidade}%` : '--'}
          </div>
          <div className="text-xs text-zinc-400 font-mono mt-0.5">
            Heap RAM Livre: <span className="text-zinc-200">{station.heap_mod ? `${station.heap_mod} KB` : '--'}</span>
          </div>
        </div>
        <div className="text-[10px] font-mono text-zinc-500 border-t border-zinc-800/80 pt-1.5 flex justify-between">
          <span>AES-128 Hardware</span>
          <span className={hasTelemetry ? 'text-emerald-400' : 'text-zinc-500'}>
            {hasTelemetry ? 'Ativo' : 'Standby'}
          </span>
        </div>
      </div>

    </div>
  );
}
