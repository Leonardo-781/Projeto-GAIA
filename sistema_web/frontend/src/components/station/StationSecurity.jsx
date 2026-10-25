import React from 'react';
import { Lock, ShieldAlert, KeyRound, ExternalLink } from 'lucide-react';

export default function StationSecurity({
  station,
  onLoginClick
}) {
  if (!station) return null;

  const isPrivate = station.visibilidade === 'PRIVADA';
  const isPending = station.status_financeiro === 'PENDENTE';

  if (!isPrivate && !isPending) return null;

  return (
    <div className="p-6 bg-zinc-900 border border-zinc-800 rounded-lg max-w-md mx-auto my-12 text-center select-none shadow-xl font-mono">
      {isPending ? (
        <div className="flex flex-col items-center">
          <div className="w-12 h-12 rounded-full bg-rose-950/80 border border-rose-700 flex items-center justify-center text-rose-400 mb-3">
            <ShieldAlert className="w-6 h-6" />
          </div>
          <h3 className="text-sm font-bold text-zinc-100 uppercase tracking-wide">
            Estação Bloqueada - Faturamento Pendente
          </h3>
          <p className="text-xs text-zinc-400 mt-2 font-sans">
            A estação <strong>{station.cod_estacao}</strong> está temporariamente bloqueada para visualização de telemetria analítica devido a pendências de assinatura de serviço de dados telemétricos.
          </p>
          <div className="mt-4 px-3 py-2 bg-zinc-950 border border-zinc-800 rounded text-xs text-zinc-300">
            Contate o administrador da rede ou renove a assinatura na Central de Contratos.
          </div>
        </div>
      ) : (
        <div className="flex flex-col items-center">
          <div className="w-12 h-12 rounded-full bg-amber-950/80 border border-amber-700 flex items-center justify-center text-amber-400 mb-3">
            <Lock className="w-6 h-6" />
          </div>
          <h3 className="text-sm font-bold text-zinc-100 uppercase tracking-wide">
            Estação Privada de Campo
          </h3>
          <p className="text-xs text-zinc-400 mt-2 font-sans">
            Os dados telemétricos de <strong>{station.cod_estacao}</strong> são confidenciais e protegidos por chave de criptografia do proprietário do talhão.
          </p>
          <button
            onClick={onLoginClick}
            className="mt-4 flex items-center gap-2 px-4 py-2 rounded bg-emerald-600 hover:bg-emerald-500 text-white text-xs font-bold transition cursor-pointer"
          >
            <KeyRound className="w-4 h-4" />
            <span>AUTENTICAR COM CREDENCIAIS</span>
          </button>
        </div>
      )}
    </div>
  );
}
