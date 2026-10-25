import React from 'react';
import {
  Layers,
  Crosshair,
  Satellite,
  Compass,
  MapPin,
  Filter,
  CheckCircle2,
  Moon,
  Mountain
} from 'lucide-react';

export default function MapToolbar({
  currentLayer,
  setCurrentLayer,
  onRecenter,
  statusFilter,
  setStatusFilter,
  cropFilter,
  setCropFilter,
  cultures,
  cursorCoords
}) {
  const layerOptions = [
    { id: 'satellite', label: 'Satélite HD', short: 'Sat', icon: Satellite },
    { id: 'dark', label: 'Carto Dark', short: 'Dark', icon: Moon },
    { id: 'topo', label: 'Topografia', short: 'Topo', icon: Mountain }
  ];

  return (
    <div className="absolute top-2 sm:top-3 left-2 sm:left-3 right-2 sm:right-auto z-[1000] flex flex-col gap-1.5 pointer-events-none">
      
      {/* Barra Principal de Ferramentas GIS */}
      <div className="flex items-center gap-1.5 p-1 sm:p-1.5 rounded-md bg-zinc-950/90 backdrop-blur-md border border-zinc-800 shadow-xl pointer-events-auto overflow-x-auto no-scrollbar max-w-full">
        
        {/* Seletor de Camada Cartográfica */}
        <div className="flex items-center gap-0.5 sm:gap-1 bg-zinc-900/90 p-0.5 rounded border border-zinc-800 shrink-0">
          {layerOptions.map((layer) => {
            const Icon = layer.icon;
            const isActive = currentLayer === layer.id;
            return (
              <button
                key={layer.id}
                onClick={() => setCurrentLayer(layer.id)}
                className={`flex items-center gap-1 px-2 py-1 rounded text-[11px] font-mono font-medium transition cursor-pointer shrink-0 ${
                  isActive
                    ? 'bg-zinc-800 text-emerald-400 border border-zinc-700 shadow-xs'
                    : 'text-zinc-400 hover:text-zinc-200 hover:bg-zinc-800/50'
                }`}
                title={`Alternar para camada ${layer.label}`}
              >
                <Icon className="w-3.5 h-3.5 shrink-0" />
                <span className="hidden sm:inline">{layer.label}</span>
                <span className="sm:hidden text-[10px]">{layer.short}</span>
              </button>
            );
          })}
        </div>

        <div className="h-4 w-[1px] bg-zinc-800 shrink-0"></div>

        {/* Botão de Recentralizar */}
        <button
          onClick={onRecenter}
          className="flex items-center gap-1 px-2 py-1 rounded bg-zinc-900 hover:bg-zinc-800 border border-zinc-800 text-zinc-300 hover:text-white text-[11px] font-mono transition cursor-pointer shrink-0"
          title="Centralizar na Estação Principal / Monte Carmelo"
        >
          <Crosshair className="w-3.5 h-3.5 text-emerald-400 shrink-0" />
          <span className="hidden sm:inline">CENTRALIZAR</span>
        </button>

        {/* Filtro Rápido de Status */}
        <div className="flex items-center gap-1 shrink-0">
          <select
            value={statusFilter}
            onChange={(e) => setStatusFilter(e.target.value)}
            className="bg-zinc-900 border border-zinc-800 text-zinc-300 text-[10px] sm:text-[11px] font-mono rounded px-1.5 py-1 focus:outline-none focus:border-emerald-500 cursor-pointer"
          >
            <option value="ALL">Status: Todos</option>
            <option value="ONLINE">Online</option>
            <option value="DEEP_SLEEP">Sleep</option>
            <option value="OFFLINE">Offline</option>
          </select>

          {cultures && cultures.length > 0 && (
            <select
              value={cropFilter}
              onChange={(e) => setCropFilter(e.target.value)}
              className="bg-zinc-900 border border-zinc-800 text-zinc-300 text-[10px] sm:text-[11px] font-mono rounded px-1.5 py-1 focus:outline-none focus:border-emerald-500 cursor-pointer hidden xs:block"
            >
              <option value="ALL">Culturas: Todas</option>
              {cultures.map(c => (
                <option key={c} value={c}>{c}</option>
              ))}
            </select>
          )}
        </div>

      </div>

      {/* Leitura de Coordenadas do Cursor / Bacia (apenas desktop/telas maiores) */}
      {cursorCoords && (
        <div className="hidden sm:block self-start px-2 py-0.5 rounded bg-zinc-950/85 backdrop-blur-xs border border-zinc-800/90 text-zinc-400 font-mono text-[10px] shadow pointer-events-auto">
          COORD: <span className="text-zinc-200">{cursorCoords.lat.toFixed(4)}° S, {cursorCoords.lng.toFixed(4)}° W</span>
        </div>
      )}

    </div>
  );
}
