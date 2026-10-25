import React, { useState, useEffect, useCallback } from 'react';
import TopCommandBar from './components/layout/TopCommandBar';
import StationListDrawer from './components/sidebar/StationListDrawer';
import GaiaMap from './components/map/GaiaMap';
import StationQuickView from './components/station/StationQuickView';
import StationDashboard from './components/dashboard/StationDashboard';
import AuthModal from './components/modals/AuthModal';
import StationSettingsModal from './components/modals/StationSettingsModal';

import { useAuth } from './hooks/useAuth';
import { useStations } from './hooks/useStations';
import { useLiveSocket } from './hooks/useLiveSocket';
import { useTelemetryHistory } from './hooks/useTelemetryHistory';

export default function App() {
  const [currentView, setCurrentView] = useState('map'); // 'map' | 'dashboard'
  
  // No mobile (< 1024px), a lista de frota inicia recolhida para exibir o mapa em tela cheia
  const [sidebarOpen, setSidebarOpen] = useState(() => {
    if (typeof window !== 'undefined') {
      return window.innerWidth >= 1024;
    }
    return false;
  });

  const [settingsModalOpen, setSettingsModalOpen] = useState(false);
  const [stationToConfigure, setStationToConfigure] = useState(null);

  // 1. Hook de Autenticação
  const {
    user,
    authModalOpen,
    setAuthModalOpen,
    authMode,
    setAuthMode,
    authForm,
    setAuthForm,
    authError,
    authLoading,
    getAuthHeaders,
    handleLoginRegister,
    handleLogout
  } = useAuth();

  // 2. Hook de Estações
  const {
    estacoes,
    setEstacoes,
    selectedStation,
    setSelectedStation,
    statusFilter,
    setStatusFilter,
    cropFilter,
    setCropFilter,
    searchQuery,
    setSearchQuery,
    cultures,
    filteredEstacoes,
    stats,
    cropProfiles,
    carregarEstacoes
  } = useStations(getAuthHeaders);

  // 3. Hook de Histórico Telemétrico
  const {
    stationHistory,
    setStationHistory,
    selectedPeriod,
    loadingHistory,
    carregarHistorico,
    mudarPeriodo,
    exportarCSV
  } = useTelemetryHistory(getAuthHeaders);

  // 4. Recebimento de Telemetria em Tempo Real via WebSocket
  const handleNovaLeitura = useCallback((leitura) => {
    if (!leitura || !leitura.cod_estacao) return;

    // Atualiza estado da frota
    setEstacoes((prev) =>
      prev.map((st) => {
        if (st.cod_estacao === leitura.cod_estacao) {
          return {
            ...st,
            ...leitura,
            segundos_desde_ultimo_dado: 0,
            status: 'ONLINE'
          };
        }
        return st;
      })
    );

    // Se for a estação selecionada atualmente
    setSelectedStation((prev) => {
      if (prev && prev.cod_estacao === leitura.cod_estacao) {
        return {
          ...prev,
          ...leitura,
          segundos_desde_ultimo_dado: 0,
          status: 'ONLINE'
        };
      }
      return prev;
    });

    // Se estivermos visualizando o histórico desta estação, anexa ao gráfico
    setStationHistory((prev) => {
      if (selectedStation && selectedStation.cod_estacao === leitura.cod_estacao) {
        const jaExiste = prev.some((h) => h.id === leitura.id);
        if (jaExiste) return prev;
        return [...prev, leitura];
      }
      return prev;
    });
  }, [selectedStation, setEstacoes, setSelectedStation, setStationHistory]);

  const { wsConnected, latencyMs } = useLiveSocket(handleNovaLeitura);

  // Carga inicial de estações
  useEffect(() => {
    carregarEstacoes().then((data) => {
      if (data && data.length > 0 && !selectedStation) {
        setSelectedStation(data[0]);
        carregarHistorico(data[0].cod_estacao, selectedPeriod);
      }
    });
  }, [carregarEstacoes, carregarHistorico, selectedPeriod]);

  // Carrega histórico automaticamente ao mudar para o Dashboard ou alterar o período
  useEffect(() => {
    if (currentView === 'dashboard' && selectedStation?.cod_estacao) {
      carregarHistorico(selectedStation.cod_estacao, selectedPeriod);
    }
  }, [currentView, selectedStation?.cod_estacao, selectedPeriod, carregarHistorico]);

  // Ao selecionar uma estação
  const handleSelectStation = useCallback((st) => {
    setSelectedStation(st);
    if (st && st.cod_estacao) {
      carregarHistorico(st.cod_estacao, selectedPeriod);
    }
  }, [carregarHistorico, selectedPeriod, setSelectedStation]);

  // Transição para Dashboard Analítico
  const handleOpenDashboard = (st) => {
    if (st) {
      setSelectedStation(st);
      carregarHistorico(st.cod_estacao, selectedPeriod);
    }
    setCurrentView('dashboard');
  };

  // Abrir Modal de Configurações
  const handleOpenSettings = (st) => {
    setStationToConfigure(st || selectedStation);
    setSettingsModalOpen(true);
  };

  // Salvar Parâmetros da Estação
  const handleSaveSettings = async (cod, form) => {
    const res = await fetch(`/api/estacoes/${cod}`, {
      method: 'PUT',
      headers: {
        'Content-Type': 'application/json',
        ...getAuthHeaders()
      },
      body: JSON.stringify(form)
    });
    if (!res.ok) {
      const data = await res.json();
      throw new Error(data.error || 'Falha ao salvar configurações.');
    }
    const updated = await res.json();
    await carregarEstacoes();
    setSelectedStation(updated);
  };

  return (
    <div className="flex flex-col h-screen w-screen overflow-hidden bg-zinc-950 font-sans text-zinc-100">
      
      {/* 1. Barra de Comando Superior */}
      <TopCommandBar
        currentView={currentView}
        setCurrentView={setCurrentView}
        wsConnected={wsConnected}
        latencyMs={latencyMs}
        stats={stats}
        estacoes={estacoes}
        selectedStation={selectedStation}
        onSelectStation={handleSelectStation}
        sidebarOpen={sidebarOpen}
        setSidebarOpen={setSidebarOpen}
        user={user}
        onOpenAuth={() => setAuthModalOpen(true)}
        onLogout={handleLogout}
      />

      {/* 2. Área Central de Operações */}
      <main className="flex-1 flex overflow-hidden relative">
        
        {/* Gaveta Retrátil de Frota (Overlay no Mobile) */}
        <StationListDrawer
          isOpen={sidebarOpen}
          onClose={() => setSidebarOpen(false)}
          estacoes={filteredEstacoes}
          selectedStation={selectedStation}
          onSelectStation={handleSelectStation}
          searchQuery={searchQuery}
          setSearchQuery={setSearchQuery}
          statusFilter={statusFilter}
          setStatusFilter={setStatusFilter}
          stats={stats}
        />

        {/* Visão 1: Mapa GIS Profissional */}
        {currentView === 'map' && (
          <div className="flex-1 relative h-full w-full overflow-hidden">
            <GaiaMap
              estacoes={filteredEstacoes}
              selectedStation={selectedStation}
              onSelectStation={handleSelectStation}
              statusFilter={statusFilter}
              setStatusFilter={setStatusFilter}
              cropFilter={cropFilter}
              setCropFilter={setCropFilter}
              cultures={cultures}
            />

            {/* Dock de Telemetria Rápida da Estação Selecionada (Bottom-Sheet no Mobile) */}
            <StationQuickView
              station={selectedStation}
              onClose={() => setSelectedStation(null)}
              onOpenDashboard={handleOpenDashboard}
              onOpenSettings={handleOpenSettings}
              onExportCSV={exportarCSV}
              user={user}
            />
          </div>
        )}

        {/* Visão 2: Dashboard Analítico de Subsistemas */}
        {currentView === 'dashboard' && (
          <StationDashboard
            station={selectedStation}
            stationHistory={stationHistory}
            selectedPeriod={selectedPeriod}
            onPeriodChange={mudarPeriodo}
            loadingHistory={loadingHistory}
            onBackToMap={() => setCurrentView('map')}
            onExportCSV={exportarCSV}
            onOpenSettings={handleOpenSettings}
            user={user}
            onOpenAuth={() => setAuthModalOpen(true)}
          />
        )}

      </main>

      {/* 3. Modais Técnicos */}
      <AuthModal
        isOpen={authModalOpen}
        onClose={() => setAuthModalOpen(false)}
        authMode={authMode}
        setAuthMode={setAuthMode}
        authForm={authForm}
        setAuthForm={setAuthForm}
        authError={authError}
        authLoading={authLoading}
        onSubmit={handleLoginRegister}
      />

      <StationSettingsModal
        isOpen={settingsModalOpen}
        onClose={() => setSettingsModalOpen(false)}
        station={stationToConfigure}
        onSave={handleSaveSettings}
        user={user}
        cropProfiles={cropProfiles}
      />

    </div>
  );
}
