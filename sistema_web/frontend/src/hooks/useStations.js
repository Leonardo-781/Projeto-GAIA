import { useState, useCallback, useMemo, useEffect } from 'react';

export function useStations(getAuthHeaders) {
  const [estacoes, setEstacoes] = useState([]);
  const [selectedStation, setSelectedStation] = useState(null);
  const [cropProfiles, setCropProfiles] = useState([]);
  const [statusFilter, setStatusFilter] = useState('ALL');
  const [visibilityFilter, setVisibilityFilter] = useState('ALL');
  const [cropFilter, setCropFilter] = useState('ALL');
  const [searchQuery, setSearchQuery] = useState('');
  const [loading, setLoading] = useState(false);

  const carregarEstacoes = useCallback(async () => {
    setLoading(true);
    try {
      const res = await fetch('/api/estacoes', { headers: getAuthHeaders ? getAuthHeaders() : {} });
      if (res.ok) {
        const data = await res.json();
        setEstacoes(data);
        return data;
      }
      return [];
    } catch (err) {
      console.error('Erro ao carregar estações:', err);
      return [];
    } finally {
      setLoading(false);
    }
  }, [getAuthHeaders]);

  const carregarCulturas = useCallback(async () => {
    try {
      const res = await fetch('/api/culturas');
      if (res.ok) {
        const data = await res.json();
        setCropProfiles(Array.isArray(data) ? data : []);
        return data;
      }
      return [];
    } catch (err) {
      console.error('Erro ao carregar catálogo de culturas:', err);
      return [];
    }
  }, []);

  useEffect(() => {
    carregarCulturas();
  }, [carregarCulturas]);

  const cultures = useMemo(() => {
    const set = new Set();
    estacoes.forEach(st => {
      if (st.cultura) set.add(st.cultura);
    });
    return Array.from(set);
  }, [estacoes]);

  const filteredEstacoes = useMemo(() => {
    return estacoes.filter(st => {
      if (statusFilter !== 'ALL' && st.status !== statusFilter) return false;
      if (visibilityFilter === 'PUBLICA' && st.visibilidade !== 'PUBLICA') return false;
      if (visibilityFilter === 'PRIVADA' && st.visibilidade !== 'PRIVADA') return false;
      if (visibilityFilter === 'PENDENTE' && st.status_financeiro !== 'PENDENTE') return false;
      if (cropFilter !== 'ALL' && st.cultura !== cropFilter) return false;
      if (searchQuery.trim()) {
        const q = searchQuery.toLowerCase();
        const nome = (st.nome || '').toLowerCase();
        const cod = (st.cod_estacao || '').toLowerCase();
        const cultura = (st.cultura || '').toLowerCase();
        if (!nome.includes(q) && !cod.includes(q) && !cultura.includes(q)) return false;
      }
      return true;
    });
  }, [estacoes, statusFilter, visibilityFilter, cropFilter, searchQuery]);

  const stats = useMemo(() => {
    return {
      total: estacoes.length,
      online: estacoes.filter(e => e.status === 'ONLINE').length,
      sleep: estacoes.filter(e => e.status === 'DEEP_SLEEP').length,
      offline: estacoes.filter(e => e.status === 'OFFLINE').length,
      publicas: estacoes.filter(e => e.visibilidade === 'PUBLICA').length,
      privadas: estacoes.filter(e => e.visibilidade === 'PRIVADA').length,
      pendentes: estacoes.filter(e => e.status_financeiro === 'PENDENTE').length
    };
  }, [estacoes]);

  return {
    estacoes,
    setEstacoes,
    selectedStation,
    setSelectedStation,
    cropProfiles,
    statusFilter,
    setStatusFilter,
    visibilityFilter,
    setVisibilityFilter,
    cropFilter,
    setCropFilter,
    searchQuery,
    setSearchQuery,
    cultures,
    filteredEstacoes,
    stats,
    loading,
    carregarEstacoes,
    carregarCulturas
  };
}
