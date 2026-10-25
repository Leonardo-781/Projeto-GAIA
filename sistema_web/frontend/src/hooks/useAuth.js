import { useState, useEffect, useCallback } from 'react';

export function useAuth() {
  const [user, setUser] = useState(null);
  const [authModalOpen, setAuthModalOpen] = useState(false);
  const [authMode, setAuthMode] = useState('login'); // 'login' | 'register'
  const [authForm, setAuthForm] = useState({ nome: '', email: '', senha: '' });
  const [authError, setAuthError] = useState('');
  const [authLoading, setAuthLoading] = useState(false);

  const getAuthHeaders = useCallback(() => {
    const token = localStorage.getItem('gaia_token');
    return token ? { 'Authorization': 'Bearer ' + token } : {};
  }, []);

  const verificarSessao = useCallback(async () => {
    const token = localStorage.getItem('gaia_token');
    if (!token) {
      setUser(null);
      return;
    }
    try {
      const res = await fetch('/api/auth/me', { headers: { 'Authorization': 'Bearer ' + token } });
      if (res.ok) {
        const data = await res.json();
        setUser(data.user);
      } else {
        localStorage.removeItem('gaia_token');
        setUser(null);
      }
    } catch (e) {
      console.error('Erro na verificação de sessão:', e);
      setUser(null);
    }
  }, []);

  useEffect(() => {
    verificarSessao();
  }, [verificarSessao]);

  const handleLoginRegister = async (e) => {
    if (e) e.preventDefault();
    setAuthLoading(true);
    setAuthError('');
    try {
      const endpoint = authMode === 'login' ? '/api/auth/login' : '/api/auth/registrar';
      const res = await fetch(endpoint, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(authForm)
      });
      const data = await res.json();
      if (!res.ok) {
        throw new Error(data.error || 'Erro na autenticação.');
      }
      localStorage.setItem('gaia_token', data.token);
      setUser(data.user);
      setAuthModalOpen(false);
      setAuthForm({ nome: '', email: '', senha: '' });
    } catch (err) {
      setAuthError(err.message || 'Falha na conexão com o servidor.');
    } finally {
      setAuthLoading(false);
    }
  };

  const handleLogout = () => {
    localStorage.removeItem('gaia_token');
    setUser(null);
  };

  return {
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
    verificarSessao,
    handleLoginRegister,
    handleLogout
  };
}
