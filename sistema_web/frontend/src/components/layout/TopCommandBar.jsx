import React, { useState, useEffect } from 'react';
import {
  Activity,
  Layers,
  User,
  LogOut,
  MapPin,
  ChevronDown,
  BarChart3,
  Map as MapIcon,
  Shield,
  Radio,
  SlidersHorizontal,
  Clock
} from 'lucide-react';

export default function TopCommandBar({
  currentView,
  setCurrentView,
  wsConnected,
  latencyMs,
  stats,
  estacoes,
  selectedStation,
  onSelectStation,
  sidebarOpen,
  setSidebarOpen,
  user,
  onOpenAuth,
  onLogout
}) {
  const [currentTime, setCurrentTime] = useState('');

  useEffect(() => {
    const updateTime = () => {
      const now = new Date();
      setCurrentTime(now.toLocaleTimeString('pt-BR', { hour: '2-digit', minute: '2-digit', second: '2-digit' }));
    };
    updateTime();
    const interval = setInterval(updateTime, 1000);
    return () => clearInterval(interval);
  }, []);

  return (
    <header className="h-13 sm:h-14 bg-zinc-950 border-b border-zinc-800 px-2 sm:px-4 flex items-center justify-between z-30 select-none shrink-0 gap-2">
      
      {/* LADO ESQUERDO: Toggle Sidebar + Logo Técnico + Seletor de Estação */}
      <div className="flex items-center gap-1.5 sm:gap-3 min-w-0">
        <button
          onClick={() => setSidebarOpen(!sidebarOpen)}
          title={sidebarOpen ? 'Recolher Lista de Estações' : 'Expandir Lista de Estações'}
          className={`p-1.5 sm:p-2 rounded border text-xs font-mono transition flex items-center gap-1.5 cursor-pointer shrink-0 ${
            sidebarOpen
              ? 'bg-emerald-950/80 border-emerald-600/70 text-emerald-300'
              : 'bg-zinc-900 border-zinc-700/80 text-zinc-300 hover:text-white hover:bg-zinc-800'
          }`}
        >
          <SlidersHorizontal className="w-4 h-4" />
          <span className="hidden md:inline font-sans text-xs font-semibold">FROTA</span>
          <span className="md:hidden text-[10px] font-bold px-1 rounded bg-zinc-800 text-emerald-400">
            {stats.online}
          </span>
        </button>

        {/* Brand Icon + Name */}
        <div
          onClick={() => setCurrentView('map')}
          className="flex items-center gap-1.5 sm:gap-2 cursor-pointer group py-1 shrink-0"
        >
          <div className="w-7 h-7 sm:w-8 sm:h-8 rounded bg-zinc-900 border border-zinc-700 flex items-center justify-center text-emerald-400 group-hover:border-emerald-500 transition shadow-inner shrink-0">
            <Radio className="w-3.5 h-3.5 sm:w-4 sm:h-4 text-emerald-400" />
          </div>
          <div className="leading-none min-w-0">
            <div className="flex items-center gap-1">
              <span className="font-mono font-black text-sm tracking-wider text-zinc-100">GAIA</span>
              <span className="text-[9px] sm:text-[10px] font-mono font-bold px-1 py-0.2 sm:px-1.5 sm:py-0.5 rounded bg-zinc-800 text-emerald-400 border border-zinc-700">
                v2.4
              </span>
            </div>
            <div className="text-[9px] uppercase tracking-wider text-zinc-400 font-mono hidden sm:block mt-0.5">
              CENTRO DE COMANDO
            </div>
          </div>
        </div>

        <div className="hidden xl:block h-5 w-[1px] bg-zinc-800"></div>

        {/* Rápido Seletor de Estação no Header (Oculto em telas menores) */}
        <div className="hidden lg:flex items-center gap-1.5 text-xs text-zinc-300">
          <span className="text-zinc-500 font-mono text-[11px]">ESTAÇÃO:</span>
          <select
            value={selectedStation?.cod_estacao || ''}
            onChange={(e) => {
              const st = estacoes.find(s => s.cod_estacao === e.target.value);
              if (st) onSelectStation(st);
            }}
            className="bg-zinc-900 border border-zinc-700 text-zinc-200 text-xs rounded px-2.5 py-1 font-mono focus:outline-none focus:border-emerald-500 cursor-pointer max-w-[200px] truncate"
          >
            <option value="">Visão Geral da Bacia (Todas)</option>
            {estacoes.map(st => (
              <option key={st.cod_estacao} value={st.cod_estacao}>
                [{st.cod_estacao}] {st.nome || 'Estação'}
              </option>
            ))}
          </select>
        </div>
      </div>

      {/* CENTRO: Telemetria da Rede & Status Técnico (Desktop) */}
      <div className="hidden md:flex items-center gap-3 text-xs font-mono">
        <div className="flex items-center gap-1.5 px-2.5 py-1 rounded bg-zinc-900 border border-zinc-800 text-zinc-300">
          <span className={`w-2 h-2 rounded-full ${wsConnected ? 'bg-emerald-400 animate-pulse' : 'bg-rose-500'}`}></span>
          <span className="text-[11px] font-semibold">
            {wsConnected ? 'LINK ATIVO' : 'OFFLINE'}
          </span>
          {latencyMs !== null && (
            <span className="text-zinc-500 text-[10px]">({latencyMs}ms)</span>
          )}
        </div>

        {/* Contadores da Frota */}
        <div className="flex items-center gap-1.5 text-[11px] px-2.5 py-1 rounded bg-zinc-900 border border-zinc-800 text-zinc-400">
          <span className="text-zinc-200 font-bold">{stats.total}</span>
          <span>ESTAÇÕES</span>
          <span className="text-zinc-600">|</span>
          <span className="text-emerald-400 font-bold">{stats.online} ON</span>
          <span className="text-zinc-600">|</span>
          <span className="text-rose-400 font-bold">{stats.offline} OFF</span>
        </div>

        {/* Relógio de Precisão */}
        <div className="hidden 2xl:flex items-center gap-1.5 px-2 py-1 rounded bg-zinc-900/60 border border-zinc-800/80 text-zinc-400 text-[11px]">
          <Clock className="w-3 h-3 text-zinc-500" />
          <span>{currentTime} BRT</span>
        </div>
      </div>

      {/* LADO DIREITO: Alternância de Modo (Mapa / Dashboard) + Perfil */}
      <div className="flex items-center gap-1.5 sm:gap-2 shrink-0">
        
        {/* Toggle Visão: Mapa vs Dashboard Completo */}
        <div className="flex items-center bg-zinc-900 border border-zinc-700/80 rounded p-0.5">
          <button
            onClick={() => setCurrentView('map')}
            className={`flex items-center gap-1 px-2 sm:px-2.5 py-1 rounded text-xs font-semibold font-mono transition cursor-pointer ${
              currentView === 'map'
                ? 'bg-emerald-600 text-white shadow-sm'
                : 'text-zinc-400 hover:text-zinc-200'
            }`}
          >
            <MapIcon className="w-3.5 h-3.5" />
            <span className="hidden xs:inline sm:inline">MAPA</span>
          </button>
          
          <button
            onClick={() => {
              if (selectedStation) {
                setCurrentView('dashboard');
              } else if (estacoes.length > 0) {
                onSelectStation(estacoes[0]);
                setCurrentView('dashboard');
              }
            }}
            className={`flex items-center gap-1 px-2 sm:px-2.5 py-1 rounded text-xs font-semibold font-mono transition cursor-pointer ${
              currentView === 'dashboard'
                ? 'bg-emerald-600 text-white shadow-sm'
                : 'text-zinc-400 hover:text-zinc-200'
            }`}
          >
            <BarChart3 className="w-3.5 h-3.5" />
            <span className="hidden xs:inline sm:inline">DADOS</span>
          </button>
        </div>

        {/* Perfil / Login */}
        {user ? (
          <div className="flex items-center gap-1 pl-1 border-l border-zinc-800">
            <div className="flex items-center gap-1.5 px-2 py-1 rounded bg-zinc-900 border border-zinc-800">
              <div className="w-5 h-5 sm:w-6 sm:h-6 rounded bg-emerald-900/80 text-emerald-300 font-mono font-bold flex items-center justify-center text-[10px] sm:text-xs border border-emerald-700">
                {user.nome ? user.nome.charAt(0).toUpperCase() : 'U'}
              </div>
              <div className="hidden sm:block text-left text-xs leading-none">
                <div className="text-zinc-200 font-mono text-[11px] font-bold truncate max-w-[90px]">
                  {user.nome.split(' ')[0]}
                </div>
              </div>
            </div>

            <button
              onClick={onLogout}
              title="Desconectar sessão"
              className="p-1.5 rounded bg-zinc-900 border border-zinc-800 text-zinc-400 hover:text-rose-400 hover:border-rose-800 transition cursor-pointer"
            >
              <LogOut className="w-3.5 h-3.5" />
            </button>
          </div>
        ) : (
          <div className="flex items-center gap-1 pl-1 border-l border-zinc-800">
            <button
              onClick={onOpenAuth}
              className="px-2 sm:px-2.5 py-1 rounded bg-zinc-900 hover:bg-zinc-800 border border-zinc-700 text-zinc-200 text-xs font-mono font-bold transition cursor-pointer"
            >
              ACESSO
            </button>
          </div>
        )}

      </div>

    </header>
  );
}
