import React from 'react';
import { X, Lock, Mail, User, Shield, AlertCircle } from 'lucide-react';

export default function AuthModal({
  isOpen,
  onClose,
  authMode,
  setAuthMode,
  authForm,
  setAuthForm,
  authError,
  authLoading,
  onSubmit
}) {
  if (!isOpen) return null;

  return (
    <div className="fixed inset-0 z-[2000] flex items-center justify-center bg-black/80 backdrop-blur-xs p-4 select-none animate-in fade-in duration-150">
      <div className="bg-zinc-950 border border-zinc-800 rounded-lg w-full max-w-sm shadow-2xl overflow-hidden font-mono">
        
        {/* Header do Modal */}
        <div className="p-4 bg-zinc-900/80 border-b border-zinc-800 flex items-center justify-between">
          <div className="flex items-center gap-2">
            <Shield className="w-4 h-4 text-emerald-400" />
            <span className="font-bold text-xs uppercase tracking-wider text-zinc-100">
              {authMode === 'login' ? 'AUTENTICAÇÃO TÉCNICA' : 'NOVO REGISTRO'}
            </span>
          </div>
          <button
            onClick={onClose}
            className="p-1 rounded text-zinc-400 hover:text-white hover:bg-zinc-800 transition cursor-pointer"
          >
            <X className="w-4 h-4" />
          </button>
        </div>

        {/* Formulário */}
        <form onSubmit={onSubmit} className="p-5 space-y-4">
          
          {authError && (
            <div className="p-2.5 rounded bg-rose-950/60 border border-rose-800 text-rose-300 text-xs flex items-center gap-2">
              <AlertCircle className="w-4 h-4 shrink-0" />
              <span>{authError}</span>
            </div>
          )}

          {authMode === 'register' && (
            <div>
              <label className="block text-[11px] text-zinc-400 mb-1">NOME COMPLETO</label>
              <div className="relative">
                <User className="w-4 h-4 text-zinc-500 absolute left-2.5 top-2.5" />
                <input
                  type="text"
                  required
                  placeholder="Seu nome"
                  value={authForm.nome}
                  onChange={(e) => setAuthForm({ ...authForm, nome: e.target.value })}
                  className="w-full bg-zinc-900 border border-zinc-700 rounded pl-8 pr-3 py-2 text-xs text-zinc-100 placeholder-zinc-500 focus:outline-none focus:border-emerald-500"
                />
              </div>
            </div>
          )}

          <div>
            <label className="block text-[11px] text-zinc-400 mb-1">E-MAIL CORPORATIVO / AGRO</label>
            <div className="relative">
              <Mail className="w-4 h-4 text-zinc-500 absolute left-2.5 top-2.5" />
              <input
                type="email"
                required
                placeholder="operador@fazenda.com"
                value={authForm.email}
                onChange={(e) => setAuthForm({ ...authForm, email: e.target.value })}
                className="w-full bg-zinc-900 border border-zinc-700 rounded pl-8 pr-3 py-2 text-xs text-zinc-100 placeholder-zinc-500 focus:outline-none focus:border-emerald-500"
              />
            </div>
          </div>

          <div>
            <label className="block text-[11px] text-zinc-400 mb-1">CHAVE / SENHA</label>
            <div className="relative">
              <Lock className="w-4 h-4 text-zinc-500 absolute left-2.5 top-2.5" />
              <input
                type="password"
                required
                placeholder="••••••••"
                value={authForm.senha}
                onChange={(e) => setAuthForm({ ...authForm, senha: e.target.value })}
                className="w-full bg-zinc-900 border border-zinc-700 rounded pl-8 pr-3 py-2 text-xs text-zinc-100 placeholder-zinc-500 focus:outline-none focus:border-emerald-500"
              />
            </div>
          </div>

          <button
            type="submit"
            disabled={authLoading}
            className="w-full mt-2 py-2.5 rounded bg-emerald-600 hover:bg-emerald-500 text-white font-bold text-xs uppercase tracking-wider transition cursor-pointer shadow disabled:opacity-50"
          >
            {authLoading
              ? 'PROCESSANDO...'
              : authMode === 'login'
              ? 'CONECTAR AO CENTRO DE COMANDO'
              : 'CONCLUIR CADASTRO'}
          </button>

          <div className="text-center pt-2">
            <button
              type="button"
              onClick={() => {
                setAuthMode(authMode === 'login' ? 'register' : 'login');
              }}
              className="text-xs text-zinc-400 hover:text-emerald-400 transition cursor-pointer"
            >
              {authMode === 'login'
                ? 'Primeiro acesso? Crie seu perfil de operador'
                : 'Já possui cadastro? Fazer login'}
            </button>
          </div>

        </form>

      </div>
    </div>
  );
}
