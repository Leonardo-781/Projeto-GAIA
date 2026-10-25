import React, { useState, useEffect, useMemo } from 'react';
import {
  X,
  Settings,
  Save,
  Key,
  Shield,
  MapPin,
  Sprout,
  AlertCircle,
  Thermometer,
  Droplets,
  Activity,
  Layers,
  Bug
} from 'lucide-react';

export default function StationSettingsModal({
  isOpen,
  onClose,
  station,
  onSave,
  user,
  cropProfiles = []
}) {
  const [form, setForm] = useState({
    nome: '',
    cultura: '',
    cultura_id: '',
    latitude: '',
    longitude: '',
    visibilidade: 'PUBLICA',
    status_financeiro: 'PAGO',
    aes_key: '',
    api_token: ''
  });
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');

  // Agrupamento de perfis agronômicos por categoria
  const groupedCrops = useMemo(() => {
    const groups = {};
    cropProfiles.forEach((cp) => {
      const cat = cp.categoria || 'Geral';
      if (!groups[cat]) groups[cat] = [];
      groups[cat].push(cp);
    });
    return groups;
  }, [cropProfiles]);

  useEffect(() => {
    if (station) {
      let matchedId = station.cultura_id;
      if (!matchedId && cropProfiles.length > 0 && station.cultura) {
        const found = cropProfiles.find(
          (c) =>
            c.nome.toLowerCase() === station.cultura.toLowerCase() ||
            c.codigo.toLowerCase() === station.cultura.toLowerCase()
        );
        if (found) matchedId = found.id;
      }

      setForm({
        nome: station.nome || '',
        cultura: station.cultura || '',
        cultura_id: matchedId ? String(matchedId) : '',
        latitude: station.latitude !== undefined && station.latitude !== null ? String(station.latitude) : '',
        longitude: station.longitude !== undefined && station.longitude !== null ? String(station.longitude) : '',
        visibilidade: station.visibilidade || 'PUBLICA',
        status_financeiro: station.status_financeiro || 'PAGO',
        aes_key: station.aes_key || '',
        api_token: station.api_token || ''
      });
      setError('');
    }
  }, [station, cropProfiles]);

  // Perfil atualmente selecionado no formulário
  const activeProfile = useMemo(() => {
    if (form.cultura_id) {
      const match = cropProfiles.find((c) => String(c.id) === String(form.cultura_id));
      if (match) return match;
    }
    if (station?.cultura_perfil && station.cultura_perfil.id) {
      return station.cultura_perfil;
    }
    if (form.cultura) {
      return cropProfiles.find(
        (c) => c.nome.toLowerCase() === form.cultura.toLowerCase()
      );
    }
    return null;
  }, [form.cultura_id, form.cultura, cropProfiles, station]);

  if (!isOpen || !station) return null;

  const handleCropChange = (e) => {
    const val = e.target.value;
    if (!val) {
      setForm((prev) => ({ ...prev, cultura_id: '', cultura: '' }));
      return;
    }
    const profile = cropProfiles.find((c) => String(c.id) === String(val));
    if (profile) {
      setForm((prev) => ({
        ...prev,
        cultura_id: String(profile.id),
        cultura: profile.nome
      }));
    } else {
      setForm((prev) => ({ ...prev, cultura_id: val }));
    }
  };

  const handleSubmit = async (e) => {
    e.preventDefault();
    setLoading(true);
    setError('');
    try {
      const payload = {
        ...form,
        cultura_id: form.cultura_id ? parseInt(form.cultura_id, 10) : null
      };
      await onSave(station.cod_estacao, payload);
      onClose();
    } catch (err) {
      setError(err.message || 'Erro ao atualizar parâmetros da estação.');
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="fixed inset-0 z-[2000] flex items-center justify-center bg-black/80 backdrop-blur-xs p-4 select-none animate-in fade-in duration-150">
      <div className="bg-zinc-950 border border-zinc-800 rounded-lg w-full max-w-xl shadow-2xl overflow-hidden font-mono flex flex-col max-h-[92vh]">
        
        {/* Header */}
        <div className="p-3.5 sm:p-4 bg-zinc-900/80 border-b border-zinc-800 flex items-center justify-between shrink-0">
          <div className="flex items-center gap-2">
            <Settings className="w-4 h-4 text-emerald-400" />
            <span className="font-bold text-xs uppercase tracking-wider text-zinc-100">
              PARÂMETROS DA ESTAÇÃO [{station.cod_estacao}]
            </span>
          </div>
          <button
            onClick={onClose}
            className="p-1 rounded text-zinc-400 hover:text-white hover:bg-zinc-800 transition cursor-pointer"
          >
            <X className="w-4 h-4" />
          </button>
        </div>

        {/* Formulário de Configuração */}
        <form onSubmit={handleSubmit} className="p-4 sm:p-5 space-y-4 overflow-y-auto flex-1">
          
          {error && (
            <div className="p-2.5 rounded bg-rose-950/60 border border-rose-800 text-rose-300 text-xs flex items-center gap-2">
              <AlertCircle className="w-4 h-4 shrink-0" />
              <span>{error}</span>
            </div>
          )}

          <div>
            <label className="block text-[11px] text-zinc-400 mb-1">NOME DA ESTAÇÃO / IDENTIFICADOR DE TALHÃO</label>
            <input
              type="text"
              required
              value={form.nome}
              onChange={(e) => setForm({ ...form, nome: e.target.value })}
              className="w-full bg-zinc-900 border border-zinc-700 rounded px-3 py-2 text-xs text-zinc-100 focus:outline-none focus:border-emerald-500"
            />
          </div>

          {/* Seletor do Catálogo de Culturas */}
          <div>
            <div className="flex items-center justify-between mb-1">
              <label className="block text-[11px] text-zinc-400">
                CULTURA AGRÍCOLA MONITORADA (CATÁLOGO FAO-56 / EMBRAPA)
              </label>
              {activeProfile && (
                <span className="text-[10px] text-emerald-400 font-bold">
                  {activeProfile.categoria}
                </span>
              )}
            </div>

            <select
              value={form.cultura_id}
              onChange={handleCropChange}
              className="w-full bg-zinc-900 border border-zinc-700 rounded px-3 py-2 text-xs text-zinc-100 focus:outline-none focus:border-emerald-500 cursor-pointer"
            >
              <option value="">Selecione uma cultura do catálogo agronômico...</option>
              {Object.keys(groupedCrops).map((categoria) => (
                <optgroup key={categoria} label={`-- ${categoria.toUpperCase()} --`}>
                  {groupedCrops[categoria].map((crop) => (
                    <option key={crop.id} value={crop.id}>
                      {crop.nome} (Kc {crop.kc_medio})
                    </option>
                  ))}
                </optgroup>
              ))}
            </select>
          </div>

          {/* Preview dos Parâmetros Agronômicos Vinculados */}
          {activeProfile && (
            <div className="p-3 rounded-lg bg-zinc-900/90 border border-emerald-900/50 space-y-2.5">
              <div className="flex items-center justify-between text-xs">
                <span className="flex items-center gap-1.5 text-emerald-400 font-bold">
                  <Sprout className="w-3.5 h-3.5" />
                  PADRÃO AGRONÔMICO CALIBRADO: {activeProfile.nome}
                </span>
                <span className="text-[10px] px-1.5 py-0.5 rounded bg-emerald-950 text-emerald-300 border border-emerald-800">
                  FAO-56 / EMBRAPA
                </span>
              </div>

              <div className="grid grid-cols-2 sm:grid-cols-4 gap-2 text-[11px]">
                <div className="bg-zinc-950 p-2 rounded border border-zinc-800">
                  <span className="text-zinc-500 block text-[10px]">KC MÉDIO (ETC)</span>
                  <span className="font-bold text-zinc-200">{activeProfile.kc_medio}</span>
                  <span className="text-zinc-500 block text-[9px]">Fim: {activeProfile.kc_final || '--'}</span>
                </div>

                <div className="bg-zinc-950 p-2 rounded border border-zinc-800">
                  <span className="text-zinc-500 block text-[10px]">TEMP. ÓTIMA</span>
                  <span className="font-bold text-emerald-400">
                    {activeProfile.temp_otima_min}°C - {activeProfile.temp_otima_max}°C
                  </span>
                  <span className="text-zinc-500 block text-[9px]">Calor: &gt; {activeProfile.temp_estresse_calor}°C</span>
                </div>

                <div className="bg-zinc-950 p-2 rounded border border-zinc-800">
                  <span className="text-zinc-500 block text-[10px]">ALERTA GEADA</span>
                  <span className="font-bold text-sky-400">
                    &lt; {activeProfile.temp_critica_geada}°C
                  </span>
                  <span className="text-zinc-500 block text-[9px]">Folha/Tronco</span>
                </div>

                <div className="bg-zinc-950 p-2 rounded border border-zinc-800">
                  <span className="text-zinc-500 block text-[10px]">SONDA DE SOLO</span>
                  <span className="font-bold text-amber-400">
                    {activeProfile.profundidade_raiz_cm || 40} cm
                  </span>
                  <span className="text-zinc-500 block text-[9px]">Profund. Raiz</span>
                </div>
              </div>

              {activeProfile.doencas_monitoradas && (
                <div className="text-[10px] bg-zinc-950 p-2 rounded border border-zinc-800/80 flex items-start gap-1.5">
                  <Bug className="w-3.5 h-3.5 text-rose-400 shrink-0 mt-0.5" />
                  <div>
                    <span className="text-zinc-400 font-semibold">Doenças Monitoradas: </span>
                    <span className="text-zinc-300">{activeProfile.doencas_monitoradas}</span>
                  </div>
                </div>
              )}
            </div>
          )}

          <div className="grid grid-cols-2 gap-3">
            <div>
              <label className="block text-[11px] text-zinc-400 mb-1">LATITUDE (WGS84)</label>
              <input
                type="number"
                step="any"
                required
                value={form.latitude}
                onChange={(e) => setForm({ ...form, latitude: e.target.value })}
                className="w-full bg-zinc-900 border border-zinc-700 rounded px-3 py-2 text-xs text-zinc-100 focus:outline-none focus:border-emerald-500"
              />
            </div>
            <div>
              <label className="block text-[11px] text-zinc-400 mb-1">LONGITUDE (WGS84)</label>
              <input
                type="number"
                step="any"
                required
                value={form.longitude}
                onChange={(e) => setForm({ ...form, longitude: e.target.value })}
                className="w-full bg-zinc-900 border border-zinc-700 rounded px-3 py-2 text-xs text-zinc-100 focus:outline-none focus:border-emerald-500"
              />
            </div>
          </div>

          <div className="grid grid-cols-2 gap-3">
            <div>
              <label className="block text-[11px] text-zinc-400 mb-1">VISIBILIDADE</label>
              <select
                value={form.visibilidade}
                onChange={(e) => setForm({ ...form, visibilidade: e.target.value })}
                className="w-full bg-zinc-900 border border-zinc-700 rounded px-3 py-2 text-xs text-zinc-100 focus:outline-none focus:border-emerald-500 cursor-pointer"
              >
                <option value="PUBLICA">PÚBLICA (Comunitária)</option>
                <option value="PRIVADA">PRIVADA (Restrita)</option>
              </select>
            </div>

            {user?.role === 'ADMIN' && (
              <div>
                <label className="block text-[11px] text-zinc-400 mb-1">STATUS FINANCEIRO</label>
                <select
                  value={form.status_financeiro}
                  onChange={(e) => setForm({ ...form, status_financeiro: e.target.value })}
                  className="w-full bg-zinc-900 border border-zinc-700 rounded px-3 py-2 text-xs text-zinc-100 focus:outline-none focus:border-emerald-500 cursor-pointer"
                >
                  <option value="PAGO">REGULAR / PAGO</option>
                  <option value="PENDENTE">PENDENTE (Bloqueio)</option>
                </select>
              </div>
            )}
          </div>

          <div>
            <label className="block text-[11px] text-zinc-400 mb-1">CHAVE CRIPTOGRÁFICA AES-128 (HEX)</label>
            <input
              type="text"
              value={form.aes_key}
              onChange={(e) => setForm({ ...form, aes_key: e.target.value })}
              className="w-full bg-zinc-900 border border-zinc-700 rounded px-3 py-2 text-xs text-zinc-100 focus:outline-none focus:border-emerald-500 font-mono"
              placeholder="32 caracteres hexadecimais..."
            />
          </div>

          <div>
            <label className="block text-[11px] text-zinc-400 mb-1">TOKEN DE INGESTÃO HTTP/API</label>
            <input
              type="text"
              value={form.api_token}
              onChange={(e) => setForm({ ...form, api_token: e.target.value })}
              className="w-full bg-zinc-900 border border-zinc-700 rounded px-3 py-2 text-xs text-zinc-100 focus:outline-none focus:border-emerald-500 font-mono"
            />
          </div>

          <div className="pt-2 flex justify-end gap-2 border-t border-zinc-800">
            <button
              type="button"
              onClick={onClose}
              className="px-4 py-2 rounded bg-zinc-900 hover:bg-zinc-800 border border-zinc-700 text-xs text-zinc-300 transition cursor-pointer"
            >
              CANCELAR
            </button>
            <button
              type="submit"
              disabled={loading}
              className="flex items-center gap-1.5 px-4 py-2 rounded bg-emerald-600 hover:bg-emerald-500 text-white text-xs font-bold transition cursor-pointer disabled:opacity-50"
            >
              <Save className="w-3.5 h-3.5" />
              <span>{loading ? 'SALVANDO...' : 'SALVAR ALTERAÇÕES'}</span>
            </button>
          </div>

        </form>

      </div>
    </div>
  );
}
