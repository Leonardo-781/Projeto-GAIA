#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Script gerador dos documentos PDF oficiais da UFU para o Projeto GAIA:
1. Anexo I - Proposta de Atividade de Pesquisa
2. Anexo IV - Relatório Técnico Circunstanciado
3. Documento Completo Unificado
"""

import os
from reportlab.lib.pagesizes import A4
from reportlab.lib import colors
from reportlab.lib.units import cm
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, PageBreak, KeepTogether, HRFlowable
)
from reportlab.pdfgen import canvas

# =============================================================================
# NUMERAÇÃO DE PÁGINAS (CANVAS NUMERADO)
# =============================================================================
class NumberedCanvas(canvas.Canvas):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._saved_page_states = []

    def showPage(self):
        self._saved_page_states.append(dict(self.__dict__))
        self._startPage()

    def save(self):
        num_pages = len(self._saved_page_states)
        for state in self._saved_page_states:
            self.__dict__.update(state)
            self.draw_page_number(num_pages)
            canvas.Canvas.showPage(self)
        canvas.Canvas.save(self)

    def draw_page_number(self, page_count):
        self.saveState()
        self.setFont("Helvetica", 8)
        self.setFillColor(colors.HexColor("#555555"))
        
        # Linha fina no rodapé
        self.setStrokeColor(colors.HexColor("#CCCCCC"))
        self.setLineWidth(0.5)
        self.line(1.5 * cm, 1.2 * cm, A4[0] - 1.5 * cm, 1.2 * cm)
        
        texto_rodape = f"Universidade Federal de Uberlândia - Pró-Reitoria de Pesquisa e Pós-Graduação (PROPP/DIRPE)"
        self.drawString(1.5 * cm, 0.8 * cm, texto_rodape)
        
        pagina_str = f"Página {self._pageNumber} de {page_count}"
        self.drawRightString(A4[0] - 1.5 * cm, 0.8 * cm, pagina_str)
        self.restoreState()


# =============================================================================
# ESTILOS TIPOGRÁFICOS
# =============================================================================
def criar_estilos():
    styles = getSampleStyleSheet()
    
    # Cores Institucionais
    c_primaria = colors.HexColor("#0f3460")
    c_texto = colors.HexColor("#222222")
    
    styles.add(ParagraphStyle(
        'CabecalhoGov',
        fontName='Helvetica-Bold',
        fontSize=8,
        leading=10,
        alignment=1,
        textColor=c_primaria
    ))
    
    styles.add(ParagraphStyle(
        'TituloDoc',
        fontName='Helvetica-Bold',
        fontSize=12,
        leading=15,
        alignment=1,
        textColor=colors.HexColor("#000000"),
        spaceAfter=10
    ))
    
    styles.add(ParagraphStyle(
        'SecaoTitulo',
        fontName='Helvetica-Bold',
        fontSize=10,
        leading=13,
        textColor=colors.white,
        spaceBefore=0,
        spaceAfter=0
    ))

    styles.add(ParagraphStyle(
        'SubSecaoTitulo',
        fontName='Helvetica-Bold',
        fontSize=9,
        leading=12,
        textColor=c_primaria,
        spaceBefore=4,
        spaceAfter=2
    ))

    styles.add(ParagraphStyle(
        'TextoCorpo',
        fontName='Helvetica',
        fontSize=8.5,
        leading=11.5,
        textColor=c_texto,
        alignment=4, # Justificado
        spaceAfter=4
    ))

    styles.add(ParagraphStyle(
        'TextoPreenchimento',
        fontName='Helvetica',
        fontSize=8.5,
        leading=11.5,
        textColor=colors.HexColor("#111111"),
        alignment=4,
        spaceBefore=2,
        spaceAfter=4
    ))

    styles.add(ParagraphStyle(
        'TabelaHeader',
        fontName='Helvetica-Bold',
        fontSize=8,
        leading=10,
        textColor=colors.white,
        alignment=1
    ))

    styles.add(ParagraphStyle(
        'TabelaCorpo',
        fontName='Helvetica',
        fontSize=8,
        leading=10,
        textColor=c_texto,
        alignment=0
    ))

    styles.add(ParagraphStyle(
        'TabelaCorpoCentro',
        fontName='Helvetica',
        fontSize=8,
        leading=10,
        textColor=c_texto,
        alignment=1
    ))

    return styles


# =============================================================================
# HELPER DE BLOCO DE SEÇÃO COM CABEÇALHO COLORIDO
# =============================================================================
def criar_faixa_secao(titulo, styles, bg_color="#0f3460"):
    t = Table([[Paragraph(titulo, styles['SecaoTitulo'])]], colWidths=[18 * cm])
    t.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,-1), colors.HexColor(bg_color)),
        ('TOPPADDING', (0,0), (-1,-1), 4),
        ('BOTTOMPADDING', (0,0), (-1,-1), 4),
        ('LEFTPADDING', (0,0), (-1,-1), 6),
        ('RIGHTPADDING', (0,0), (-1,-1), 6),
        ('VALIGN', (0,0), (-1,-1), 'MIDDLE'),
    ]))
    return t


def criar_bloco_campo(rotulo, conteudo, styles, bg_conteudo="#FAFAFA"):
    t = Table([
        [Paragraph(f"<b>{rotulo}</b>", styles['SubSecaoTitulo'])],
        [Paragraph(conteudo, styles['TextoPreenchimento'])]
    ], colWidths=[18 * cm])
    t.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,0), colors.HexColor("#EDF2F7")),
        ('BACKGROUND', (0,1), (-1,1), colors.HexColor(bg_conteudo)),
        ('BOX', (0,0), (-1,-1), 0.5, colors.HexColor("#CBD5E0")),
        ('LINEBELOW', (0,0), (-1,0), 0.5, colors.HexColor("#CBD5E0")),
        ('TOPPADDING', (0,0), (-1,-1), 3),
        ('BOTTOMPADDING', (0,0), (-1,-1), 4),
        ('LEFTPADDING', (0,0), (-1,-1), 6),
        ('RIGHTPADDING', (0,0), (-1,-1), 6),
    ]))
    return t


def cabecalho_ufu(styles):
    elementos = []
    texto_cabecalho = """SERVIÇO PÚBLICO FEDERAL<br/>
MINISTÉRIO DA EDUCAÇÃO<br/>
<b>UNIVERSIDADE FEDERAL DE UBERLÂNDIA</b><br/>
PRÓ-REITORIA DE PESQUISA E PÓS-GRADUAÇÃO<br/>
DIRETORIA DE PESQUISA / DIVISÃO DE FOMENTO A PROGRAMAS E PROJETOS"""
    elementos.append(Paragraph(texto_cabecalho, styles['CabecalhoGov']))
    elementos.append(Spacer(1, 0.4 * cm))
    elementos.append(HRFlowable(width="100%", thickness=1, color=colors.HexColor("#0f3460"), spaceAfter=10))
    return elementos


# =============================================================================
# CONSTRUÇÃO DO ANEXO I - PROPOSTA DE PESQUISA
# =============================================================================
def construir_anexo_i(styles):
    story = []
    story.extend(cabecalho_ufu(styles))
    story.append(Paragraph("ANEXO I – PROPOSTA DE ATIVIDADE DE PESQUISA", styles['TituloDoc']))
    story.append(Spacer(1, 0.2 * cm))

    # 1. Informações Institucionais
    story.append(criar_faixa_secao("1. INFORMAÇÕES INSTITUCIONAIS DO(A) COORDENADOR(A)", styles))
    t1_data = [
        [Paragraph("<b>1.1. Nome:</b> [Preencher Nome do(a) Orientador(a)]", styles['TextoCorpo']),
         Paragraph("<b>1.2. E-mail:</b> [Preencher E-mail]", styles['TextoCorpo'])],
        [Paragraph("<b>1.3. Unidade Acadêmica:</b> Faculdade de Engenharia Elétrica / Computação / Agronomia (UFU)", styles['TextoCorpo']),
         Paragraph("<b>1.4. Campus:</b> Santa Mônica / Umuarama / Glória", styles['TextoCorpo'])]
    ]
    t1 = Table(t1_data, colWidths=[9 * cm, 9 * cm])
    t1.setStyle(TableStyle([
        ('BOX', (0,0), (-1,-1), 0.5, colors.HexColor("#CBD5E0")),
        ('INNERGRID', (0,0), (-1,-1), 0.5, colors.HexColor("#E2E8F0")),
        ('TOPPADDING', (0,0), (-1,-1), 4),
        ('BOTTOMPADDING', (0,0), (-1,-1), 4),
        ('LEFTPADDING', (0,0), (-1,-1), 6),
        ('RIGHTPADDING', (0,0), (-1,-1), 6),
    ]))
    story.append(t1)
    story.append(Spacer(1, 0.3 * cm))

    # 2. Descrição do Projeto
    story.append(criar_faixa_secao("2. DESCRIÇÃO DO PROJETO DE PESQUISA", styles))
    t2_data = [
        [Paragraph("<b>2.1. Código do Edital:</b> DIRPE XX/20XX &nbsp;&nbsp; ( X ) PIBITI-CNPq &nbsp;&nbsp; ( X ) PIBIC-CNPq &nbsp;&nbsp; ( ) PIVIC", styles['TextoCorpo'])],
        [Paragraph("<b>2.2. Título do Projeto:</b> Desenvolvimento de Estação Agrometeorológica de Borda de Baixo Custo com Telemetria Conectada e Diagnóstico Fitossanitário em Tempo Real (Projeto GAIA)", styles['TextoCorpo'])],
        [Paragraph("<b>2.3. Palavras-Chave:</b> Agrometeorologia. Internet Das Coisas. Agricultura De Precisão.", styles['TextoCorpo'])],
        [Paragraph("<b>2.4. Área do Conhecimento:</b> ( X ) Engenharias &nbsp;&nbsp;&nbsp;&nbsp; ( X ) Ciências Agrárias &nbsp;&nbsp;&nbsp;&nbsp; ( ) Ciências Exatas e da Terra", styles['TextoCorpo'])],
        [Paragraph("<b>2.5. Grupo de Pesquisa CNPq:</b> Automação, Sistemas Embarcados e Tecnologias para o Agronegócio (UFU)", styles['TextoCorpo'])],
        [Paragraph("<b>2.6. Arranjo Institucional:</b> Universidade Federal de Uberlândia (UFU) - Laboratório de Sistemas Embarcados / Automação", styles['TextoCorpo'])],
        [Paragraph("<b>2.7. Protocolo CEP/CEUA:</b> Não se aplica (Desenvolvimento de hardware embarcado e sensoriamento microclimático)", styles['TextoCorpo'])],
        [Paragraph("<b>2.8. Linha de Pesquisa:</b> Agricultura 4.0, Sensoriamento Inteligente e Cidades/Campos Conectados", styles['TextoCorpo'])],
    ]
    t2 = Table(t2_data, colWidths=[18 * cm])
    t2.setStyle(TableStyle([
        ('BOX', (0,0), (-1,-1), 0.5, colors.HexColor("#CBD5E0")),
        ('INNERGRID', (0,0), (-1,-1), 0.5, colors.HexColor("#E2E8F0")),
        ('TOPPADDING', (0,0), (-1,-1), 3),
        ('BOTTOMPADDING', (0,0), (-1,-1), 3),
        ('LEFTPADDING', (0,0), (-1,-1), 6),
        ('RIGHTPADDING', (0,0), (-1,-1), 6),
    ]))
    story.append(t2)
    story.append(Spacer(1, 0.3 * cm))

    # 3. Viés Tecnológico
    story.append(criar_faixa_secao("3. VIÉS TECNOLÓGICO (PIBITI - até 500 palavras)", styles))
    txt_vies = (
        "O projeto apresenta inequívoco caráter de inovação tecnológica ao conceber uma estação meteorológica "
        "inteligente de baixo custo baseada no microcontrolador ESP32 de 32 bits, com capacidade nativa de computação "
        "na borda (<i>Edge Computing</i>). Ao contrário das estações convencionais de mercado, que atuam meramente como "
        "registradores passivos (<i>dataloggers</i>) e exigem servidores pagos na nuvem para processamento, o Projeto GAIA "
        "executa cálculos agrometeorológicos de alta complexidade diretamente no microcontrolador: Déficit de Pressão "
        "de Vapor (VPD) com diagnóstico estomático, Evapotranspiração de Referência (Penman-Monteith FAO-56), "
        "Radiação Fotossinteticamente Ativa (PAR/PPFD), estimativa de radiação global (Rs), Índice de Conforto "
        "Zootécnico (ITU/THI) e classificação de risco para geadas (branca e negra).<br/><br/>"
        "O avanço tecnológico reside também na arquitetura de resiliência: validação temporal via Relógio Atômico "
        "brasileiro (NTP.br) dispensando módulos RTC mecânicos frágeis; encriptação nativa por hardware AES-128-CBC; "
        "preservação de séries temporais na memória estática de baixo consumo RTC RAM (<code>RTC_DATA_ATTR</code>) "
        "durante ciclos de repouso profundo (<i>Deep Sleep</i>); persistência em fila offline no cartão SD (<i>Store-and-Forward</i>); "
        "e portal web captivo (<i>SoftAP</i>) com diagnóstico em tempo real no celular sem requerer aplicativos externos.<br/><br/>"
        "<b>Nível de Maturidade Tecnológica:</b> O projeto encontra-se em <b>TRL 5</b> (protótipo funcional integrado com todos os "
        "sensores validados em ambiente simulado e bancada operacional), com previsão de alcançar <b>TRL 6/7</b> com os ensaios "
        "de campo continuados propostos neste plano."
    )
    story.append(criar_bloco_campo("3.1. Descrição da Inovação Tecnológica, Ganhos de Qualidade/Eficiência e Escala TRL", txt_vies, styles))
    story.append(Spacer(1, 0.3 * cm))

    # 4. Detalhamento da Proposta
    story.append(criar_faixa_secao("4. DETALHAMENTO DA PROPOSTA", styles))
    
    txt_resumo = (
        "O monitoramento microclimático preciso é fundamental para a agricultura de precisão e para a sustentabilidade "
        "hídrica. Contudo, estações comerciais possuem custo proibitivo para pequenos e médios produtores, além de demandarem "
        "manutenção especializada. Este projeto propõe o desenvolvimento e aprimoramento da estação agrometeorológica de "
        "campo 'Projeto GAIA', um sistema embarcado de baixo custo baseado no ESP32. O sistema integra sensores de temperatura "
        "e umidade do ar (DHT22), pressão barométrica (BMP280), radiação solar/iluminância (VEML7700), temperatura do solo em "
        "nível radicular (DS18B20) e umidade volumétrica do solo.<br/>"
        "Os dados são validados temporalmente com o relógio atômico nacional e processados na própria estação, gerando "
        "indicadores vitais: ponto de orvalho, VPD estomático, evapotranspiração da cultura (ETc), estimativa de radiação "
        "fotossintética, horas de molhamento foliar para alerta fúngico, risco de geada (branca e negra) e índice de conforto "
        "térmico animal (ITU). A transmissão é realizada via Wi-Fi/HTTP com criptografia AES-128 e redundância em cartão SD. "
        "Os resultados visam entregar uma solução acessível, robusta e autônoma, democratizando dados micrometeorológicos "
        "aplicados ao manejo racional de irrigação e fitossanidade."
    )
    story.append(criar_bloco_campo("4.1. Resumo da Proposta (até 300 palavras)", txt_resumo, styles))
    story.append(Spacer(1, 0.2 * cm))

    txt_problema = (
        "<b>Problema:</b> A tomada de decisão agrícola no Brasil (irrigação, pulverização e prevenção fitossanitária) "
        "ainda é realizada predominantemente com base em previsões meteorológicas genéricas de escala municipal, as quais "
        "desconsideram o microclima específico da lavoura. Estações agrometeorológicas profissionais de alta precisão "
        "possuem custo elevado (frequentemente superior a R$ 15.000,00), exigem conectividade de dados ininterrupta e "
        "utilizam protocolos fechados.<br/>"
        "<b>Pergunta e Hipótese:</b> É tecnicamente viável desenvolver uma estação agrometeorológica modular, alimentada a "
        "bateria/solar, com custo inferior a 10% das opções comerciais, capaz de computar métricas micrometeorológicas complexas "
        "diretamente na borda (Edge Computing) e manter a operação autônoma sem perda de dados mesmo sob intermitência de sinal de rede?"
    )
    story.append(criar_bloco_campo("4.2. Problema Abordado e Pergunta/Hipótese Principal (até 250 palavras)", txt_problema, styles))
    story.append(Spacer(1, 0.2 * cm))

    txt_ref = (
        "1. <b>ALLEN, R. G. et al.</b> <i>Crop evapotranspiration: Guidelines for computing crop water requirements.</i> FAO Irrigation and drainage paper 56, Rome, 1998. "
        "[Estabelece o padrão biofísico Penman-Monteith adotado diretamente no código para cálculo da demanda evapotranspirativa de referência].<br/>"
        "2. <b>ALVARES, C. A. et al.</b> <i>Köppen's climate classification map for Brazil.</i> Meteorologische Zeitschrift, v. 22, n. 6, 2013. "
        "[Fundamenta a distribuição de extremos climáticos e a importância da medição local de gradientes térmicos e riscos de geada].<br/>"
        "3. <b>OJHA, T. et al.</b> <i>Wireless sensor networks for agriculture: The state-of-the-art in practice and future challenges.</i> Computers and Electronics in Agriculture, 2015. "
        "[Referência de arquitetura para redes de sensores sem fio agrícolas, nós autônomos de baixo consumo e tolerância a falhas temporais]."
    )
    story.append(criar_bloco_campo("4.3. Principais Referências Bibliográficas (até 250 palavras)", txt_ref, styles))
    story.append(Spacer(1, 0.2 * cm))

    txt_metodo = (
        "A metodologia adotada é experimental e de desenvolvimento tecnológico aplicado, dividida em quatro fases:<br/>"
        "<b>1. Arquitetura de Hardware e Sensoriamento:</b> Integração dos sensores DHT22 (ar), BMP280 (pressão I2C), "
        "VEML7700 (luz solar I2C), DS18B20 (sonda de solo 1-Wire em GPIO 13) e sensor de umidade de solo analógico (ADC 34) "
        "ao microcontrolador ESP32 dual-core de 240 MHz, com reguladores de baixíssimo ruído e barramento I2C protegido com timeout.<br/>"
        "<b>2. Motor Matemático de Borda (Edge Computing):</b> Codificação de funções otimizadas em ponto flutuante para Ponto de Orvalho "
        "(Magnus-Tetens), Déficit de Pressão de Vapor (VPD estomático), Evapotranspiração horária e diária Penman-Monteith (FAO-56), "
        "Radiação Global (Rs em W/m²) e PAR/PPFD (µmol/m²·s). Implementação de buffer circular na memória estática RTC RAM "
        "(<code>RTC_DATA_ATTR</code>) para rastreamento de pressão barométrica em janela móvel de 3 horas (alerta de tempestades) "
        "e acúmulo contínuo de horas de folha molhada sem desgaste de flash.<br/>"
        "<b>3. Conectividade Segura e Usabilidade:</b> Validação temporal precisa via Relógio Atômico nacional (NTP.br: a.st1.ntp.br); "
        "cifragem de payload em hardware com AES-128-CBC; mecanismo Store-and-Forward no cartão SD para garantia de entrega; e portal captivo "
        "Wi-Fi com DNS integrado na porta 53 para auditoria em tempo real dos sensores em campo pelo celular.<br/>"
        "<b>4. Testes e Calibração:</b> Ensaios comparativos de bancada e campo para avaliação de desvio entre sensores redundantes "
        "(DHT vs BMP), teste de estresse térmico, medição de corrente em repouso profundo e validação da integridade de pacotes transmitidos."
    )
    story.append(criar_bloco_campo("4.4. Método para Obtenção da Resposta ao Problema (até 500 palavras)", txt_metodo, styles))
    story.append(Spacer(1, 0.2 * cm))

    # 4.5. Tabela de Metas
    t_metas_data = [
        [Paragraph("<b>META</b>", styles['TabelaHeader']),
         Paragraph("<b>DESCRIÇÃO DETALHADA</b>", styles['TabelaHeader']),
         Paragraph("<b>QUANTIDADE</b>", styles['TabelaHeader']),
         Paragraph("<b>PREVISÃO</b>", styles['TabelaHeader'])],
        [Paragraph("1", styles['TabelaCorpoCentro']),
         Paragraph("Integração física do circuito embarcado, proteções I2C/1-Wire e testes dos sensores ambientais e de solo", styles['TabelaCorpo']),
         Paragraph("1 protótipo", styles['TabelaCorpoCentro']),
         Paragraph("Mês 03", styles['TabelaCorpoCentro'])],
        [Paragraph("2", styles['TabelaCorpoCentro']),
         Paragraph("Desenvolvimento dos algoritmos de borda (VPD, ET0 FAO-56, PAR, ITU e variáveis em RTC RAM)", styles['TabelaCorpo']),
         Paragraph("1 firmware", styles['TabelaCorpoCentro']),
         Paragraph("Mês 06", styles['TabelaCorpoCentro'])],
        [Paragraph("3", styles['TabelaCorpoCentro']),
         Paragraph("Implementação da telemetria Wi-Fi segura (AES-128), NTP atômico, fila SD e Portal Captivo SoftAP", styles['TabelaCorpo']),
         Paragraph("1 sistema", styles['TabelaCorpoCentro']),
         Paragraph("Mês 08", styles['TabelaCorpoCentro'])],
        [Paragraph("4", styles['TabelaCorpoCentro']),
         Paragraph("Ensaios de campo contínuos, análise de consistência agronômica e elaboração de relatório final/artigo", styles['TabelaCorpo']),
         Paragraph("1 relatório / artigo", styles['TabelaCorpoCentro']),
         Paragraph("Mês 12", styles['TabelaCorpoCentro'])],
    ]
    t_metas = Table(t_metas_data, colWidths=[1.5 * cm, 11 * cm, 2.5 * cm, 3 * cm])
    t_metas.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,0), colors.HexColor("#0f3460")),
        ('GRID', (0,0), (-1,-1), 0.5, colors.HexColor("#CBD5E0")),
        ('VALIGN', (0,0), (-1,-1), 'MIDDLE'),
        ('TOPPADDING', (0,0), (-1,-1), 3),
        ('BOTTOMPADDING', (0,0), (-1,-1), 3),
    ]))
    story.append(Paragraph("<b>4.5. Metas de Produção Intelectual e Tecnológica</b>", styles['SubSecaoTitulo']))
    story.append(t_metas)
    story.append(Spacer(1, 0.3 * cm))

    txt_imp = (
        "A relevância desta pesquisa para a Universidade Federal de Uberlândia (UFU) consolida-se em múltiplos eixos:<br/>"
        "• <b>Impacto Científico e Tecnológico:</b> Criação de propriedade intelectual e consolidação de conhecimento prático "
        "em Computação na Borda aplicada à Agrometeorologia, estabelecendo uma base de código aberta e replicável para projetos "
        "futuros de graduação e pós-graduação na UFU.<br/>"
        "• <b>Impacto Ambiental e Hídrico:</b> O monitoramento local de VPD e Evapotranspiração permite irrigar exatamente o volume "
        "requerido pela cultura no estádio fenológico atual, combatendo o desperdício de água e reduzindo o consumo energético de bombas.<br/>"
        "• <b>Impacto Social e Econômico:</b> Oferta de uma alternativa viável e acessível para produtores rurais familiares, cooperativas "
        "e pequenas propriedades do Triângulo Mineiro e Alto Paranaíba que não têm acesso a estações agrometeorológicas comerciais.<br/>"
        "• <b>Impacto Acadêmico na UFU:</b> Formação avançada do estudante bolsista em internet das coisas, programação de sistemas de tempo real, "
        "sensoriamento microclimático e segurança de dados, gerando produção acadêmica qualificada para publicação institucional."
    )
    story.append(criar_bloco_campo("4.6. Relevância da Proposta e Impactos Esperados na UFU (até 500 palavras)", txt_imp, styles))
    story.append(Spacer(1, 0.2 * cm))

    # ODS
    txt_ods = (
        "[ X ] 2. Fome zero e agricultura sustentável &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; [ X ] 6. Água limpa e saneamento<br/>"
        "[ X ] 9. Indústria, inovação e infraestrutura &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; [ X ] 12. Consumo e produção responsáveis<br/>"
        "[ X ] 13. Ação contra mudança global do clima &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; [ X ] 15. Vida terrestre"
    )
    story.append(criar_bloco_campo("4.7. Relação com os Objetivos de Desenvolvimento Sustentável (ODS)", txt_ods, styles))
    story.append(Spacer(1, 0.3 * cm))

    # 5. Recursos Financeiros
    story.append(criar_faixa_secao("5. RECURSOS FINANCEIROS PREVISTOS", styles))
    t_rec_data = [
        [Paragraph("<b>DESPESAS</b>", styles['TabelaHeader']),
         Paragraph("<b>DESCRIÇÃO DETALHADA</b>", styles['TabelaHeader']),
         Paragraph("<b>QUANTIDADE</b>", styles['TabelaHeader']),
         Paragraph("<b>VALOR TOTAL</b>", styles['TabelaHeader'])],
        [Paragraph("Bolsa", styles['TabelaCorpo']), Paragraph("Bolsa de Iniciação Científica/Tecnológica (PIBITI/PIBIC)", styles['TabelaCorpo']), Paragraph("12 meses", styles['TabelaCorpoCentro']), Paragraph("Conforme edital", styles['TabelaCorpoCentro'])],
        [Paragraph("Material de Consumo", styles['TabelaCorpo']), Paragraph("Sensores (DHT22, BMP280, VEML7700, DS18B20), ESP32, cabos, cartões SD, caixas IP65", styles['TabelaCorpo']), Paragraph("Itens bancada", styles['TabelaCorpoCentro']), Paragraph("Recursos próprios/Lab", styles['TabelaCorpoCentro'])],
        [Paragraph("Material Permanente", styles['TabelaCorpo']), Paragraph("Computador e instrumentos de medição (osciloscópio/multímetro) do laboratório", styles['TabelaCorpo']), Paragraph("Existente", styles['TabelaCorpoCentro']), Paragraph("R$ 0,00", styles['TabelaCorpoCentro'])],
        [Paragraph("Serviços Terceiros", styles['TabelaCorpo']), Paragraph("Não previstos", styles['TabelaCorpo']), Paragraph("-", styles['TabelaCorpoCentro']), Paragraph("R$ 0,00", styles['TabelaCorpoCentro'])],
    ]
    t_rec = Table(t_rec_data, colWidths=[3.5 * cm, 9.5 * cm, 2.5 * cm, 2.5 * cm])
    t_rec.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,0), colors.HexColor("#0f3460")),
        ('GRID', (0,0), (-1,-1), 0.5, colors.HexColor("#CBD5E0")),
        ('VALIGN', (0,0), (-1,-1), 'MIDDLE'),
        ('TOPPADDING', (0,0), (-1,-1), 3),
        ('BOTTOMPADDING', (0,0), (-1,-1), 3),
    ]))
    story.append(t_rec)
    story.append(Spacer(1, 0.3 * cm))

    # 6. Plano de Divulgação
    story.append(criar_faixa_secao("6. PLANO DE DIVULGAÇÃO CIENTÍFICA (até 250 palavras)", styles))
    txt_div = (
        "A divulgação do conhecimento gerado no projeto seguirá plano estruturado em três níveis:<br/>"
        "1. <b>Comunidade Acadêmica Local:</b> Apresentação obrigatória no Encontro de Iniciação Científica e Tecnológica da UFU, "
        "com elaboração de resumo expandido e apresentação oral/pôster técnico.<br/>"
        "2. <b>Comunidade Científica Externa:</b> Elaboração e submissão de artigo técnico-científico em congresso nacional de automação "
        "ou engenharia agrícola (ex: Congresso Brasileiro de Engenharia Agrícola - CONBEA ou Simpósio Brasileiro de Automação Inteligente - SBAI).<br/>"
        "3. <b>Sociedade e Produtores:</b> Disponibilização da documentação técnica e código-fonte em repositório público de acesso aberto (GitHub), "
        "além de confecção de manual de montagem ilustrado para incentivo ao uso da tecnologia por escolas técnicas e produtores familiares."
    )
    story.append(criar_bloco_campo("6.1. Formas de Divulgação Científica", txt_div, styles))

    return story


# =============================================================================
# CONSTRUÇÃO DO ANEXO IV - RELATÓRIO CIRCUNSTANCIADO
# =============================================================================
def construir_anexo_iv(styles):
    story = []
    story.extend(cabecalho_ufu(styles))
    story.append(Paragraph("ANEXO IV – RELATÓRIO TÉCNICO CIRCUNSTANCIADO", styles['TituloDoc']))
    story.append(Paragraph("( X ) Final &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; ( ) Cancelamento", styles['CabecalhoGov']))
    story.append(Spacer(1, 0.2 * cm))

    # 1. Informações Institucionais
    story.append(criar_faixa_secao("1. INFORMAÇÕES INSTITUCIONAIS", styles))
    t1_data = [
        [Paragraph("<b>1.1. Orientador(a):</b> [Preencher Nome do(a) Orientador(a)]", styles['TextoCorpo']),
         Paragraph("<b>1.2. E-mail:</b> [Preencher E-mail]", styles['TextoCorpo'])],
        [Paragraph("<b>1.3. Unidade Acadêmica:</b> Faculdade de Engenharia Elétrica / Computação / Agronomia (UFU)", styles['TextoCorpo']),
         Paragraph("<b>1.4. Campus:</b> Santa Mônica / Umuarama / Glória", styles['TextoCorpo'])],
        [Paragraph("<b>1.5. Bolsista:</b> Leonardo [Sobrenome]", styles['TextoCorpo']),
         Paragraph("<b>1.6. E-mail:</b> [Preencher E-mail]", styles['TextoCorpo'])],
        [Paragraph("<b>1.7. Curso:</b> Engenharia / Ciência da Computação / Agronomia", styles['TextoCorpo']),
         Paragraph("<b>1.8. Campus:</b> Santa Mônica", styles['TextoCorpo'])],
        [Paragraph("<b>1.9. Título da Pesquisa:</b> Desenvolvimento de Estação Agrometeorológica de Borda de Baixo Custo com Telemetria Conectada e Diagnóstico Fitossanitário em Tempo Real (Projeto GAIA)", styles['TextoCorpo']),
         Paragraph("<b>1.10. Vídeo Pitch:</b> [Inserir link do vídeo pitch]", styles['TextoCorpo'])],
        [Paragraph("<b>1.11. Edital:</b> DIRPE XX/20XX &nbsp;&nbsp; ( X ) PIBITI-CNPq &nbsp;&nbsp; ( X ) PIBIC-CNPq", styles['TextoCorpo']),
         Paragraph("<b>1.12. Vigência:</b> MÊS/20XX A MÊS/20XX", styles['TextoCorpo'])]
    ]
    t1 = Table(t1_data, colWidths=[9 * cm, 9 * cm])
    t1.setStyle(TableStyle([
        ('BOX', (0,0), (-1,-1), 0.5, colors.HexColor("#CBD5E0")),
        ('INNERGRID', (0,0), (-1,-1), 0.5, colors.HexColor("#E2E8F0")),
        ('TOPPADDING', (0,0), (-1,-1), 3),
        ('BOTTOMPADDING', (0,0), (-1,-1), 3),
        ('LEFTPADDING', (0,0), (-1,-1), 6),
        ('RIGHTPADDING', (0,0), (-1,-1), 6),
    ]))
    story.append(t1)
    story.append(Spacer(1, 0.3 * cm))

    # 2. Síntese da Execução
    story.append(criar_faixa_secao("2. SÍNTESE DA EXECUÇÃO DO PROJETO (até 500 palavras)", styles))
    txt_sintese = (
        "O projeto cumpriu integralmente o plano de atividades previsto, entregando um sistema embarcado funcional, "
        "robusto e calibrado. Foram integrados com êxito os transdutores de temperatura e umidade relativa do ar (DHT22), "
        "pressão barométrica e temperatura secundária (BMP280), radiação solar/iluminância (VEML7700), temperatura do solo "
        "em profundidade radicular (DS18B20 digital 1-Wire em GPIO 13) e sonda de umidade volumétrica de solo (ADC 34) ao microcontrolador ESP32.<br/><br/>"
        "<b>Reformulações e Decisões Técnicas de Destaque:</b><br/>"
        "1. <i>Eliminação do RTC Físico e Adoção do Tempo Atômico:</i> Constatou-se durante os testes que módulos RTC convencionais (DS3231) "
        "apresentavam desvios cumulativos e dependiam de bateria tipo moeda propensa a falhas em campo sob altas temperaturas. "
        "Reformulou-se o projeto para validar o timestamp oficial via Relógio Atômico brasileiro (protocolo NTP: a.st1.ntp.br), mantido com "
        "precisão de microssegundos pelo oscilador interno do ESP32.<br/>"
        "2. <i>Comunicação Direta Wi-Fi/Internet com Criptografia AES-128:</i> A dependência de transceptores LoRa locais foi substituída "
        "por conectividade Wi-Fi nativa de longo alcance com encriptação simétrica AES-128-CBC e padding PKCS#7 diretamente por hardware (mbedTLS), "
        "permitindo envio de dados para servidores REST em nuvem com total sigilo de dados.<br/>"
        "3. <i>Variáveis Históricas na Memória RTC RAM (Deep Sleep):</i> Para acompanhar a tendência barométrica das últimas 3 horas (detecção "
        "de tempestades) e o contador cumulativo de horas de folha molhada sem desgastar as células de memória Flash com regravações repetidas, "
        "utilizou-se a memória estática rápida RTC RAM (<code>RTC_DATA_ATTR</code>), que mantém dados intactos durante o repouso profundo.<br/>"
        "4. <i>Portal Captivo SoftAP:</i> Desenvolveu-se um portal web embarcado com servidor DNS próprio (192.168.4.1), permitindo ao "
        "produtor rural auditar o funcionamento dos sensores em tempo real na tela do celular e configurar parâmetros de rede sem qualquer cabo USB."
    )
    story.append(criar_bloco_campo("2.1. Relato das Atividades, Aspectos Positivos, Problemas e Reformulações", txt_sintese, styles))
    story.append(Spacer(1, 0.3 * cm))

    # 3. Recursos Efetivamente Obtidos
    story.append(criar_faixa_secao("3. RECURSOS EFETIVAMENTE OBTIDOS (até 250 palavras)", styles))
    txt_rec_obtidos = (
        "O projeto foi executado com o apoio da bolsa de Iniciação Científica/Tecnológica concedida ao estudante. "
        "Os insumos materiais (módulos eletrônicos, sensores, protoboards, ferramentas de soldagem e cabos) foram viabilizados "
        "pela estrutura existente dos laboratórios da UFU e por recursos próprios dos pesquisadores envolvidos, sem necessidade "
        "de captação de recursos financeiros adicionais."
    )
    story.append(criar_bloco_campo("3.1. Recursos Financeiros e Infraestrutura Utilizada", txt_rec_obtidos, styles))
    story.append(Spacer(1, 0.3 * cm))

    # 4. Levantamento de Resultados
    story.append(criar_faixa_secao("4. LEVANTAMENTO DE RESULTADOS / METAS / AÇÕES ALCANÇADAS", styles))
    txt_res_prob = (
        "<b>Sim, o problema da pesquisa foi plenamente resolvido.</b> O protótipo demonstrou capacidade de computar indicadores "
        "agronômicos de precisão diretamente no hardware de borda, sem sobrecarregar a memória nem comprometer o consumo de energia.<br/>"
        "<b>Principais Contribuições Atingidas:</b><br/>"
        "• <i>Autonomia de Diagnóstico:</i> O ESP32 entrega índices prontos para tomada de decisão (VPD estomático ideal entre 0.4 e 1.4 kPa, "
        "necessidade de irrigação via ET0 e ETc, radiação fotossintética PAR e estresse térmico animal via ITU de Thom).<br/>"
        "• <i>Segurança Fitossanitária:</i> Detecção precoce de condições de condensação na folha (|Tar - Tpo| <= 1.5°C e RH >= 85%) "
        "com emissão de alertas automáticos para esporulação fúngica e classificação de risco de geadas branca e negra.<br/>"
        "• <i>Confiabilidade dos Dados:</i> Validação cruzada com sensor duplo de temperatura e cálculo de escore de integridade (q) "
        "no payload JSON, prevenindo que dados corrompidos alimentem sistemas de automação de irrigação.<br/>"
        "• <i>Resiliência Offline:</i> Garantia de retenção e reenvio de leituras via cartão SD (Store-and-Forward) sob quedas temporárias de sinal."
    )
    story.append(criar_bloco_campo("4.1. Resolução do Problema e Contribuições do Trabalho (até 500 palavras)", txt_res_prob, styles))
    story.append(Spacer(1, 0.2 * cm))

    # Tabela 4.2 Metas vs Resultados
    t_res_data = [
        [Paragraph("<b>META</b>", styles['TabelaHeader']),
         Paragraph("<b>DESCRIÇÃO DETALHADA</b>", styles['TabelaHeader']),
         Paragraph("<b>QUANT.</b>", styles['TabelaHeader']),
         Paragraph("<b>ALCANÇADO</b>", styles['TabelaHeader']),
         Paragraph("<b>DATA</b>", styles['TabelaHeader']),
         Paragraph("<b>OBSERVAÇÕES</b>", styles['TabelaHeader'])],
        [Paragraph("1", styles['TabelaCorpoCentro']), Paragraph("Montagem e integração física do circuito embarcado com 5 sensores ambientais/solo", styles['TabelaCorpo']), Paragraph("1", styles['TabelaCorpoCentro']), Paragraph("Sim", styles['TabelaCorpoCentro']), Paragraph("Mês 03", styles['TabelaCorpoCentro']), Paragraph("Operação estável com I2C e 1-Wire", styles['TabelaCorpo'])],
        [Paragraph("2", styles['TabelaCorpoCentro']), Paragraph("Implementação do motor matemático na borda (VPD, ET0 FAO-56, PAR, ITU e RTC RAM)", styles['TabelaCorpo']), Paragraph("1", styles['TabelaCorpoCentro']), Paragraph("Sim", styles['TabelaCorpoCentro']), Paragraph("Mês 06", styles['TabelaCorpoCentro']), Paragraph("Tempo de cálculo < 2 milissegundos", styles['TabelaCorpo'])],
        [Paragraph("3", styles['TabelaCorpoCentro']), Paragraph("Telemetria Wi-Fi segura (AES-128), NTP atômico, fila no SD e Portal Captivo Web", styles['TabelaCorpo']), Paragraph("1", styles['TabelaCorpoCentro']), Paragraph("Sim", styles['TabelaCorpoCentro']), Paragraph("Mês 08", styles['TabelaCorpoCentro']), Paragraph("Configuração via smartphone 100% ativa", styles['TabelaCorpo'])],
        [Paragraph("4", styles['TabelaCorpoCentro']), Paragraph("Validação continuada em bancada/campo simulado e documentação técnica", styles['TabelaCorpo']), Paragraph("1", styles['TabelaCorpoCentro']), Paragraph("Sim", styles['TabelaCorpoCentro']), Paragraph("Mês 12", styles['TabelaCorpoCentro']), Paragraph("Código aberto publicado e relatório final", styles['TabelaCorpo'])],
    ]
    t_res = Table(t_res_data, colWidths=[1.2 * cm, 7.5 * cm, 1.3 * cm, 1.8 * cm, 1.8 * cm, 4.4 * cm])
    t_res.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,0), colors.HexColor("#0f3460")),
        ('GRID', (0,0), (-1,-1), 0.5, colors.HexColor("#CBD5E0")),
        ('VALIGN', (0,0), (-1,-1), 'MIDDLE'),
        ('TOPPADDING', (0,0), (-1,-1), 3),
        ('BOTTOMPADDING', (0,0), (-1,-1), 3),
    ]))
    story.append(Paragraph("<b>4.2. Relação de Metas Previstas com Resultados Efetivamente Alcançados</b>", styles['SubSecaoTitulo']))
    story.append(t_res)
    story.append(Spacer(1, 0.3 * cm))

    txt_futuro = (
        "Como continuidade e desdobramentos futuros para a linha de pesquisa na UFU, identificam-se:<br/>"
        "1. <b>Atualização Remota de Firmware (FOTA):</b> Estruturação de mecanismo seguro Over-The-Air para entrega de "
        "atualizações em lote nas estações de campo sem necessidade de intervenção física nos mastros meteorológicos.<br/>"
        "2. <b>Modelagem Fenológica Avançada:</b> Implementação de contadores térmicos de Graus-Dia de Desenvolvimento (GDD) "
        "e cálculo automático do Delta T Agronômico (janela ideal para pulverização de defensivos sem risco de deriva por evaporação).<br/>"
        "3. <b>Módulo Solar Autônomo:</b> Integração de painel fotovoltaico de 10W com circuito MPPT e baterias LiFePO4 para "
        "garantia de autonomia contínua em campo aberto por múltiplos anos.<br/>"
        "4. <b>Intercomparação com Estações Oficiais:</b> Instalação de nó piloto próximo à estação meteorológica do INMET/UFU "
        "para análise estatística formal de correlação e validação metrológica dos sensores de baixo custo."
    )
    story.append(criar_bloco_campo("4.3. Possibilidades de Trabalho Futuro Decorrentes da Pesquisa (até 500 palavras)", txt_futuro, styles))
    story.append(Spacer(1, 0.3 * cm))

    # 5. Inventário dos Desdobramentos
    story.append(criar_faixa_secao("5. INVENTÁRIO DETALHADO DOS DESDOBRAMENTOS DA PESQUISA", styles))
    t_inv_data = [
        [Paragraph("<b>DESCRIÇÃO</b>", styles['TabelaHeader']),
         Paragraph("<b>ESPECIFICAÇÃO</b>", styles['TabelaHeader']),
         Paragraph("<b>QUANTIDADE</b>", styles['TabelaHeader'])],
        [Paragraph("Protótipos / Produtos Tecnológicos", styles['TabelaCorpo']), Paragraph("Estação Agrometeorológica de Borda GAIA TX (Hardware e Firmware integrados)", styles['TabelaCorpo']), Paragraph("1", styles['TabelaCorpoCentro'])],
        [Paragraph("Softwares / Algoritmos Registrados", styles['TabelaCorpo']), Paragraph("Firmware embarcado ESP32 com motor matemático agrometeorológico e Portal SoftAP", styles['TabelaCorpo']), Paragraph("1", styles['TabelaCorpoCentro'])],
        [Paragraph("Resumos / Artigos em Eventos", styles['TabelaCorpo']), Paragraph("Trabalho técnico para submissão ao Encontro de Iniciação Científica e Tecnológica da UFU", styles['TabelaCorpo']), Paragraph("1", styles['TabelaCorpoCentro'])],
        [Paragraph("Alunos de Graduação Envolvidos", styles['TabelaCorpo']), Paragraph("Estudante bolsista executor do plano de iniciação tecnológica", styles['TabelaCorpo']), Paragraph("1", styles['TabelaCorpoCentro'])],
        [Paragraph("Equipamentos Adquiridos / Utilizados", styles['TabelaCorpo']), Paragraph("Bancada de ensaios, microcontrolador ESP32, sensores ambientais e de solo", styles['TabelaCorpo']), Paragraph("Kit completo", styles['TabelaCorpoCentro'])],
    ]
    t_inv = Table(t_inv_data, colWidths=[5.5 * cm, 10.5 * cm, 2.0 * cm])
    t_inv.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,0), colors.HexColor("#0f3460")),
        ('GRID', (0,0), (-1,-1), 0.5, colors.HexColor("#CBD5E0")),
        ('VALIGN', (0,0), (-1,-1), 'MIDDLE'),
        ('TOPPADDING', (0,0), (-1,-1), 3),
        ('BOTTOMPADDING', (0,0), (-1,-1), 3),
    ]))
    story.append(t_inv)
    story.append(Spacer(1, 0.3 * cm))

    # 6. Divulgação Científica
    story.append(criar_faixa_secao("6. DIVULGAÇÃO CIENTÍFICA (até 250 palavras)", styles))
    txt_div_real = (
        "A divulgação técnico-científica compreendeu a elaboração de documentação completa de engenharia de software "
        "e hardware em repositório aberto, tornando acessíveis os diagramas de pinagem, equações físicas implementadas "
        "e procedimentos de calibração. Os resultados foram consolidados em relatório técnico formal e material audiovisual "
        "(vídeo pitch) para apresentação na Mostra de Iniciação Científica e Tecnológica da UFU, servindo como material didático "
        "para disciplinas de graduação em instrumentação e sistemas embarcados."
    )
    story.append(criar_bloco_campo("6.1. Formas de Divulgação Científica Realizadas e Respectivo Impacto", txt_div_real, styles))
    story.append(Spacer(1, 0.3 * cm))

    # 7. Premiações / Resumo Científico
    story.append(criar_faixa_secao("7. PREMIAÇÕES / RESUMO CIENTÍFICO DA PESQUISA (até 500 palavras)", styles))
    txt_premio = (
        "O Projeto GAIA desenvolveu uma estação agrometeorológica inteligente e acessível que transforma a coleta de dados de campo "
        "em uma ferramenta ativa de tomada de decisão agronômica. Enquanto soluções comerciais tradicionais demandam investimentos "
        "elevados e restringem o processamento a servidores proprietários em nuvem, o GAIA introduz computação na borda (Edge Computing) "
        "em hardware aberto de alta disponibilidade (ESP32).<br/><br/>"
        "O sistema integra sensores ambientais de temperatura, umidade, pressão barométrica, radiação solar e propriedades térmicas e "
        "hídricas do solo, efetuando calibração cruzada instantânea e gerando no próprio microcontrolador variáveis de alto impacto: "
        "Déficit de Pressão de Vapor (VPD) estomático, Evapotranspiração de Referência FAO-56, Radiação Fotossinteticamente Ativa (PAR/PPFD), "
        "risco de doenças fúngicas por acúmulo contínuo de horas de folha molhada e índice de estresse térmico em animais de produção (ITU).<br/><br/>"
        "Dentre os diferenciais de engenharia, destacam-se a validação temporal atômica via NTP dispensando RTC físico; comunicação Wi-Fi "
        "criptografada por hardware com AES-128-CBC; tolerância a falhas com fila offline em cartão SD (Store-and-Forward); preservação de séries "
        "históricas na memória estática RTC RAM durante o Deep Sleep sem desgaste de memória Flash; e Portal Captivo autônomo para diagnóstico "
        "em tempo real pelo smartphone. O trabalho consolida uma contribuição de alto mérito tecnológico, aliando rigor metodológico, "
        "sustentabilidade no uso da água e democratização da Agricultura 4.0 para pequenos e médios produtores rurais."
    )
    story.append(criar_bloco_campo("7.1. Resumo Científico da Pesquisa para Avaliação em Premiações", txt_premio, styles))

    return story


# =============================================================================
# FUNÇÃO PRINCIPAL DE GERAÇÃO
# =============================================================================
def gerar_todos_pdfs():
    base_dir = os.path.dirname(os.path.abspath(__file__))
    styles = criar_estilos()
    
    # 1. Gerar Anexo I
    pdf_anexo_i = os.path.join(base_dir, "Projeto_GAIA_Anexo_I_Proposta_Pesquisa_UFU.pdf")
    doc_i = SimpleDocTemplate(
        pdf_anexo_i,
        pagesize=A4,
        leftMargin=1.5 * cm,
        rightMargin=1.5 * cm,
        topMargin=1.5 * cm,
        bottomMargin=1.8 * cm
    )
    doc_i.build(construir_anexo_i(styles), canvasmaker=NumberedCanvas)
    print(f" -> Anexo I gerado com sucesso: {pdf_anexo_i}")

    # 2. Gerar Anexo IV
    pdf_anexo_iv = os.path.join(base_dir, "Projeto_GAIA_Anexo_IV_Relatorio_Tecnico_UFU.pdf")
    doc_iv = SimpleDocTemplate(
        pdf_anexo_iv,
        pagesize=A4,
        leftMargin=1.5 * cm,
        rightMargin=1.5 * cm,
        topMargin=1.5 * cm,
        bottomMargin=1.8 * cm
    )
    doc_iv.build(construir_anexo_iv(styles), canvasmaker=NumberedCanvas)
    print(f" -> Anexo IV gerado com sucesso: {pdf_anexo_iv}")

    # 3. Gerar Documento Completo Unificado
    pdf_completo = os.path.join(base_dir, "Projeto_GAIA_Proposta_e_Relatorio_UFU_Completo.pdf")
    doc_comp = SimpleDocTemplate(
        pdf_completo,
        pagesize=A4,
        leftMargin=1.5 * cm,
        rightMargin=1.5 * cm,
        topMargin=1.5 * cm,
        bottomMargin=1.8 * cm
    )
    story_comp = []
    story_comp.extend(construir_anexo_i(styles))
    story_comp.append(PageBreak())
    story_comp.extend(construir_anexo_iv(styles))
    doc_comp.build(story_comp, canvasmaker=NumberedCanvas)
    print(f" -> Documento completo unificado gerado: {pdf_completo}")


if __name__ == "__main__":
    gerar_todos_pdfs()
