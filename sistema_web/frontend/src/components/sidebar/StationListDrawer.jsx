import React from 'react';
import {
  Search,
  X,
  Radio,
  Lock,
  Battery,
  Droplets,
  Thermometer,
  ChevronRight,
  ShieldAlert,
  Sprout
} from 'lucide-react';
import { formatarTempoRelativo } from '../../utils/agroCalculators';

export default function StationListDrawer({
  isOpen,
  onClose,
  estacoes,
  selectedStation,
  onSelectStation,
  searchQuery,
  setSearchQuery,
  statusFilter,
  setStatusFilter,
  stats
}) {
  if (!isOpen) return null;

  const handleSelect = (st) => {
    onSelectStation(st);
    // Em telas pequenas (mobile), fecha automaticamente o drawer ao selecionar
    if (typeof window !== 'undefined' && window.innerWidth < 1024) {
      onClose();
    }
  };

  return (
    <>
      {/* Backdrop para mobile */}
      <div
        onClick={onClose}
        className="fixed inset-0 bg-black/70 backdrop-blur-xs z-30 lg:hidden transition-opacity"
      />

      <aside className="fixed lg:relative inset-y-0 left-0 w-[85vw] max-w-xs sm:w-80 h-full bg-zinc-950/98 backdrop-blur-md border-r border-zinc-800 flex flex-col z-40 select-none shadow-2xl transition-all">
        
        {/* Header do Drawer */}
        <div className="p-3 border-b border-zinc-800 flex items-center justify-between">
          <div className="flex items-center gap-2">
            <Radio className="w-4 h-4 text-emerald-400" />
            <span className="font-mono font-bold text-xs uppercase tracking-wider text-zinc-200">
              FROTA DE CAMPO ({estacoes.length})
            </span>
          </div>
          <button
            onClick={onClose}
            className="p-1.5 rounded text-zinc-400 hover:text-zinc-200 hover:bg-zinc-900 transition cursor-pointer"
            title="Fechar gaveta"
          >
            <X className="w-4 h-4" />
          </button>
        </div>

        {/* Busca Técnica por Estação ou Talhão */}
        <div className="p-3 border-b border-zinc-800 bg-zinc-900/40">
          <div className="relative">
            <Search className="w-3.5 h-3.5 text-zinc-500 absolute left-2.5 top-2.5" />
            <input
              type="text"
              placeholder="Filtrar código, talhão, cultura..."
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              className="w-full bg-zinc-900 border border-zinc-700/80 rounded pl-8 pr-7 py-1.5 text-xs font-mono text-zinc-200 placeholder-zinc-500 focus:outline-none focus:border-emerald-500"
            />
            {searchQuery && (
              <button
                onClick={() => setSearchQuery('')}
                className="absolute right-2 top-2 text-zinc-500 hover:text-zinc-300"
              >
                <X className="w-3.5 h-3.5" />
              </button>
            )}
          </div>

          {/* Chips de Status Rápido */}
          <div className="flex items-center gap-1 mt-2 text-[10px] font-mono overflow-x-auto no-scrollbar py-0.5">
            <button
              onClick={() => setStatusFilter('ALL')}
              className={`px-2 py-0.5 rounded cursor-pointer transition shrink-0 ${
                statusFilter === 'ALL'
                  ? 'bg-zinc-800 text-zinc-100 font-bold border border-zinc-600'
                  : 'text-zinc-400 hover:text-zinc-200 bg-zinc-900/60'
              }`}
            >
              TODAS ({stats.total})
            </button>
            <button
              onClick={() => setStatusFilter('ONLINE')}
              className={`px-2 py-0.5 rounded cursor-pointer transition shrink-0 ${
                statusFilter === 'ONLINE'
                  ? 'bg-emerald-950 text-emerald-300 font-bold border border-emerald-700'
                  : 'text-zinc-400 hover:text-emerald-400 bg-zinc-900/60'
              }`}
            >
              ON ({stats.online})
            </button>
            <button
              onClick={() => setStatusFilter('DEEP_SLEEP')}
              className={`px-2 py-0.5 rounded cursor-pointer transition shrink-0 ${
                statusFilter === 'DEEP_SLEEP'
                  ? 'bg-amber-950 text-amber-300 font-bold border border-amber-700'
                  : 'text-zinc-400 hover:text-amber-400 bg-zinc-900/60'
              }`}
            >
              SLEEP ({stats.sleep})
            </button>
            <button
              onClick={() => setStatusFilter('OFFLINE')}
              className={`px-2 py-0.5 rounded cursor-pointer transition shrink-0 ${
                statusFilter === 'OFFLINE'
                  ? 'bg-rose-950 text-rose-300 font-bold border border-rose-700'
                  : 'text-zinc-400 hover:text-rose-400 bg-zinc-900/60'
              }`}
            >
              OFF ({stats.offline})
            </button>
          </div>
        </div>

        {/* Lista de Estações */}
        <div className="flex-1 overflow-y-auto divide-y divide-zinc-900 p-2 space-y-1">
          {estacoes.length === 0 ? (
            <div className="text-center py-10 px-4 text-zinc-500 font-mono text-xs">
              Nenhuma estação localizada com os filtros selecionados.
            </div>
          ) : (
            estacoes.map((st) => {
              const isSelected = selectedStation?.cod_estacao === st.cod_estacao;
              const isOnline = st.status === 'ONLINE';
              const isSleep = st.status === 'DEEP_SLEEP';

              return (
                <div
                  key={st.cod_estacao}
                  onClick={() => handleSelect(st)}
                  className={`p-2.5 rounded border transition cursor-pointer select-none ${
                    isSelected
                      ? 'bg-zinc-900/90 border-emerald-500/80 shadow-md'
                      : 'bg-zinc-900/30 hover:bg-zinc-900/60 border-zinc-800/80'
                  }`}
                >
                  {/* Linha 1: Código, Status & Tempo */}
                  <div className="flex items-center justify-between">
                    <div className="flex items-center gap-1.5">
                      <span
                        className={`w-2 h-2 rounded-full ${
                          isOnline ? 'bg-emerald-400 shadow-sm shadow-emerald-400/50' : isSleep ? 'bg-amber-400' : 'bg-rose-500'
                        }`}
                      />
                      <span className="font-mono font-bold text-xs text-zinc-100">
                        {st.cod_estacao}
                      </span>
                      {st.visibilidade === 'PRIVADA' && (
                        <Lock className="w-3 h-3 text-amber-400/80" title="Estação Privada" />
                      )}
                      {st.status_financeiro === 'PENDENTE' && (
                        <ShieldAlert className="w-3 h-3 text-rose-400" title="Faturamento Pendente" />
                      )}
                    </div>

                    <span className="text-[10px] font-mono text-zinc-400">
                      {formatarTempoRelativo(st.segundos_desde_ultimo_dado)}
                    </span>
                  </div>

                  {/* Linha 2: Nome do Talhão / Fazenda */}
                  <div className="text-xs font-medium text-zinc-200 truncate mt-1">
                    {st.nome || 'Estação Sem Nome'}
                  </div>

                  {/* Linha 3: Cultura */}
                  <div className="flex items-center gap-1 text-[11px] text-zinc-400 mt-0.5">
                    <Sprout className="w-3 h-3 text-emerald-500/80 shrink-0" />
                    <span className="truncate">{st.cultura || 'Cultura não informada'}</span>
                  </div>

                  {/* Linha 4: Micro Telemetria */}
                  <div className="grid grid-cols-3 gap-1 mt-2 pt-1.5 border-t border-zinc-800/70 font-mono text-[10px]">
                    <div className="flex items-center gap-1 text-zinc-300">
                      <Thermometer className="w-3 h-3 text-rose-400 shrink-0" />
                      <span className="truncate">{st.temp_ref || st.temp_dht ? `${st.temp_ref || st.temp_dht}°C` : '--'}</span>
                    </div>
                    <div className="flex items-center gap-1 text-zinc-300">
                      <Droplets className="w-3 h-3 text-sky-400 shrink-0" />
                      <span className="truncate">{st.umidade ? `${st.umidade}%` : '--'}</span>
                    </div>
                    <div className="flex items-center gap-1 text-zinc-300">
                      <span className="text-emerald-400 text-[9px] font-bold shrink-0">SOLO</span>
                      <span className="truncate">{st.umidade_solo !== null && st.umidade_solo !== undefined ? `${st.umidade_solo}%` : '--'}</span>
                    </div>
                  </div>
                </div>
              );
            })
          )}
        </div>

      </aside>
    </>
  );
}
