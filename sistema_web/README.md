# 🌾 Sistema Web & Plataforma de Telemetria - Projeto GAIA

Este diretório contém a **Plataforma Web Completa** do Projeto GAIA, composta pela **API Backend REST/WebSocket** e pelo **Dashboard em Tempo Real** para monitoramento agrometeorológico de precisão.

---

## 🏛️ 1. Arquitetura da Aplicação

* **Backend (`backend/`):**
  * **Runtime:** Node.js (Express.js)
  * **Banco de Dados:** PostgreSQL 16 (Armazenamento histórico de séries temporais e catálogo agronômico)
  * **Segurança:** Descriptografia AES-128 nativa, autenticação JWT e senhas com hash Bcrypt
  * **Tempo Real:** Servidor WebSocket (`ws`) para atualização imediata dos cards de telemetria
  * **Motor Agronômico:** Cálculo automático de VPD, Evapotranspiração de Referência ($ET_0$), Demanda da Cultura ($ET_c$), Ponto de Orvalho e Molhamento Foliar baseado nos modelos da **FAO-56** e **Embrapa**.

* **Frontend (`frontend/`):**
  * **Framework:** React 19 + Vite
  * **Estilização:** Tailwind CSS v4 (Interface Dark Mode de alta legibilidade em campo)
  * **Mapas:** Leaflet / OpenStreetMap com renderização de satélite em tela cheia e marcadores dinâmicos
  * **Gráficos:** Chart.js com curvas climáticas, limites agronômicos e histórico de 24h a 30 dias

---

## 🚀 2. Como Executar com Docker Compose (Recomendado)

A forma mais simples de rodar todo o sistema (banco de dados + servidor web) é utilizando o Docker Compose:

```bash
cd sistema_web
docker compose up -d
```

O sistema estará acessível em:
👉 **`http://localhost:3000`**

Para parar a execução:
```bash
docker compose down
```

---

## 💻 3. Como Executar em Modo de Desenvolvimento Local

### A. Backend:
```bash
cd sistema_web/backend
npm install
node server.js
```

### B. Frontend:
```bash
cd sistema_web/frontend
npm install
npm run dev
```

---

## 🔑 4. Credenciais Padrão de Acesso ao Dashboard

| Perfil | E-mail | Senha Padrão | Permissões |
| :--- | :--- | :--- | :--- |
| **Operador / Produtor** | `usuario@gaia.agro.br` | `GaiaUser2026!` | Visualização de estações públicas, mapas e séries temporais |
| **Administrador / Gestor** | `admin@gaia.agro.br` | `GaiaAdmin2026!` | Gestão completa de estações, catálogo de culturas e status financeiro |

> [!TIP]
> Em produção, as credenciais administrativas e segredos de criptografia podem ser customizados criando um arquivo `.env` com as variáveis: `ADMIN_EMAIL`, `ADMIN_PASSWORD`, `JWT_SECRET` e `AES_KEY`.
