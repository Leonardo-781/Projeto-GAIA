#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Script gerador dos documentos DOCX oficiais da UFU para o Projeto GAIA:
1. Anexo I - Proposta de Atividade de Pesquisa (DOCX)
2. Anexo IV - Relatório Técnico Circunstanciado (DOCX)
3. Documento Completo Unificado (DOCX)
"""

import os
from docx import Document
from docx.shared import Inches, Pt, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT, WD_ALIGN_VERTICAL
from docx.oxml import parse_xml, OxmlElement
from docx.oxml.ns import nsdecls, qn

# =============================================================================
# CORES INSTITUCIONAIS
# =============================================================================
COR_AZUL_ESC = "0F3460"    # Navy Blue
COR_CINZA_CLARO = "F1F5F9" # Light Background
COR_CINZA_BORDA = "CBD5E1" # Border
COR_TEXTO_PADRAO = RGBColor(34, 34, 34)
COR_AZUL_PADRAO = RGBColor(15, 52, 96)

def aplicar_shading(cell, cor_hex):
    shd = parse_xml(f'<w:shd {nsdecls("w")} w:fill="{cor_hex}"/>')
    cell._tc.get_or_add_tcPr().append(shd)

def aplicar_margens_celula(cell, top=100, bottom=100, left=150, right=150):
    tcMar = parse_xml(f'<w:tcMar {nsdecls("w")}><w:top w:w="{top}" w:type="dxa"/><w:bottom w:w="{bottom}" w:type="dxa"/><w:left w:w="{left}" w:type="dxa"/><w:right w:w="{right}" w:type="dxa"/></w:tcMar>')
    cell._tc.get_or_add_tcPr().append(tcMar)

def aplicar_borda_tabela(table, cor_hex="CBD5E1"):
    tblPr = table._tbl.tblPr
    borders = parse_xml(
        f'<w:tblBorders {nsdecls("w")}>'
        f'<w:top w:val="single" w:sz="4" w:space="0" w:color="{cor_hex}"/>'
        f'<w:left w:val="single" w:sz="4" w:space="0" w:color="{cor_hex}"/>'
        f'<w:bottom w:val="single" w:sz="4" w:space="0" w:color="{cor_hex}"/>'
        f'<w:right w:val="single" w:sz="4" w:space="0" w:color="{cor_hex}"/>'
        f'<w:insideH w:val="single" w:sz="4" w:space="0" w:color="{cor_hex}"/>'
        f'<w:insideV w:val="single" w:sz="4" w:space="0" w:color="{cor_hex}"/>'
        f'</w:tblBorders>'
    )
    tblPr.append(borders)

def formatar_paragrafo(p, espaco_antes=0, espaco_depois=4, line_spacing=1.15):
    p.paragraph_format.space_before = Pt(espaco_antes)
    p.paragraph_format.space_after = Pt(espaco_depois)
    p.paragraph_format.line_spacing = line_spacing

def adicionar_cabecalho_ufu(doc):
    p_cab = doc.add_paragraph()
    p_cab.alignment = WD_ALIGN_PARAGRAPH.CENTER
    formatar_paragrafo(p_cab, 0, 8, 1.1)
    
    r1 = p_cab.add_run("SERVIÇO PÚBLICO FEDERAL\nMINISTÉRIO DA EDUCAÇÃO\n")
    r1.font.name = "Arial"
    r1.font.size = Pt(8.5)
    r1.font.bold = True
    r1.font.color.rgb = COR_AZUL_PADRAO

    r2 = p_cab.add_run("UNIVERSIDADE FEDERAL DE UBERLÂNDIA\n")
    r2.font.name = "Arial"
    r2.font.size = Pt(10)
    r2.font.bold = True
    r2.font.color.rgb = COR_AZUL_PADRAO

    r3 = p_cab.add_run("PRÓ-REITORIA DE PESQUISA E PÓS-GRADUAÇÃO\nDIRETORIA DE PESQUISA / DIVISÃO DE FOMENTO A PROGRAMAS E PROJETOS")
    r3.font.name = "Arial"
    r3.font.size = Pt(8)
    r3.font.color.rgb = RGBColor(100, 100, 100)

    # Linha divisória horizontal
    p_hr = doc.add_paragraph()
    formatar_paragrafo(p_hr, 0, 10, 1.0)
    pBdr = parse_xml(f'<w:pBdr {nsdecls("w")}><w:bottom w:val="single" w:sz="12" w:space="1" w:color="{COR_AZUL_ESC}"/></w:pBdr>')
    p_hr._p.get_or_add_pPr().append(pBdr)

def adicionar_faixa_secao(doc, titulo):
    table = doc.add_table(rows=1, cols=1)
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    table.autofit = False
    cell = table.cell(0, 0)
    cell.width = Inches(7.0)
    aplicar_shading(cell, COR_AZUL_ESC)
    aplicar_margens_celula(cell, top=80, bottom=80, left=120, right=120)
    
    p = cell.paragraphs[0]
    p.alignment = WD_ALIGN_PARAGRAPH.LEFT
    formatar_paragrafo(p, 0, 0, 1.0)
    run = p.add_run(titulo)
    run.font.name = "Arial"
    run.font.size = Pt(9.5)
    run.font.bold = True
    run.font.color.rgb = RGBColor(255, 255, 255)

    p_spacer = doc.add_paragraph()
    formatar_paragrafo(p_spacer, 0, 2, 1.0)

def adicionar_campo(doc, titulo_campo, texto_conteudo):
    table = doc.add_table(rows=2, cols=1)
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    table.autofit = False
    
    # Linha 1: Título do campo
    c_tit = table.cell(0, 0)
    c_tit.width = Inches(7.0)
    aplicar_shading(c_tit, "E2E8F0")
    aplicar_margens_celula(c_tit, top=60, bottom=60, left=100, right=100)
    p_tit = c_tit.paragraphs[0]
    formatar_paragrafo(p_tit, 0, 0, 1.0)
    r_tit = p_tit.add_run(titulo_campo)
    r_tit.font.name = "Arial"
    r_tit.font.size = Pt(9)
    r_tit.font.bold = True
    r_tit.font.color.rgb = COR_AZUL_PADRAO

    # Linha 2: Conteúdo
    c_cnt = table.cell(1, 0)
    c_cnt.width = Inches(7.0)
    aplicar_shading(c_cnt, COR_CINZA_CLARO)
    aplicar_margens_celula(c_cnt, top=80, bottom=80, left=100, right=100)
    p_cnt = c_cnt.paragraphs[0]
    p_cnt.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY
    formatar_paragrafo(p_cnt, 0, 0, 1.15)
    r_cnt = p_cnt.add_run(texto_conteudo)
    r_cnt.font.name = "Arial"
    r_cnt.font.size = Pt(9)
    r_cnt.font.color.rgb = COR_TEXTO_PADRAO

    aplicar_borda_tabela(table, COR_CINZA_BORDA)
    p_spacer = doc.add_paragraph()
    formatar_paragrafo(p_spacer, 0, 4, 1.0)

# =============================================================================
# CONSTRUÇÃO DO ANEXO I (DOCX)
# =============================================================================
def construir_docx_anexo_i(doc):
    adicionar_cabecalho_ufu(doc)

    p_tit = doc.add_paragraph()
    p_tit.alignment = WD_ALIGN_PARAGRAPH.CENTER
    formatar_paragrafo(p_tit, 4, 12, 1.1)
    r_tit = p_tit.add_run("ANEXO I – PROPOSTA DE ATIVIDADE DE PESQUISA")
    r_tit.font.name = "Arial"
    r_tit.font.size = Pt(13)
    r_tit.font.bold = True
    r_tit.font.color.rgb = RGBColor(0, 0, 0)

    # 1. Informações Institucionais
    adicionar_faixa_secao(doc, "1. INFORMAÇÕES INSTITUCIONAIS DO(A) COORDENADOR(A)")
    t1 = doc.add_table(rows=2, cols=2)
    t1.alignment = WD_TABLE_ALIGNMENT.CENTER
    t1.autofit = False
    
    dados_t1 = [
        ("1.1. Nome:", "[Preencher Nome do(a) Orientador(a)]", "1.2. E-mail:", "[Preencher E-mail]"),
        ("1.3. Unidade Acadêmica:", "Faculdade de Engenharia Elétrica / Computação / Agronomia (UFU)", "1.4. Campus:", "Santa Mônica / Umuarama / Glória")
    ]
    for row_idx, (rot1, val1, rot2, val2) in enumerate(dados_t1):
        c1, c2 = t1.cell(row_idx, 0), t1.cell(row_idx, 1)
        c1.width, c2.width = Inches(3.5), Inches(3.5)
        for cell, rot, val in [(c1, rot1, val1), (c2, rot2, val2)]:
            aplicar_margens_celula(cell, 60, 60, 80, 80)
            aplicar_shading(cell, "FAFAFA")
            p = cell.paragraphs[0]
            formatar_paragrafo(p, 0, 0, 1.1)
            r_rot = p.add_run(rot + " ")
            r_rot.font.name = "Arial"
            r_rot.font.size = Pt(8.5)
            r_rot.font.bold = True
            r_rot.font.color.rgb = COR_AZUL_PADRAO
            
            r_val = p.add_run(val)
            r_val.font.name = "Arial"
            r_val.font.size = Pt(8.5)
            r_val.font.color.rgb = COR_TEXTO_PADRAO
            
    aplicar_borda_tabela(t1, COR_CINZA_BORDA)
    p_sp = doc.add_paragraph()
    formatar_paragrafo(p_sp, 0, 6, 1.0)

    # 2. Descrição do Projeto
    adicionar_faixa_secao(doc, "2. DESCRIÇÃO DO PROJETO DE PESQUISA")
    t2 = doc.add_table(rows=8, cols=1)
    t2.alignment = WD_TABLE_ALIGNMENT.CENTER
    t2.autofit = False
    
    dados_t2 = [
        ("2.1. Código do Edital:", "DIRPE XX/20XX   ( X ) PIBITI-CNPq   ( X ) PIBIC-CNPq   ( ) PIVIC"),
        ("2.2. Título do Projeto:", "Desenvolvimento de Estação Agrometeorológica de Borda de Baixo Custo com Telemetria Conectada e Diagnóstico Fitossanitário em Tempo Real (Projeto GAIA)"),
        ("2.3. Palavras-Chave:", "Agrometeorologia. Internet Das Coisas. Agricultura De Precisão."),
        ("2.4. Área do Conhecimento:", "( X ) Engenharias      ( X ) Ciências Agrárias      ( ) Ciências Exatas e da Terra"),
        ("2.5. Grupo de Pesquisa CNPq:", "Automação, Sistemas Embarcados e Tecnologias para o Agronegócio (UFU)"),
        ("2.6. Arranjo Institucional:", "Universidade Federal de Uberlândia (UFU) - Laboratório de Sistemas Embarcados / Automação"),
        ("2.7. Protocolo CEP/CEUA:", "Não se aplica (Pesquisa em hardware embarcado, sensores ambientais e firmware de borda)"),
        ("2.8. Linha de Pesquisa:", "Agricultura 4.0, Sensoriamento Inteligente e Cidades/Campos Conectados")
    ]
    for row_idx, (rot, val) in enumerate(dados_t2):
        cell = t2.cell(row_idx, 0)
        cell.width = Inches(7.0)
        aplicar_margens_celula(cell, 50, 50, 80, 80)
        aplicar_shading(cell, "FAFAFA" if row_idx % 2 == 0 else "FFFFFF")
        p = cell.paragraphs[0]
        formatar_paragrafo(p, 0, 0, 1.1)
        r_rot = p.add_run(rot + " ")
        r_rot.font.name = "Arial"
        r_rot.font.size = Pt(8.5)
        r_rot.font.bold = True
        r_rot.font.color.rgb = COR_AZUL_PADRAO
        
        r_val = p.add_run(val)
        r_val.font.name = "Arial"
        r_val.font.size = Pt(8.5)
        r_val.font.color.rgb = COR_TEXTO_PADRAO

    aplicar_borda_tabela(t2, COR_CINZA_BORDA)
    p_sp = doc.add_paragraph()
    formatar_paragrafo(p_sp, 0, 6, 1.0)

    # 3. Viés Tecnológico
    adicionar_faixa_secao(doc, "3. VIÉS TECNOLÓGICO (PIBITI - até 500 palavras)")
    txt_vies = (
        "O projeto apresenta inequívoco caráter de inovação tecnológica ao conceber uma estação meteorológica inteligente "
        "de baixo custo baseada no microcontrolador ESP32 de 32 bits, com capacidade nativa de computação na borda (Edge Computing). "
        "Ao contrário das estações convencionais de mercado, que atuam meramente como registradores passivos (dataloggers) e exigem "
        "servidores pagos na nuvem para processamento, o Projeto GAIA executa cálculos agrometeorológicos de alta complexidade diretamente "
        "no microcontrolador: Déficit de Pressão de Vapor (VPD) com diagnóstico estomático, Evapotranspiração de Referência (Penman-Monteith FAO-56), "
        "Radiação Fotossinteticamente Ativa (PAR/PPFD), estimativa de radiação global (Rs), Índice de Conforto Zootécnico (ITU/THI) e classificação "
        "de risco para geadas (branca e negra).\n\n"
        "O avanço tecnológico reside também na arquitetura de resiliência: validação temporal via Relógio Atômico brasileiro (NTP.br) dispensando "
        "módulos RTC mecânicos frágeis; encriptação nativa por hardware AES-128-CBC; preservação de séries temporais na memória estática de baixo "
        "consumo RTC RAM (RTC_DATA_ATTR) durante ciclos de repouso profundo (Deep Sleep); persistência em fila offline no cartão SD (Store-and-Forward); "
        "e portal web captivo (SoftAP) com diagnóstico em tempo real no celular sem requerer aplicativos externos.\n\n"
        "Nível de Maturidade Tecnológica: O projeto encontra-se em TRL 5 (protótipo funcional integrado com todos os sensores validados em "
        "ambiente simulado e bancada operacional), com previsão de alcançar TRL 6/7 com os ensaios de campo continuados propostos neste plano."
    )
    adicionar_campo(doc, "3.1. Inovação Tecnológica, Ganhos de Qualidade/Eficiência e Escala TRL", txt_vies)

    # 4. Detalhamento da Proposta
    adicionar_faixa_secao(doc, "4. DETALHAMENTO DA PROPOSTA")
    
    txt_resumo = (
        "O monitoramento microclimático preciso é fundamental para a agricultura de precisão e para a sustentabilidade hídrica. "
        "Contudo, estações comerciais possuem custo proibitivo para pequenos e médios produtores, além de demandarem manutenção especializada. "
        "Este projeto propõe o desenvolvimento e aprimoramento da estação agrometeorológica de campo 'Projeto GAIA', um sistema embarcado de baixo "
        "custo baseado no ESP32. O sistema integra sensores de temperatura e umidade do ar (DHT22), pressão barométrica (BMP280), radiação solar/iluminância "
        "(VEML7700), temperatura do solo em nível radicular (DS18B20) e umidade volumétrica do solo.\n"
        "Os dados são validados temporalmente com o relógio atômico nacional e processados na própria estação, gerando indicadores vitais: ponto de orvalho, "
        "VPD estomático, evapotranspiração da cultura (ETc), estimativa de radiação fotossintética, horas de molhamento foliar para alerta fúngico, risco de geada "
        "(branca e negra) e índice de conforto térmico animal (ITU). A transmissão é realizada via Wi-Fi/HTTP com criptografia AES-128 e redundância em cartão SD. "
        "Os resultados visam entregar uma solução acessível, robusta e autônoma, democratizando dados micrometeorológicos aplicados ao manejo racional de irrigação e fitossanidade."
    )
    adicionar_campo(doc, "4.1. Resumo da Proposta (até 300 palavras)", txt_resumo)

    txt_problema = (
        "Problema: A tomada de decisão agrícola no Brasil (irrigação, pulverização e prevenção fitossanitária) ainda é realizada predominantemente "
        "com base em previsões meteorológicas genéricas de escala municipal, as quais desconsideram o microclima específico da lavoura. Estações "
        "agrometeorológicas profissionais de alta precisão possuem custo elevado (frequentemente superior a R$ 15.000,00), exigem conectividade de "
        "dados ininterrupta e utilizam protocolos fechados.\n"
        "Pergunta e Hipótese: É tecnicamente viável desenvolver uma estação agrometeorológica modular, alimentada a bateria/solar, com custo inferior "
        "a 10% das opções comerciais, capaz de computar métricas micrometeorológicas complexas diretamente na borda (Edge Computing) e manter a "
        "operação autônoma sem perda de dados mesmo sob intermitência de sinal de rede?"
    )
    adicionar_campo(doc, "4.2. Problema Abordado e Pergunta/Hipótese Principal (até 250 palavras)", txt_problema)

    txt_ref = (
        "1. ALLEN, R. G. et al. Crop evapotranspiration: Guidelines for computing crop water requirements. FAO Irrigation and drainage paper 56, Rome, 1998. "
        "[Estabelece o padrão biofísico Penman-Monteith adotado diretamente no código para cálculo da demanda evapotranspirativa de referência].\n"
        "2. ALVARES, C. A. et al. Köppen's climate classification map for Brazil. Meteorologische Zeitschrift, v. 22, n. 6, 2013. "
        "[Fundamenta a distribuição de extremos climáticos e a importância da medição local de gradientes térmicos e riscos de geada].\n"
        "3. OJHA, T. et al. Wireless sensor networks for agriculture: The state-of-the-art in practice and future challenges. Computers and Electronics in Agriculture, 2015. "
        "[Referência de arquitetura para redes de sensores sem fio agrícolas, nós autônomos de baixo consumo e tolerância a falhas temporais]."
    )
    adicionar_campo(doc, "4.3. Principais Referências Bibliográficas (até 250 palavras)", txt_ref)

    txt_metodo = (
        "A metodologia adotada é experimental e de desenvolvimento tecnológico aplicado, dividida em quatro fases:\n"
        "1. Arquitetura de Hardware e Sensoriamento: Integração dos sensores DHT22 (ar), BMP280 (pressão I2C), VEML7700 (luz solar I2C), "
        "DS18B20 (sonda de solo 1-Wire em GPIO 13) e sensor de umidade de solo analógico (ADC 34) ao microcontrolador ESP32 dual-core de 240 MHz, "
        "com reguladores de baixíssimo ruído e barramento I2C protegido com timeout.\n"
        "2. Motor Matemático de Borda (Edge Computing): Codificação de funções otimizadas em ponto flutuante para Ponto de Orvalho (Magnus-Tetens), "
        "Déficit de Pressão de Vapor (VPD estomático), Evapotranspiração horária e diária Penman-Monteith (FAO-56), Radiação Global (Rs em W/m²) "
        "e PAR/PPFD (µmol/m²·s). Implementação de buffer circular na memória estática RTC RAM (RTC_DATA_ATTR) para rastreamento de pressão "
        "barométrica em janela móvel de 3 horas (alerta de tempestades) e acúmulo contínuo de horas de folha molhada sem desgaste de flash.\n"
        "3. Conectividade Segura e Usabilidade: Validação temporal precisa via Relógio Atômico nacional (NTP.br: a.st1.ntp.br); cifragem de "
        "payload em hardware com AES-128-CBC; mecanismo Store-and-Forward no cartão SD para garantia de entrega; e portal captivo Wi-Fi com "
        "DNS integrado na porta 53 para auditoria em tempo real dos sensores em campo pelo celular.\n"
        "4. Testes e Calibração: Ensaios comparativos de bancada e campo para avaliação de desvio entre sensores redundantes (DHT vs BMP), "
        "teste de estresse térmico, medição de corrente em repouso profundo e validação da integridade de pacotes transmitidos."
    )
    adicionar_campo(doc, "4.4. Método para Obtenção da Resposta ao Problema (até 500 palavras)", txt_metodo)

    # 4.5. Tabela de Metas
    t_metas = doc.add_table(rows=5, cols=4)
    t_metas.alignment = WD_TABLE_ALIGNMENT.CENTER
    t_metas.autofit = False
    
    cab_m = ["META", "DESCRIÇÃO DETALHADA", "QUANTIDADE", "PREVISÃO"]
    larguras_m = [Inches(0.6), Inches(4.2), Inches(1.0), Inches(1.2)]
    
    for i, h in enumerate(cab_m):
        cell = t_metas.cell(0, i)
        cell.width = larguras_m[i]
        aplicar_shading(cell, COR_AZUL_ESC)
        aplicar_margens_celula(cell, 80, 80, 80, 80)
        p = cell.paragraphs[0]
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        formatar_paragrafo(p, 0, 0, 1.0)
        r = p.add_run(h)
        r.font.name = "Arial"
        r.font.size = Pt(8.5)
        r.font.bold = True
        r.font.color.rgb = RGBColor(255, 255, 255)

    dados_m = [
        ("1", "Integração física do circuito embarcado, proteções I2C/1-Wire e testes dos sensores ambientais e de solo", "1 protótipo", "Mês 03"),
        ("2", "Desenvolvimento dos algoritmos de borda (VPD, ET0 FAO-56, PAR, ITU e variáveis em RTC RAM)", "1 firmware", "Mês 06"),
        ("3", "Implementação da telemetria Wi-Fi segura (AES-128), NTP atômico, fila SD e Portal Captivo SoftAP", "1 sistema", "Mês 08"),
        ("4", "Ensaios de campo contínuos, análise de consistência agronômica e elaboração de relatório final/artigo", "1 artigo", "Mês 12"),
    ]
    for row_idx, linha in enumerate(dados_m, start=1):
        for col_idx, val in enumerate(linha):
            cell = t_metas.cell(row_idx, col_idx)
            cell.width = larguras_m[col_idx]
            aplicar_shading(cell, "FAFAFA" if row_idx % 2 == 0 else "FFFFFF")
            aplicar_margens_celula(cell, 60, 60, 80, 80)
            p = cell.paragraphs[0]
            p.alignment = WD_ALIGN_PARAGRAPH.CENTER if col_idx != 1 else WD_ALIGN_PARAGRAPH.LEFT
            formatar_paragrafo(p, 0, 0, 1.1)
            r = p.add_run(val)
            r.font.name = "Arial"
            r.font.size = Pt(8.5)
            r.font.color.rgb = COR_TEXTO_PADRAO

    aplicar_borda_tabela(t_metas, COR_CINZA_BORDA)
    p_sp = doc.add_paragraph()
    formatar_paragrafo(p_sp, 0, 6, 1.0)

    txt_imp = (
        "A relevância desta pesquisa para a Universidade Federal de Uberlândia (UFU) consolida-se em múltiplos eixos:\n"
        "• Impacto Científico e Tecnológico: Criação de propriedade intelectual e consolidação de conhecimento prático em Computação "
        "na Borda aplicada à Agrometeorologia, estabelecendo uma base de código aberta e replicável para projetos futuros de graduação "
        "e pós-graduação na UFU.\n"
        "• Impacto Ambiental e Hídrico: O monitoramento local de VPD e Evapotranspiração permite irrigar exatamente o volume requerido "
        "pela cultura no estádio fenológico atual, combatendo o desperdício de água e reduzindo o consumo energético de bombas.\n"
        "• Impacto Social e Econômico: Oferta de uma alternativa viável e acessível para produtores rurais familiares, cooperativas "
        "e pequenas propriedades do Triângulo Mineiro e Alto Paranaíba que não têm acesso a estações agrometeorológicas comerciais.\n"
        "• Impacto Acadêmico na UFU: Formação avançada do estudante bolsista em internet das coisas, programação de sistemas de tempo real, "
        "sensoriamento microclimático e segurança de dados, gerando produção acadêmica qualificada para publicação institucional."
    )
    adicionar_campo(doc, "4.6. Relevância da Proposta e Impactos Esperados na UFU (até 500 palavras)", txt_imp)

    txt_ods = (
        "[ X ] 2. Fome zero e agricultura sustentável       [ X ] 6. Água limpa e saneamento\n"
        "[ X ] 9. Indústria, inovação e infraestrutura        [ X ] 12. Consumo e produção responsáveis\n"
        "[ X ] 13. Ação contra mudança global do clima       [ X ] 15. Vida terrestre"
    )
    adicionar_campo(doc, "4.7. Relação com os Objetivos de Desenvolvimento Sustentável (ODS)", txt_ods)

    # 5. Recursos Financeiros
    adicionar_faixa_secao(doc, "5. RECURSOS FINANCEIROS PREVISTOS")
    t_rec = doc.add_table(rows=5, cols=4)
    t_rec.alignment = WD_TABLE_ALIGNMENT.CENTER
    t_rec.autofit = False
    
    cab_r = ["DESPESAS", "DESCRIÇÃO DETALHADA", "QUANTIDADE", "VALOR TOTAL"]
    larg_r = [Inches(1.4), Inches(3.6), Inches(1.0), Inches(1.0)]
    for i, h in enumerate(cab_r):
        cell = t_rec.cell(0, i)
        cell.width = larg_r[i]
        aplicar_shading(cell, COR_AZUL_ESC)
        aplicar_margens_celula(cell, 80, 80, 80, 80)
        p = cell.paragraphs[0]
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        formatar_paragrafo(p, 0, 0, 1.0)
        r = p.add_run(h)
        r.font.name = "Arial"
        r.font.size = Pt(8.5)
        r.font.bold = True
        r.font.color.rgb = RGBColor(255, 255, 255)

    dados_r = [
        ("Bolsa", "Bolsa de Iniciação Científica/Tecnológica (PIBITI/PIBIC)", "12 meses", "Conforme edital"),
        ("Material de Consumo", "Sensores (DHT22, BMP280, VEML7700, DS18B20), ESP32, cabos, cartões SD, caixas IP65", "Itens bancada", "Recursos Lab"),
        ("Material Permanente", "Computador e instrumentos de medição (osciloscópio/multímetro) do laboratório", "Existente", "R$ 0,00"),
        ("Serviços Terceiros", "Não previstos", "-", "R$ 0,00"),
    ]
    for row_idx, linha in enumerate(dados_r, start=1):
        for col_idx, val in enumerate(linha):
            cell = t_rec.cell(row_idx, col_idx)
            cell.width = larg_r[col_idx]
            aplicar_shading(cell, "FAFAFA" if row_idx % 2 == 0 else "FFFFFF")
            aplicar_margens_celula(cell, 60, 60, 80, 80)
            p = cell.paragraphs[0]
            p.alignment = WD_ALIGN_PARAGRAPH.CENTER if col_idx >= 2 else WD_ALIGN_PARAGRAPH.LEFT
            formatar_paragrafo(p, 0, 0, 1.1)
            r = p.add_run(val)
            r.font.name = "Arial"
            r.font.size = Pt(8.5)
            r.font.color.rgb = COR_TEXTO_PADRAO

    aplicar_borda_tabela(t_rec, COR_CINZA_BORDA)
    p_sp = doc.add_paragraph()
    formatar_paragrafo(p_sp, 0, 6, 1.0)

    # 6. Divulgação
    adicionar_faixa_secao(doc, "6. PLANO DE DIVULGAÇÃO CIENTÍFICA (até 250 palavras)")
    txt_div = (
        "A divulgação do conhecimento gerado no projeto seguirá plano estruturado em três níveis:\n"
        "1. Comunidade Acadêmica Local: Apresentação obrigatória no Encontro de Iniciação Científica e Tecnológica da UFU, "
        "com elaboração de resumo expandido e apresentação oral/pôster técnico.\n"
        "2. Comunidade Científica Externa: Elaboração e submissão de artigo técnico-científico em congresso nacional de automação "
        "ou engenharia agrícola (ex: Congresso Brasileiro de Engenharia Agrícola - CONBEA ou Simpósio Brasileiro de Automação Inteligente - SBAI).\n"
        "3. Sociedade e Produtores: Disponibilização da documentação técnica e código-fonte em repositório público de acesso aberto (GitHub), "
        "além de confecção de manual de montagem ilustrado para incentivo ao uso da tecnologia por escolas técnicas e produtores familiares."
    )
    adicionar_campo(doc, "6.1. Formas de Divulgação Científica", txt_div)


# =============================================================================
# CONSTRUÇÃO DO ANEXO IV (DOCX)
# =============================================================================
def construir_docx_anexo_iv(doc):
    adicionar_cabecalho_ufu(doc)

    p_tit = doc.add_paragraph()
    p_tit.alignment = WD_ALIGN_PARAGRAPH.CENTER
    formatar_paragrafo(p_tit, 4, 4, 1.1)
    r_tit = p_tit.add_run("ANEXO IV – RELATÓRIO TÉCNICO CIRCUNSTANCIADO")
    r_tit.font.name = "Arial"
    r_tit.font.size = Pt(13)
    r_tit.font.bold = True
    r_tit.font.color.rgb = RGBColor(0, 0, 0)

    p_sub = doc.add_paragraph()
    p_sub.alignment = WD_ALIGN_PARAGRAPH.CENTER
    formatar_paragrafo(p_sub, 0, 10, 1.0)
    r_sub = p_sub.add_run("( X ) Final       ( ) Cancelamento")
    r_sub.font.name = "Arial"
    r_sub.font.size = Pt(9.5)
    r_sub.font.bold = True
    r_sub.font.color.rgb = COR_AZUL_PADRAO

    # 1. Informações Institucionais
    adicionar_faixa_secao(doc, "1. INFORMAÇÕES INSTITUCIONAIS")
    t1 = doc.add_table(rows=6, cols=2)
    t1.alignment = WD_TABLE_ALIGNMENT.CENTER
    t1.autofit = False
    
    dados_t1 = [
        ("1.1. Orientador(a):", "[Preencher Nome do(a) Orientador(a)]", "1.2. E-mail:", "[Preencher E-mail]"),
        ("1.3. Unidade Acadêmica:", "Faculdade de Engenharia Elétrica / Computação / Agronomia (UFU)", "1.4. Campus:", "Santa Mônica / Umuarama / Glória"),
        ("1.5. Bolsista:", "Leonardo [Sobrenome]", "1.6. E-mail:", "[Preencher E-mail]"),
        ("1.7. Curso:", "Engenharia / Ciência da Computação / Agronomia", "1.8. Campus:", "Santa Mônica"),
        ("1.9. Título da Pesquisa:", "Desenvolvimento de Estação Agrometeorológica de Borda de Baixo Custo com Telemetria Conectada e Diagnóstico Fitossanitário em Tempo Real (Projeto GAIA)", "1.10. Link Vídeo Pitch:", "[Inserir Link]"),
        ("1.11. Edital:", "DIRPE XX/20XX   ( X ) PIBITI-CNPq   ( X ) PIBIC-CNPq", "1.12. Vigência:", "MÊS/20XX A MÊS/20XX")
    ]
    for row_idx, (rot1, val1, rot2, val2) in enumerate(dados_t1):
        c1, c2 = t1.cell(row_idx, 0), t1.cell(row_idx, 1)
        c1.width, c2.width = Inches(3.5), Inches(3.5)
        for cell, rot, val in [(c1, rot1, val1), (c2, rot2, val2)]:
            aplicar_margens_celula(cell, 50, 50, 80, 80)
            aplicar_shading(cell, "FAFAFA" if row_idx % 2 == 0 else "FFFFFF")
            p = cell.paragraphs[0]
            formatar_paragrafo(p, 0, 0, 1.1)
            r_rot = p.add_run(rot + " ")
            r_rot.font.name = "Arial"
            r_rot.font.size = Pt(8.5)
            r_rot.font.bold = True
            r_rot.font.color.rgb = COR_AZUL_PADRAO
            
            r_val = p.add_run(val)
            r_val.font.name = "Arial"
            r_val.font.size = Pt(8.5)
            r_val.font.color.rgb = COR_TEXTO_PADRAO

    aplicar_borda_tabela(t1, COR_CINZA_BORDA)
    p_sp = doc.add_paragraph()
    formatar_paragrafo(p_sp, 0, 6, 1.0)

    # 2. Síntese da Execução
    adicionar_faixa_secao(doc, "2. SÍNTESE DA EXECUÇÃO DO PROJETO (até 500 palavras)")
    txt_sintese = (
        "O projeto cumpriu integralmente o plano de atividades previsto, entregando um sistema embarcado funcional, "
        "robusto e calibrado. Foram integrados com êxito os transdutores de temperatura e umidade relativa do ar (DHT22), "
        "pressão barométrica e temperatura secundária (BMP280), radiação solar/iluminância (VEML7700), temperatura do solo "
        "em profundidade radicular (DS18B20 digital 1-Wire em GPIO 13) e sonda de umidade volumétrica de solo (ADC 34) ao microcontrolador ESP32.\n\n"
        "Reformulações e Decisões Técnicas de Destaque:\n"
        "1. Eliminação do RTC Físico e Adoção do Tempo Atômico: Constatou-se durante os testes que módulos RTC convencionais (DS3231) "
        "apresentavam desvios cumulativos e dependiam de bateria tipo moeda propensa a falhas em campo sob altas temperaturas. "
        "Reformulou-se o projeto para validar o timestamp oficial via Relógio Atômico brasileiro (protocolo NTP: a.st1.ntp.br), mantido com "
        "precisão de microssegundos pelo oscilador interno do ESP32.\n"
        "2. Comunicação Direta Wi-Fi/Internet com Criptografia AES-128: A dependência de transceptores LoRa locais foi substituída "
        "por conectividade Wi-Fi nativa de longo alcance com encriptação simétrica AES-128-CBC e padding PKCS#7 diretamente por hardware (mbedTLS), "
        "permitindo envio de dados para servidores REST em nuvem com total sigilo de dados.\n"
        "3. Variáveis Históricas na Memória RTC RAM (Deep Sleep): Para acompanhar a tendência barométrica das últimas 3 horas (detecção "
        "de tempestades) e o contador cumulativo de horas de folha molhada sem desgastar as células de memória Flash com regravações repetidas, "
        "utilizou-se a memória estática rápida RTC RAM (RTC_DATA_ATTR), que mantém dados intactos durante o repouso profundo.\n"
        "4. Portal Captivo SoftAP: Desenvolveu-se um portal web embarcado com servidor DNS próprio (192.168.4.1), permitindo ao "
        "produtor rural auditar o funcionamento dos sensores em tempo real na tela do celular e configurar parâmetros de rede sem qualquer cabo USB."
    )
    adicionar_campo(doc, "2.1. Relato das Atividades, Aspectos Positivos, Problemas e Reformulações", txt_sintese)

    # 3. Recursos Obtidos
    adicionar_faixa_secao(doc, "3. RECURSOS EFETIVAMENTE OBTIDOS (até 250 palavras)")
    txt_rec = (
        "O projeto foi executado com o apoio da bolsa de Iniciação Científica/Tecnológica concedida ao estudante. "
        "Os insumos materiais (módulos eletrônicos, sensores, protoboards, ferramentas de soldagem e cabos) foram viabilizados "
        "pela estrutura existente dos laboratórios da UFU e por recursos próprios dos pesquisadores envolvidos, sem necessidade "
        "de captação de recursos financeiros adicionais."
    )
    adicionar_campo(doc, "3.1. Recursos Financeiros e Infraestrutura Utilizada", txt_rec)

    # 4. Levantamento de Resultados
    adicionar_faixa_secao(doc, "4. LEVANTAMENTO DE RESULTADOS / METAS / AÇÕES ALCANÇADAS")
    txt_res = (
        "Sim, o problema da pesquisa foi plenamente resolvido. O protótipo demonstrou capacidade de computar indicadores "
        "agronômicos de precisão diretamente no hardware de borda, sem sobrecarregar a memória nem comprometer o consumo de energia.\n"
        "Principais Contribuições Atingidas:\n"
        "• Autonomia de Diagnóstico: O ESP32 entrega índices prontos para tomada de decisão (VPD estomático ideal entre 0.4 e 1.4 kPa, "
        "necessidade de irrigação via ET0 e ETc, radiação fotossintética PAR e estresse térmico animal via ITU de Thom).\n"
        "• Segurança Fitossanitária: Detecção precoce de condições de condensação na folha (|Tar - Tpo| <= 1.5°C e RH >= 85%) "
        "com emissão de alertas automáticos para esporulação fúngica e classificação de risco de geadas branca e negra.\n"
        "• Confiabilidade dos Dados: Validação cruzada com sensor duplo de temperatura e cálculo de escore de integridade (q) "
        "no payload JSON, prevenindo que dados corrompidos alimentem sistemas de automação de irrigação.\n"
        "• Resiliência Offline: Garantia de retenção e reenvio de leituras via cartão SD (Store-and-Forward) sob quedas temporárias de sinal."
    )
    adicionar_campo(doc, "4.1. Resolução do Problema e Contribuições do Trabalho (até 500 palavras)", txt_res)

    # 4.2. Tabela Metas vs Resultados
    t_res = doc.add_table(rows=5, cols=6)
    t_res.alignment = WD_TABLE_ALIGNMENT.CENTER
    t_res.autofit = False
    
    cab_r = ["META", "DESCRIÇÃO DETALHADA", "QUANT.", "ALCANÇADO", "DATA", "OBSERVAÇÕES"]
    larg_r = [Inches(0.5), Inches(2.9), Inches(0.6), Inches(0.8), Inches(0.8), Inches(1.4)]
    for i, h in enumerate(cab_r):
        cell = t_res.cell(0, i)
        cell.width = larg_r[i]
        aplicar_shading(cell, COR_AZUL_ESC)
        aplicar_margens_celula(cell, 80, 80, 60, 60)
        p = cell.paragraphs[0]
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        formatar_paragrafo(p, 0, 0, 1.0)
        r = p.add_run(h)
        r.font.name = "Arial"
        r.font.size = Pt(8)
        r.font.bold = True
        r.font.color.rgb = RGBColor(255, 255, 255)

    dados_res = [
        ("1", "Montagem e integração física do circuito embarcado com 5 sensores ambientais/solo", "1", "Sim", "Mês 03", "Operação I2C e 1-Wire estável"),
        ("2", "Implementação do motor matemático na borda (VPD, ET0 FAO-56, PAR, ITU e RTC RAM)", "1", "Sim", "Mês 06", "Tempo cálculo < 2ms na FPU"),
        ("3", "Telemetria Wi-Fi segura (AES-128), NTP atômico, fila no SD e Portal Captivo Web", "1", "Sim", "Mês 08", "Configuração móvel ativa"),
        ("4", "Validação continuada em bancada/campo simulado e documentação técnica", "1", "Sim", "Mês 12", "Código aberto e relatório"),
    ]
    for row_idx, linha in enumerate(dados_res, start=1):
        for col_idx, val in enumerate(linha):
            cell = t_res.cell(row_idx, col_idx)
            cell.width = larg_r[col_idx]
            aplicar_shading(cell, "FAFAFA" if row_idx % 2 == 0 else "FFFFFF")
            aplicar_margens_celula(cell, 60, 60, 60, 60)
            p = cell.paragraphs[0]
            p.alignment = WD_ALIGN_PARAGRAPH.CENTER if col_idx not in (1, 5) else WD_ALIGN_PARAGRAPH.LEFT
            formatar_paragrafo(p, 0, 0, 1.1)
            r = p.add_run(val)
            r.font.name = "Arial"
            r.font.size = Pt(8)
            r.font.color.rgb = COR_TEXTO_PADRAO

    aplicar_borda_tabela(t_res, COR_CINZA_BORDA)
    p_sp = doc.add_paragraph()
    formatar_paragrafo(p_sp, 0, 6, 1.0)

    txt_fut = (
        "Como continuidade e desdobramentos futuros para a linha de pesquisa na UFU, identificam-se:\n"
        "1. Atualização Remota de Firmware (FOTA): Estruturação de mecanismo seguro Over-The-Air para entrega de atualizações em lote "
        "nas estações de campo sem necessidade de intervenção física nos mastros meteorológicos.\n"
        "2. Modelagem Fenológica Avançada: Implementação de contadores térmicos de Graus-Dia de Desenvolvimento (GDD) e cálculo automático "
        "do Delta T Agronômico (janela ideal para pulverização de defensivos sem risco de deriva por evaporação).\n"
        "3. Módulo Solar Autônomo: Integração de painel fotovoltaico de 10W com circuito MPPT e baterias LiFePO4 para garantia de autonomia "
        "contínua em campo aberto por múltiplos anos.\n"
        "4. Intercomparação com Estações Oficiais: Instalação de nó piloto próximo à estação meteorológica do INMET/UFU para análise "
        "estatística formal de correlação e validação metrológica dos sensores de baixo custo."
    )
    adicionar_campo(doc, "4.3. Possibilidades de Trabalho Futuro Decorrentes da Pesquisa (até 500 palavras)", txt_fut)

    # 5. Inventário dos Desdobramentos
    adicionar_faixa_secao(doc, "5. INVENTÁRIO DETALHADO DOS DESDOBRAMENTOS DA PESQUISA")
    t_inv = doc.add_table(rows=6, cols=3)
    t_inv.alignment = WD_TABLE_ALIGNMENT.CENTER
    t_inv.autofit = False
    
    cab_i = ["DESCRIÇÃO", "ESPECIFICAÇÃO", "QUANTIDADE"]
    larg_i = [Inches(2.2), Inches(4.0), Inches(0.8)]
    for i, h in enumerate(cab_i):
        cell = t_inv.cell(0, i)
        cell.width = larg_i[i]
        aplicar_shading(cell, COR_AZUL_ESC)
        aplicar_margens_celula(cell, 80, 80, 80, 80)
        p = cell.paragraphs[0]
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        formatar_paragrafo(p, 0, 0, 1.0)
        r = p.add_run(h)
        r.font.name = "Arial"
        r.font.size = Pt(8.5)
        r.font.bold = True
        r.font.color.rgb = RGBColor(255, 255, 255)

    dados_inv = [
        ("Protótipos / Produtos Tecnológicos", "Estação Agrometeorológica de Borda GAIA TX (Hardware e Firmware integrados)", "1"),
        ("Softwares / Algoritmos Registrados", "Firmware embarcado ESP32 com motor matemático agrometeorológico e Portal SoftAP", "1"),
        ("Resumos / Artigos em Eventos", "Trabalho técnico para submissão ao Encontro de Iniciação Científica e Tecnológica da UFU", "1"),
        ("Alunos de Graduação Envolvidos", "Estudante bolsista executor do plano de iniciação tecnológica", "1"),
        ("Equipamentos Utilizados", "Bancada de ensaios, microcontrolador ESP32, sensores ambientais e de solo", "Kit completo"),
    ]
    for row_idx, linha in enumerate(dados_inv, start=1):
        for col_idx, val in enumerate(linha):
            cell = t_inv.cell(row_idx, col_idx)
            cell.width = larg_i[col_idx]
            aplicar_shading(cell, "FAFAFA" if row_idx % 2 == 0 else "FFFFFF")
            aplicar_margens_celula(cell, 60, 60, 80, 80)
            p = cell.paragraphs[0]
            p.alignment = WD_ALIGN_PARAGRAPH.CENTER if col_idx == 2 else WD_ALIGN_PARAGRAPH.LEFT
            formatar_paragrafo(p, 0, 0, 1.1)
            r = p.add_run(val)
            r.font.name = "Arial"
            r.font.size = Pt(8.5)
            r.font.color.rgb = COR_TEXTO_PADRAO

    aplicar_borda_tabela(t_inv, COR_CINZA_BORDA)
    p_sp = doc.add_paragraph()
    formatar_paragrafo(p_sp, 0, 6, 1.0)

    # 6. Divulgação
    adicionar_faixa_secao(doc, "6. DIVULGAÇÃO CIENTÍFICA (até 250 palavras)")
    txt_div_r = (
        "A divulgação técnico-científica compreendeu a elaboração de documentação completa de engenharia de software "
        "e hardware em repositório aberto, tornando acessíveis os diagramas de pinagem, equações físicas implementadas "
        "e procedimentos de calibração. Os resultados foram consolidados em relatório técnico formal e material audiovisual "
        "(vídeo pitch) para apresentação na Mostra de Iniciação Científica e Tecnológica da UFU, servindo como material didático "
        "para disciplinas de graduação em instrumentação e sistemas embarcados."
    )
    adicionar_campo(doc, "6.1. Formas de Divulgação Científica Realizadas e Respectivo Impacto", txt_div_r)

    # 7. Premiações
    adicionar_faixa_secao(doc, "7. PREMIAÇÕES / RESUMO CIENTÍFICO DA PESQUISA (até 500 palavras)")
    txt_prem = (
        "O Projeto GAIA desenvolveu uma estação agrometeorológica inteligente e acessível que transforma a coleta de dados de campo "
        "em uma ferramenta ativa de tomada de decisão agronômica. Enquanto soluções comerciais tradicionais demandam investimentos "
        "elevados e restringem o processamento a servidores proprietários em nuvem, o GAIA introduz computação na borda (Edge Computing) "
        "em hardware aberto de alta disponibilidade (ESP32).\n\n"
        "O sistema integra sensores ambientais de temperatura, umidade, pressão barométrica, radiação solar e propriedades térmicas e "
        "hídricas do solo, efetuando calibração cruzada instantânea e gerando no próprio microcontrolador variáveis de alto impacto: "
        "Déficit de Pressão de Vapor (VPD) estomático, Evapotranspiração de Referência FAO-56, Radiação Fotossinteticamente Ativa (PAR/PPFD), "
        "risco de doenças fúngicas por acúmulo contínuo de horas de folha molhada e índice de estresse térmico em animais de produção (ITU).\n\n"
        "Dentre os diferenciais de engenharia, destacam-se a validação temporal atômica via NTP dispensando RTC físico; comunicação Wi-Fi "
        "criptografada por hardware com AES-128-CBC; tolerância a falhas com fila offline em cartão SD (Store-and-Forward); preservação de séries "
        "históricas na memória estática RTC RAM durante o Deep Sleep sem desgaste de memória Flash; e Portal Captivo autônomo para diagnóstico "
        "em tempo real pelo smartphone. O trabalho consolida uma contribuição de alto mérito tecnológico, aliando rigor metodológico, "
        "sustentabilidade no uso da água e democratização da Agricultura 4.0 para pequenos e médios produtores rurais."
    )
    adicionar_campo(doc, "7.1. Resumo Científico da Pesquisa para Avaliação em Premiações", txt_prem)


# =============================================================================
# FUNÇÃO PRINCIPAL DE GERAÇÃO DOS ARQUIVOS .DOCX
# =============================================================================
def gerar_todos_docx():
    base_dir = os.path.dirname(os.path.abspath(__file__))

    # 1. Gerar Anexo I
    doc_i = Document()
    for s in doc_i.sections:
        s.top_margin = Inches(0.8)
        s.bottom_margin = Inches(0.8)
        s.left_margin = Inches(0.8)
        s.right_margin = Inches(0.8)
    construir_docx_anexo_i(doc_i)
    path_i = os.path.join(base_dir, "Projeto_GAIA_Anexo_I_Proposta_Pesquisa_UFU.docx")
    doc_i.save(path_i)
    print(f" -> Anexo I DOCX gerado: {path_i}")

    # 2. Gerar Anexo IV
    doc_iv = Document()
    for s in doc_iv.sections:
        s.top_margin = Inches(0.8)
        s.bottom_margin = Inches(0.8)
        s.left_margin = Inches(0.8)
        s.right_margin = Inches(0.8)
    construir_docx_anexo_iv(doc_iv)
    path_iv = os.path.join(base_dir, "Projeto_GAIA_Anexo_IV_Relatorio_Tecnico_UFU.docx")
    doc_iv.save(path_iv)
    print(f" -> Anexo IV DOCX gerado: {path_iv}")

    # 3. Gerar Documento Completo Unificado
    doc_comp = Document()
    for s in doc_comp.sections:
        s.top_margin = Inches(0.8)
        s.bottom_margin = Inches(0.8)
        s.left_margin = Inches(0.8)
        s.right_margin = Inches(0.8)
    construir_docx_anexo_i(doc_comp)
    doc_comp.add_page_break()
    construir_docx_anexo_iv(doc_comp)
    path_comp = os.path.join(base_dir, "Projeto_GAIA_Proposta_e_Relatorio_UFU_Completo.docx")
    doc_comp.save(path_comp)
    print(f" -> Documento Completo DOCX gerado: {path_comp}")


if __name__ == "__main__":
    gerar_todos_docx()
