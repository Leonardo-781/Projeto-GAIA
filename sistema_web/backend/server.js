const express = require('express');
const http = require('http');
const { WebSocketServer, WebSocket } = require('ws');
const { Pool } = require('pg');
const crypto = require('crypto');
const path = require('path');
const bcrypt = require('bcryptjs');
const jwt = require('jsonwebtoken');

const PORT = process.env.PORT || 3000;
const AES_KEY = process.env.AES_KEY || 'GaiaAgroIoT2026!';
const LEGACY_AES_KEY = 'MARIACRYPTOKEY12';
const GAIA_PROJECT_TOKEN = process.env.GAIA_PROJECT_TOKEN || 'GAIA_TOKEN_2026_AGRO';
const DATABASE_URL = process.env.DATABASE_URL || 'postgresql://server_admin:server_password_123@postgres:5432/app_db';
const JWT_SECRET = process.env.JWT_SECRET || 'GAIA_SUPER_SECRET_KEY_2026_JWT';

const app = express();
app.use(express.json({ limit: '2mb' }));
app.use(express.urlencoded({ extended: true }));
app.use(express.static(path.join(__dirname, 'public')));

// Pool de conexão com o PostgreSQL
const pool = new Pool({
  connectionString: DATABASE_URL,
});

// Inicialização e verificação de tabelas
async function initDatabase() {
  try {
    // 1. Tabela de Usuários
    await pool.query(`
      CREATE TABLE IF NOT EXISTS gaia_usuarios (
        id SERIAL PRIMARY KEY,
        nome VARCHAR(100) NOT NULL,
        email VARCHAR(150) UNIQUE NOT NULL,
        senha_hash VARCHAR(255) NOT NULL,
        role VARCHAR(20) DEFAULT 'USER',
        criado_em TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP
      );
    `);

    // 2. Tabela de Telemetria
    await pool.query(`
      CREATE TABLE IF NOT EXISTS gaia_telemetria (
        id SERIAL PRIMARY KEY,
        cod_estacao VARCHAR(16) NOT NULL,
        timestamp_utc BIGINT NOT NULL,
        data_hora TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP,
        temp_bmp NUMERIC(5,2),
        pressao NUMERIC(7,2),
        lux NUMERIC(8,2),
        umidade NUMERIC(5,2),
        temp_dht NUMERIC(5,2),
        umidade_solo INT,
        temp_solo NUMERIC(5,2),
        temp_ref NUMERIC(5,2),
        delta_temp NUMERIC(5,2),
        qualidade INT,
        ponto_orvalho NUMERIC(5,2),
        vpd NUMERIC(6,3),
        et0 NUMERIC(6,4),
        etc NUMERIC(6,4),
        rs NUMERIC(7,2),
        par NUMERIC(7,2),
        rf INT DEFAULT 0,
        rg NUMERIC(6,2) DEFAULT 0,
        dp NUMERIC(5,2),
        at BOOLEAN DEFAULT FALSE,
        itu NUMERIC(5,2),
        hi NUMERIC(5,2),
        heap_mod INT,
        raw_payload JSONB
      );
      CREATE INDEX IF NOT EXISTS idx_gaia_estacao_ts ON gaia_telemetria (cod_estacao, timestamp_utc DESC);
    `);

    // Migração de colunas adicionais para bancos já existentes
    await pool.query(`
      ALTER TABLE gaia_telemetria ADD COLUMN IF NOT EXISTS etc NUMERIC(6,4);
      ALTER TABLE gaia_telemetria ADD COLUMN IF NOT EXISTS rs NUMERIC(7,2);
      ALTER TABLE gaia_telemetria ADD COLUMN IF NOT EXISTS par NUMERIC(7,2);
      ALTER TABLE gaia_telemetria ADD COLUMN IF NOT EXISTS rf INT DEFAULT 0;
      ALTER TABLE gaia_telemetria ADD COLUMN IF NOT EXISTS rg NUMERIC(6,2) DEFAULT 0;
      ALTER TABLE gaia_telemetria ADD COLUMN IF NOT EXISTS dp NUMERIC(5,2);
      ALTER TABLE gaia_telemetria ADD COLUMN IF NOT EXISTS at BOOLEAN DEFAULT FALSE;
      ALTER TABLE gaia_telemetria ADD COLUMN IF NOT EXISTS itu NUMERIC(5,2);
      ALTER TABLE gaia_telemetria ADD COLUMN IF NOT EXISTS hi NUMERIC(5,2);
    `);

    // 3. Tabela de Perfis Agronômicos de Culturas (FAO-56 & Embrapa)
    await pool.query(`
      CREATE TABLE IF NOT EXISTS gaia_culturas_perfil (
        id SERIAL PRIMARY KEY,
        codigo VARCHAR(32) UNIQUE NOT NULL,
        nome VARCHAR(64) NOT NULL,
        categoria VARCHAR(32) NOT NULL,
        kc_inicial NUMERIC(4,2) DEFAULT 0.40,
        kc_medio NUMERIC(4,2) DEFAULT 1.05,
        kc_final NUMERIC(4,2) DEFAULT 0.85,
        temp_basal_inferior NUMERIC(4,1) DEFAULT 10.0,
        temp_otima_min NUMERIC(4,1) DEFAULT 18.0,
        temp_otima_max NUMERIC(4,1) DEFAULT 24.0,
        temp_estresse_calor NUMERIC(4,1) DEFAULT 32.0,
        temp_critica_geada NUMERIC(4,1) DEFAULT 2.0,
        vpd_otimo_min NUMERIC(4,2) DEFAULT 0.8,
        vpd_otimo_max NUMERIC(4,2) DEFAULT 1.2,
        vpd_estresse NUMERIC(4,2) DEFAULT 1.6,
        profundidade_raiz_cm INT DEFAULT 40,
        fator_esgotamento_p NUMERIC(3,2) DEFAULT 0.45,
        doencas_monitoradas VARCHAR(150),
        alerta_fungo_temp_min NUMERIC(4,1) DEFAULT 20.0,
        alerta_fungo_temp_max NUMERIC(4,1) DEFAULT 26.0,
        alerta_fungo_umid_min NUMERIC(4,1) DEFAULT 85.0
      );

      INSERT INTO gaia_culturas_perfil (
        codigo, nome, categoria, kc_inicial, kc_medio, kc_final,
        temp_basal_inferior, temp_otima_min, temp_otima_max, temp_estresse_calor, temp_critica_geada,
        vpd_otimo_min, vpd_otimo_max, vpd_estresse, profundidade_raiz_cm, fator_esgotamento_p,
        doencas_monitoradas, alerta_fungo_temp_min, alerta_fungo_temp_max, alerta_fungo_umid_min
      ) VALUES 
      ('CAFE_ARABICA', 'Café Arábica', 'Perene', 0.85, 1.05, 0.90, 10.0, 18.0, 23.0, 32.0, 2.5, 0.80, 1.20, 1.60, 40, 0.45, 'Ferrugem do Cafeeiro e Cercosporiose', 20.0, 26.0, 85.0),
      ('CAFE_CONILON', 'Café Conilon (Robusta)', 'Perene', 0.90, 1.10, 0.95, 13.0, 22.0, 27.0, 35.0, 4.0, 0.80, 1.30, 1.70, 35, 0.45, 'Mancha Aureolada e Ferrugem', 22.0, 28.0, 85.0),
      ('SOJA', 'Soja', 'Grãos', 0.40, 1.15, 0.50, 10.0, 20.0, 30.0, 35.0, 0.0, 0.80, 1.40, 1.80, 40, 0.50, 'Ferrugem Asiática e Mancha Alvo', 18.0, 26.0, 80.0),
      ('MILHO', 'Milho (Grão e Silagem)', 'Grãos', 0.35, 1.20, 0.60, 10.0, 24.0, 30.0, 36.0, 1.0, 0.80, 1.50, 2.00, 50, 0.55, 'Cercosporiose, Enfezamento e Mancha Branca', 22.0, 30.0, 80.0),
      ('FEIJAO', 'Feijão Comum', 'Grãos', 0.40, 1.10, 0.35, 10.0, 18.0, 25.0, 30.0, 2.0, 0.80, 1.30, 1.60, 30, 0.45, 'Antracnose e Mofo Branco', 16.0, 22.0, 85.0),
      ('ALGODAO', 'Algodão', 'Fibras', 0.45, 1.15, 0.60, 12.0, 25.0, 32.0, 38.0, 2.0, 0.80, 1.50, 2.00, 60, 0.65, 'Ramulária e Mancha de Phoma', 22.0, 30.0, 80.0),
      ('CANA_DE_ACUCAR', 'Cana-de-Açúcar', 'Semi-perene', 0.40, 1.25, 0.70, 12.0, 26.0, 33.0, 38.0, 0.0, 0.80, 1.60, 2.20, 60, 0.65, 'Ferrugem Alaranjada e Carvão', 24.0, 30.0, 80.0),
      ('TRIGO', 'Trigo', 'Grãos', 0.35, 1.15, 0.40, 4.5, 15.0, 22.0, 30.0, -2.0, 0.60, 1.20, 1.50, 40, 0.55, 'Brusone do Trigo e Giberela', 18.0, 24.0, 85.0),
      ('BATATA', 'Batata', 'Hortifrúti', 0.50, 1.15, 0.75, 7.0, 16.0, 20.0, 26.0, 1.0, 0.60, 1.10, 1.40, 30, 0.35, 'Requeima e Pinta Preta', 15.0, 22.0, 90.0),
      ('TOMATE', 'Tomateiro', 'Hortifrúti', 0.60, 1.15, 0.80, 10.0, 19.0, 24.0, 31.0, 2.0, 0.70, 1.20, 1.50, 35, 0.40, 'Requeima e Murcha Bacteriana', 18.0, 24.0, 85.0),
      ('CITROS', 'Citros (Laranja e Limão)', 'Perene', 0.70, 0.80, 0.75, 13.0, 23.0, 32.0, 37.0, -1.0, 0.80, 1.40, 1.80, 50, 0.50, 'Cancro Cítrico e Clorose Variegada', 24.0, 32.0, 80.0),
      ('PASTAGEM_BRAQUIARIA', 'Pastagem (Braquiária)', 'Forrageira', 0.70, 0.95, 0.80, 15.0, 25.0, 35.0, 40.0, 1.0, 0.80, 1.80, 2.50, 50, 0.60, 'Cigarrinha-das-pastagens', 24.0, 32.0, 80.0)
      ON CONFLICT (codigo) DO UPDATE SET
        nome = EXCLUDED.nome,
        categoria = EXCLUDED.categoria,
        kc_medio = EXCLUDED.kc_medio,
        temp_basal_inferior = EXCLUDED.temp_basal_inferior,
        temp_otima_min = EXCLUDED.temp_otima_min,
        temp_otima_max = EXCLUDED.temp_otima_max,
        temp_estresse_calor = EXCLUDED.temp_estresse_calor,
        temp_critica_geada = EXCLUDED.temp_critica_geada,
        vpd_otimo_min = EXCLUDED.vpd_otimo_min,
        vpd_otimo_max = EXCLUDED.vpd_otimo_max,
        vpd_estresse = EXCLUDED.vpd_estresse,
        doencas_monitoradas = EXCLUDED.doencas_monitoradas;
    `);

    // 4. Tabela de Cadastro e Geolocalização das Estações
    await pool.query(`
      CREATE TABLE IF NOT EXISTS gaia_estacoes (
        cod_estacao VARCHAR(16) PRIMARY KEY,
        nome VARCHAR(64) NOT NULL,
        cultura VARCHAR(64) DEFAULT 'Agricultura Geral',
        latitude NUMERIC(10, 6) NOT NULL DEFAULT -18.7300,
        longitude NUMERIC(10, 6) NOT NULL DEFAULT -47.4980,
        visibilidade VARCHAR(20) DEFAULT 'PUBLICA',
        status_financeiro VARCHAR(20) DEFAULT 'ATIVO',
        proprietario_id INT REFERENCES gaia_usuarios(id) ON DELETE SET NULL,
        api_token VARCHAR(64) DEFAULT NULL,
        ativo BOOLEAN DEFAULT TRUE,
        cultura_id INT REFERENCES gaia_culturas_perfil(id),
        criado_em TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP,
        atualizado_em TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP
      );
      ALTER TABLE gaia_estacoes ADD COLUMN IF NOT EXISTS api_token VARCHAR(64) DEFAULT NULL;
      ALTER TABLE gaia_estacoes ADD COLUMN IF NOT EXISTS cultura_id INT REFERENCES gaia_culturas_perfil(id);
    `);

    // 4. Contas de Acesso Padrão (Administrador e Usuário Operador)
    // Permite customização segura via variáveis de ambiente
    const adminEmail = process.env.ADMIN_EMAIL || 'admin@gaia.agro.br';
    const adminSenha = process.env.ADMIN_PASSWORD || 'GaiaAdmin2026!';
    const adminHash = bcrypt.hashSync(adminSenha, 10);

    const userEmail = process.env.DEMO_USER_EMAIL || 'usuario@gaia.agro.br';
    const userSenha = process.env.DEMO_USER_PASSWORD || 'GaiaUser2026!';
    const userHash = bcrypt.hashSync(userSenha, 10);

    // Limpeza de contas administrativas antigas que não correspondam ao padrão configurado
    await pool.query("DELETE FROM gaia_usuarios WHERE email NOT IN ($1, $2)", [adminEmail, userEmail]);

    // Criação/Atualização da Conta Administrador
    const adminCheck = await pool.query('SELECT * FROM gaia_usuarios WHERE email = $1', [adminEmail]);
    if (adminCheck.rows.length === 0) {
      await pool.query(`
        INSERT INTO gaia_usuarios (nome, email, senha_hash, role)
        VALUES ($1, $2, $3, 'ADMIN');
      `, ['Administrador GAIA', adminEmail, adminHash]);
      console.log(`[AUTH] Conta Administrador inicializada: ${adminEmail}`);
    } else {
      await pool.query(`
        UPDATE gaia_usuarios 
        SET senha_hash = $1, role = 'ADMIN' 
        WHERE email = $2
      `, [adminHash, adminEmail]);
    }

    // Criação/Atualização de Conta de Usuário Padrão / Demonstração
    const userCheck = await pool.query('SELECT * FROM gaia_usuarios WHERE email = $1', [userEmail]);
    if (userCheck.rows.length === 0) {
      await pool.query(`
        INSERT INTO gaia_usuarios (nome, email, senha_hash, role)
        VALUES ($1, $2, $3, 'USER');
      `, ['Operador / Usuário GAIA', userEmail, userHash]);
      console.log(`[AUTH] Conta de Usuário inicializada: ${userEmail}`);
    } else {
      await pool.query(`
        UPDATE gaia_usuarios 
        SET senha_hash = $1, role = 'USER' 
        WHERE email = $2
      `, [userHash, userEmail]);
    }

    // 5. Registro Exclusivo da Estação Real G00001 (Remoção de estações fake)
    await pool.query("DELETE FROM gaia_estacoes WHERE cod_estacao != 'G00001'");
    await pool.query("DELETE FROM gaia_telemetria WHERE cod_estacao != 'G00001'");

    await pool.query(`
      INSERT INTO gaia_estacoes (cod_estacao, nome, cultura, cultura_id, latitude, longitude, visibilidade, status_financeiro, proprietario_id, api_token)
      VALUES 
        ('G00001', 'Estação GAIA G00001 (Monte Carmelo)', 'Café Arábica', (SELECT id FROM gaia_culturas_perfil WHERE codigo = 'CAFE_ARABICA'), -18.726500, -47.498500, 'PUBLICA', 'ATIVO', NULL, 'GAIA_TOKEN_2026_AGRO')
      ON CONFLICT (cod_estacao) DO UPDATE SET
        nome = EXCLUDED.nome,
        cultura = EXCLUDED.cultura,
        cultura_id = (SELECT id FROM gaia_culturas_perfil WHERE codigo = 'CAFE_ARABICA'),
        latitude = EXCLUDED.latitude,
        longitude = EXCLUDED.longitude,
        visibilidade = EXCLUDED.visibilidade,
        status_financeiro = EXCLUDED.status_financeiro,
        api_token = 'GAIA_TOKEN_2026_AGRO';
    `);

    console.log('Banco de dados PostgreSQL e permissões de estações inicializados com sucesso.');
  } catch (err) {
    console.error('Erro ao inicializar banco de dados:', err.message);
  }
}
initDatabase();

// Middleware de Autenticação Opcional (extrai usuário do token se existir)
function obterUsuarioOpcional(req, res, next) {
  const authHeader = req.headers.authorization;
  if (authHeader && authHeader.startsWith('Bearer ')) {
    const token = authHeader.substring(7);
    try {
      const decoded = jwt.verify(token, JWT_SECRET);
      req.user = decoded;
    } catch (e) {
      req.user = null;
    }
  } else {
    req.user = null;
  }
  next();
}

// Middleware de Autenticação Obrigatória
function exigirAuth(req, res, next) {
  const authHeader = req.headers.authorization;
  if (!authHeader || !authHeader.startsWith('Bearer ')) {
    return res.status(401).json({ error: 'Acesso não autorizado. Faça login para continuar.' });
  }
  const token = authHeader.substring(7);
  try {
    const decoded = jwt.verify(token, JWT_SECRET);
    req.user = decoded;
    next();
  } catch (err) {
    return res.status(401).json({ error: 'Sessão expirada ou token inválido. Faça login novamente.' });
  }
}

// ==========================================
// ROTAS DE AUTENTICAÇÃO SEGURA (AUTH)
// ==========================================

// Login de Usuário
app.post('/api/auth/login', async (req, res) => {
  const { email, senha } = req.body;
  if (!email || !senha) {
    return res.status(400).json({ error: 'E-mail e senha são obrigatórios.' });
  }

  try {
    const result = await pool.query('SELECT * FROM gaia_usuarios WHERE email = $1', [email.trim().toLowerCase()]);
    if (result.rows.length === 0) {
      return res.status(401).json({ error: 'Credenciais inválidas. Verifique seu e-mail e senha.' });
    }

    const usuario = result.rows[0];
    const senhaValida = bcrypt.compareSync(senha, usuario.senha_hash);
    if (!senhaValida) {
      return res.status(401).json({ error: 'Credenciais inválidas. Verifique seu e-mail e senha.' });
    }

    // Gera token JWT assinado
    const payload = {
      id: usuario.id,
      nome: usuario.nome,
      email: usuario.email,
      role: usuario.role
    };
    const token = jwt.sign(payload, JWT_SECRET, { expiresIn: '7d' });

    res.json({
      status: 'ok',
      token,
      user: payload
    });
  } catch (err) {
    console.error('Erro no login:', err);
    res.status(500).json({ error: 'Erro interno ao realizar login.' });
  }
});

// Cadastro de Novo Usuário
app.post('/api/auth/registrar', async (req, res) => {
  const { nome, email, senha } = req.body;
  if (!nome || !email || !senha) {
    return res.status(400).json({ error: 'Nome, e-mail e senha são obrigatórios.' });
  }
  if (senha.length < 6) {
    return res.status(400).json({ error: 'A senha deve ter no mínimo 6 caracteres.' });
  }

  try {
    const emailFormatado = email.trim().toLowerCase();
    const check = await pool.query('SELECT id FROM gaia_usuarios WHERE email = $1', [emailFormatado]);
    if (check.rows.length > 0) {
      return res.status(409).json({ error: 'Este e-mail já está cadastrado no sistema.' });
    }

    const salt = bcrypt.genSaltSync(10);
    const hash = bcrypt.hashSync(senha, salt);

    const result = await pool.query(`
      INSERT INTO gaia_usuarios (nome, email, senha_hash, role)
      VALUES ($1, $2, $3, 'USER')
      RETURNING id, nome, email, role, criado_em;
    `, [nome.trim(), emailFormatado, hash]);

    const novoUsuario = result.rows[0];
    const token = jwt.sign(novoUsuario, JWT_SECRET, { expiresIn: '7d' });

    res.status(201).json({
      status: 'ok',
      token,
      user: novoUsuario
    });
  } catch (err) {
    console.error('Erro no cadastro:', err);
    res.status(500).json({ error: 'Erro interno ao registrar usuário.' });
  }
});

// Obter dados do usuário logado atual
app.get('/api/auth/me', exigirAuth, (req, res) => {
  res.json({ user: req.user });
});

// ==========================================
// TELEMETRIA E INGESTÃO DO ESP32
// ==========================================

function decifrarComChave(raw, chaveString) {
  const iv = raw.subarray(0, 16);
  const ciphertext = raw.subarray(16);
  const key = Buffer.from(chaveString.padEnd(16, '0').slice(0, 16), 'utf8');

  const decipher = crypto.createDecipheriv('aes-128-cbc', key, iv);
  decipher.setAutoPadding(true);
  let decrypted = decipher.update(ciphertext, null, 'utf8');
  decrypted += decipher.final('utf8');
  return JSON.parse(decrypted);
}

function descriptografarPayload(cipherB64) {
  try {
    const raw = Buffer.from(cipherB64, 'base64');
    if (raw.length < 32) {
      throw new Error('Buffer criptografado inválido ou muito curto (< 32 bytes).');
    }

    // 1. Tenta decifrar com a chave oficial do Projeto GAIA
    try {
      return decifrarComChave(raw, AES_KEY);
    } catch (err1) {
      // 2. Fallback para a chave legada
      try {
        return decifrarComChave(raw, LEGACY_AES_KEY);
      } catch (err2) {
        throw new Error('Chave AES-128 inválida ou dados corrompidos.');
      }
    }
  } catch (err) {
    console.error('Falha ao descriptografar payload AES:', err.message);
    return null;
  }
}

const server = http.createServer(app);
const wss = new WebSocketServer({ server });

function broadcast(tipo, dados) {
  const mensagem = JSON.stringify({ type: tipo, data: dados });
  wss.clients.forEach(client => {
    if (client.readyState === WebSocket.OPEN) {
      client.send(mensagem);
    }
  });
}

async function processarTelemetria(req, res) {
  let doc = null;

  if (req.body && req.body.data) {
    doc = descriptografarPayload(req.body.data, AES_KEY);
    if (!doc) {
      return res.status(400).json({ error: 'Falha na descriptografia AES-128 ou chave incorreta.' });
    }
  } else if (req.body && (req.body.codSta || req.body.estacao)) {
    doc = req.body;
  } else {
    return res.status(400).json({ error: 'Formato de payload inválido.' });
  }

  const codSta = doc.codSta || doc.estacao || 'G00001';
  const ts = parseInt(doc.ts, 10) || Math.floor(Date.now() / 1000);

  // Validação de Token: aceita o Token Unificado do Projeto GAIA ou o token individual da estação
  const incomingToken = req.headers['x-station-token'] || 
                        (req.headers['authorization'] && req.headers['authorization'].startsWith('Bearer ') ? req.headers['authorization'].substring(7) : null) || 
                        doc.token;

  const isUnifiedProjectToken = (incomingToken && incomingToken === GAIA_PROJECT_TOKEN);

  try {
    const stationRow = await pool.query('SELECT api_token FROM gaia_estacoes WHERE cod_estacao = $1', [codSta]);
    if (stationRow.rows.length > 0 && stationRow.rows[0].api_token && !isUnifiedProjectToken) {
      if (!incomingToken || incomingToken !== stationRow.rows[0].api_token) {
        return res.status(401).json({ error: 'Token de autenticação da estação inválido ou ausente.' });
      }
    }

    const tBmp = doc.tB !== undefined && doc.tB !== null ? parseFloat(doc.tB) : null;
    const pressao = doc.p !== undefined && doc.p !== null ? parseFloat(doc.p) : null;
    const lux = doc.l !== undefined && doc.l !== null ? parseFloat(doc.l) : null;
    const umidade = doc.h !== undefined && doc.h !== null ? parseFloat(doc.h) : null;
    const tDht = doc.tD !== undefined && doc.tD !== null ? parseFloat(doc.tD) : null;
    const solo = doc.s !== undefined && doc.s !== null ? parseInt(doc.s, 10) : null;
    const tSolo = doc.tS !== undefined && doc.tS !== null ? parseFloat(doc.tS) : null;
    const tRef = doc.tRef !== undefined && doc.tRef !== null ? parseFloat(doc.tRef) : (tDht || tBmp);
    const dT = doc.dT !== undefined && doc.dT !== null ? parseFloat(doc.dT) : null;
    const qualidade = doc.q !== undefined && doc.q !== null ? parseInt(doc.q, 10) : 100;
    const po = doc.po !== undefined && doc.po !== null ? parseFloat(doc.po) : null;
    const vpd = doc.vpd !== undefined && doc.vpd !== null ? parseFloat(doc.vpd) : null;
    const et0 = doc.et0 !== undefined && doc.et0 !== null ? parseFloat(doc.et0) : null;
    const etc = doc.etc !== undefined && doc.etc !== null ? parseFloat(doc.etc) : null;
    const rs = doc.rs !== undefined && doc.rs !== null ? parseFloat(doc.rs) : null;
    const par = doc.par !== undefined && doc.par !== null ? parseFloat(doc.par) : null;
    const rf = doc.rf !== undefined && doc.rf !== null ? parseInt(doc.rf, 10) : 0;
    const rg = doc.rg !== undefined && doc.rg !== null ? parseFloat(doc.rg) : 0;
    const dp = doc.dp !== undefined && doc.dp !== null ? parseFloat(doc.dp) : null;
    const at = doc.at !== undefined && doc.at !== null ? Boolean(doc.at) : false;
    const itu = doc.itu !== undefined && doc.itu !== null ? parseFloat(doc.itu) : null;
    const hi = doc.hi !== undefined && doc.hi !== null ? parseFloat(doc.hi) : null;
    const heapMod = doc.heapMod !== undefined && doc.heapMod !== null ? parseInt(doc.heapMod, 10) : null;

    await pool.query(`
      INSERT INTO gaia_estacoes (cod_estacao, nome, api_token)
      VALUES ($1, $2, $3)
      ON CONFLICT (cod_estacao) DO NOTHING;
    `, [codSta, `Estação ${codSta}`, GAIA_PROJECT_TOKEN]);

    const insertQuery = `
      INSERT INTO gaia_telemetria (
        cod_estacao, timestamp_utc, temp_bmp, pressao, lux, umidade, temp_dht,
        umidade_solo, temp_solo, temp_ref, delta_temp, qualidade, ponto_orvalho,
        vpd, et0, etc, rs, par, rf, rg, dp, at, itu, hi, heap_mod, raw_payload
      ) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17, $18, $19, $20, $21, $22, $23, $24, $25, $26)
      RETURNING *;
    `;
    const values = [
      codSta, ts, tBmp, pressao, lux, umidade, tDht, solo, tSolo, tRef, dT,
      qualidade, po, vpd, et0, etc, rs, par, rf, rg, dp, at, itu, hi, heapMod, JSON.stringify(doc)
    ];

    const result = await pool.query(insertQuery, values);
    const leituraSalva = result.rows[0];

    broadcast('NOVA_LEITURA', leituraSalva);

    return res.status(200).json({
      status: 'ok',
      ack: ts,
      station: codSta,
      timestamp: ts,
      server_time: new Date().toISOString()
    });
  } catch (err) {
    console.error('Erro ao salvar no banco:', err.message);
    return res.status(500).json({ error: 'Erro interno ao gravar telemetria.' });
  }
}

app.post('/api/telemetria', processarTelemetria);
app.post('/maria-web', processarTelemetria);
app.post('/maria-web/', processarTelemetria);

// ==========================================
// ROTAS DE ESTAÇÕES COM REGRAS DE ACESSO
// ==========================================

// Rota de listagem do catálogo agronômico de culturas
app.get('/api/culturas', async (req, res) => {
  try {
    const result = await pool.query('SELECT * FROM gaia_culturas_perfil ORDER BY categoria, nome');
    res.json(result.rows);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// Listar estações com controle de visualização (Pública, Privada e Inadimplente)
app.get('/api/estacoes', obterUsuarioOpcional, async (req, res) => {
  try {
    const user = req.user; // Usuário decodificado do JWT se logado
    const isMaster = user && user.role === 'ADMIN';

    const result = await pool.query(`
      SELECT 
        e.cod_estacao,
        e.nome,
        e.cultura,
        e.cultura_id,
        e.latitude,
        e.longitude,
        e.visibilidade,
        e.status_financeiro,
        e.proprietario_id,
        e.api_token,
        e.ativo,
        cp.codigo AS cp_codigo,
        cp.nome AS cp_nome,
        cp.categoria AS cp_categoria,
        cp.kc_inicial AS cp_kc_inicial,
        cp.kc_medio AS cp_kc_medio,
        cp.kc_final AS cp_kc_final,
        cp.temp_basal_inferior AS cp_temp_basal_inferior,
        cp.temp_otima_min AS cp_temp_otima_min,
        cp.temp_otima_max AS cp_temp_otima_max,
        cp.temp_estresse_calor AS cp_temp_estresse_calor,
        cp.temp_critica_geada AS cp_temp_critica_geada,
        cp.vpd_otimo_min AS cp_vpd_otimo_min,
        cp.vpd_otimo_max AS cp_vpd_otimo_max,
        cp.vpd_estresse AS cp_vpd_estresse,
        cp.doencas_monitoradas AS cp_doencas_monitoradas,
        t.timestamp_utc,
        t.data_hora,
        t.temp_ref,
        t.temp_dht,
        t.temp_bmp,
        t.umidade,
        t.umidade_solo,
        t.temp_solo,
        t.vpd,
        t.et0,
        t.etc,
        t.rs,
        t.par,
        t.rf,
        t.rg,
        t.dp,
        t.at,
        t.itu,
        t.hi,
        t.lux,
        t.pressao,
        t.qualidade,
        t.heap_mod
      FROM gaia_estacoes e
      LEFT JOIN gaia_culturas_perfil cp ON cp.id = e.cultura_id
      LEFT JOIN LATERAL (
        SELECT * FROM gaia_telemetria 
        WHERE cod_estacao = e.cod_estacao 
        ORDER BY timestamp_utc DESC LIMIT 1
      ) t ON true
      ORDER BY e.cod_estacao;
    `);

    const agoraSegundos = Math.floor(Date.now() / 1000);
    const estacoes = result.rows.map(r => {
      let status = 'OFFLINE';
      let diffSegundos = null;

      if (r.timestamp_utc) {
        diffSegundos = agoraSegundos - parseInt(r.timestamp_utc, 10);
        if (diffSegundos < 360) status = 'ONLINE';
        else if (diffSegundos <= 900) status = 'DEEP_SLEEP';
        else status = 'OFFLINE';
      }

      // Regra 1: Checagem de Inadimplência / Pagamento Pendente
      const isPendente = r.status_financeiro === 'PENDENTE';

      // Regra 2: Checagem de Privacidade
      const isPrivada = r.visibilidade === 'PRIVADA';
      const isOwner = user && user.id === r.proprietario_id;

      // Determina se os dados de telemetria devem ser bloqueados
      let acesso_bloqueado = false;
      let motivo_bloqueio = null;

      if (!isMaster) {
        if (isPendente) {
          acesso_bloqueado = true;
          motivo_bloqueio = 'MENSALIDADE_PENDENTE';
        } else if (isPrivada && !isOwner) {
          acesso_bloqueado = true;
          motivo_bloqueio = 'ESTACAO_PRIVADA';
        }
      }

      return {
        cod_estacao: r.cod_estacao,
        nome: r.nome,
        cultura: r.cultura,
        cultura_id: r.cultura_id,
        cultura_perfil: r.cultura_id ? {
          id: r.cultura_id,
          codigo: r.cp_codigo,
          nome: r.cp_nome,
          categoria: r.cp_categoria,
          kc_inicial: r.cp_kc_inicial ? parseFloat(r.cp_kc_inicial) : 0.4,
          kc_medio: r.cp_kc_medio ? parseFloat(r.cp_kc_medio) : 1.05,
          kc_final: r.cp_kc_final ? parseFloat(r.cp_kc_final) : 0.85,
          temp_basal_inferior: r.cp_temp_basal_inferior ? parseFloat(r.cp_temp_basal_inferior) : 10.0,
          temp_otima_min: r.cp_temp_otima_min ? parseFloat(r.cp_temp_otima_min) : 18.0,
          temp_otima_max: r.cp_temp_otima_max ? parseFloat(r.cp_temp_otima_max) : 23.0,
          temp_estresse_calor: r.cp_temp_estresse_calor ? parseFloat(r.cp_temp_estresse_calor) : 32.0,
          temp_critica_geada: r.cp_temp_critica_geada ? parseFloat(r.cp_temp_critica_geada) : 2.0,
          vpd_otimo_min: r.cp_vpd_otimo_min ? parseFloat(r.cp_vpd_otimo_min) : 0.8,
          vpd_otimo_max: r.cp_vpd_otimo_max ? parseFloat(r.cp_vpd_otimo_max) : 1.2,
          vpd_estresse: r.cp_vpd_estresse ? parseFloat(r.cp_vpd_estresse) : 1.6,
          doencas_monitoradas: r.cp_doencas_monitoradas
        } : null,
        latitude: parseFloat(r.latitude),
        longitude: parseFloat(r.longitude),
        visibilidade: r.visibilidade,
        status_financeiro: r.status_financeiro,
        status,
        segundos_desde_ultimo_envio: diffSegundos,
        acesso_bloqueado,
        motivo_bloqueio,
        // Se bloqueado, mascara os dados numéricos de sensores
        timestamp_utc: acesso_bloqueado ? null : r.timestamp_utc,
        data_hora: acesso_bloqueado ? null : r.data_hora,
        temp_ref: acesso_bloqueado ? null : r.temp_ref,
        temp_dht: acesso_bloqueado ? null : r.temp_dht,
        temp_bmp: acesso_bloqueado ? null : r.temp_bmp,
        umidade: acesso_bloqueado ? null : r.umidade,
        umidade_solo: acesso_bloqueado ? null : r.umidade_solo,
        temp_solo: acesso_bloqueado ? null : r.temp_solo,
        vpd: acesso_bloqueado ? null : r.vpd,
        et0: acesso_bloqueado ? null : r.et0,
        etc: acesso_bloqueado ? null : r.etc,
        rs: acesso_bloqueado ? null : r.rs,
        par: acesso_bloqueado ? null : r.par,
        rf: acesso_bloqueado ? null : r.rf,
        rg: acesso_bloqueado ? null : r.rg,
        dp: acesso_bloqueado ? null : r.dp,
        at: acesso_bloqueado ? null : r.at,
        itu: acesso_bloqueado ? null : r.itu,
        hi: acesso_bloqueado ? null : r.hi,
        lux: acesso_bloqueado ? null : r.lux,
        pressao: acesso_bloqueado ? null : r.pressao,
        qualidade: acesso_bloqueado ? null : r.qualidade,
        heap_mod: acesso_bloqueado ? null : r.heap_mod,
        api_token: (isMaster || isOwner) ? r.api_token : null
      };
    });

    res.json(estacoes);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// Histórico de leituras (com suporte a 24h, 1d, 7d, 30d, 6m, 1y e all)
app.get('/api/telemetria/historico', obterUsuarioOpcional, async (req, res) => {
  const estacao = req.query.estacao || 'G00001';
  const periodo = (req.query.periodo || '24h').toLowerCase();
  const user = req.user;
  const isMaster = user && user.role === 'ADMIN';

  try {
    // 1. Verifica permissões da estação
    const estCheck = await pool.query('SELECT * FROM gaia_estacoes WHERE cod_estacao = $1', [estacao]);
    if (estCheck.rows.length > 0) {
      const st = estCheck.rows[0];
      if (!isMaster) {
        if (st.status_financeiro === 'PENDENTE') {
          return res.status(403).json({
            error: 'Acesso suspenso: Estação com mensalidade pendente.',
            motivo: 'MENSALIDADE_PENDENTE'
          });
        }
        if (st.visibilidade === 'PRIVADA' && (!user || user.id !== st.proprietario_id)) {
          return res.status(403).json({
            error: 'Acesso restrito: Esta estação é privada.',
            motivo: 'ESTACAO_PRIVADA'
          });
        }
      }
    }

    const nowUtc = Math.floor(Date.now() / 1000);
    let cutoffTs = 0;
    let maxLimit = 500;

    switch (periodo) {
      case '24h':
      case '1d':
        cutoffTs = nowUtc - (24 * 3600);
        maxLimit = 150;
        break;
      case '7d':
      case '1s':
      case 'semana':
        cutoffTs = nowUtc - (7 * 86400);
        maxLimit = 250;
        break;
      case '30d':
      case '1m':
      case 'mes':
        cutoffTs = nowUtc - (30 * 86400);
        maxLimit = 350;
        break;
      case '6m':
      case '6meses':
        cutoffTs = nowUtc - (180 * 86400);
        maxLimit = 500;
        break;
      case '1y':
      case '1ano':
      case 'ano':
        cutoffTs = nowUtc - (365 * 86400);
        maxLimit = 600;
        break;
      case 'all':
      case 'todos':
      default:
        cutoffTs = 0;
        maxLimit = 1000;
        break;
    }

    if (req.query.limite) {
      maxLimit = parseInt(req.query.limite, 10) || maxLimit;
    }

    let result;
    if (cutoffTs > 0) {
      result = await pool.query(
        `SELECT * FROM (
          SELECT * FROM gaia_telemetria 
          WHERE cod_estacao = $1 AND timestamp_utc >= $2
          ORDER BY timestamp_utc DESC 
          LIMIT $3
        ) sub ORDER BY timestamp_utc ASC;`,
        [estacao, cutoffTs, maxLimit]
      );
    } else {
      result = await pool.query(
        `SELECT * FROM (
          SELECT * FROM gaia_telemetria 
          WHERE cod_estacao = $1 
          ORDER BY timestamp_utc DESC 
          LIMIT $2
        ) sub ORDER BY timestamp_utc ASC;`,
        [estacao, maxLimit]
      );
    }

    res.json(result.rows);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// Atualizar dados de uma estação (apenas Master/Admin ou dono)
app.put('/api/estacoes/:cod', exigirAuth, async (req, res) => {
  const cod = req.params.cod;
  const { nome, cultura, cultura_id, latitude, longitude, visibilidade, status_financeiro, api_token } = req.body;
  const user = req.user;
  const isMaster = user.role === 'ADMIN';

  try {
    const estCheck = await pool.query('SELECT * FROM gaia_estacoes WHERE cod_estacao = $1', [cod]);
    if (estCheck.rows.length === 0) {
      return res.status(404).json({ error: 'Estação não encontrada.' });
    }

    const st = estCheck.rows[0];
    if (!isMaster && st.proprietario_id !== user.id) {
      return res.status(403).json({ error: 'Você não tem permissão para alterar esta estação.' });
    }

    // Se informou cultura_id, sincroniza o nome correspondente do catálogo
    let nomeCultura = cultura;
    let novoCulturaId = cultura_id !== undefined ? cultura_id : st.cultura_id;
    if (cultura_id) {
      const cRes = await pool.query('SELECT nome FROM gaia_culturas_perfil WHERE id = $1', [cultura_id]);
      if (cRes.rows.length > 0) {
        nomeCultura = cRes.rows[0].nome;
      }
    }

    // Apenas Master pode alterar o status financeiro
    const novoStatusFin = isMaster ? status_financeiro || st.status_financeiro : st.status_financeiro;

    const result = await pool.query(`
      UPDATE gaia_estacoes
      SET 
        nome = COALESCE($1, nome),
        cultura = COALESCE($2, cultura),
        cultura_id = COALESCE($3, cultura_id),
        latitude = COALESCE($4, latitude),
        longitude = COALESCE($5, longitude),
        visibilidade = COALESCE($6, visibilidade),
        status_financeiro = COALESCE($7, status_financeiro),
        api_token = COALESCE($8, api_token),
        atualizado_em = CURRENT_TIMESTAMP
      WHERE cod_estacao = $9
      RETURNING *;
    `, [nome, nomeCultura, novoCulturaId, latitude, longitude, visibilidade, novoStatusFin, api_token, cod]);

    broadcast('ESTACAO_ATUALIZADA', result.rows[0]);
    res.json(result.rows[0]);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});


server.listen(PORT, '0.0.0.0', () => {
  console.log(`Servidor GAIA com Autenticação e Regras de Estações ativo na porta ${PORT}`);
});
