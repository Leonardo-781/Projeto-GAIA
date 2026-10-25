import React from 'react';
import {
  Chart as ChartJS,
  registerables
} from 'chart.js';
import { Line, Bar } from 'react-chartjs-2';
import {
  chartGridColor,
  chartBorderColor,
  chartTextColor,
  getBasePlugins,
  formatTemporalLabels
} from '../../utils/chartConfigs';

ChartJS.register(...registerables);

export default function SubsystemCharts({ history, selectedPeriod }) {
  if (!history || history.length === 0) {
    return (
      <div className="p-10 text-center bg-zinc-900/30 border border-zinc-800/80 rounded-lg text-zinc-400 font-mono text-xs flex flex-col items-center justify-center gap-2">
        <span className="text-zinc-300 font-semibold uppercase tracking-wider">Aguardando Pacotes Telemétricos de Campo</span>
        <span className="text-[11px] text-zinc-500 max-w-md">Nenhuma telemetria registrada para este período. Os gráficos serão gerados automaticamente conforme as leituras reais forem transmitidas pela sonda.</span>
      </div>
    );
  }

  const labels = formatTemporalLabels(history, selectedPeriod);
  const plugins = getBasePlugins(history);

  const baseScales = {
    x: {
      grid: { color: chartGridColor, borderColor: chartBorderColor },
      ticks: { color: chartTextColor, font: { size: 10, family: 'monospace' }, maxRotation: 0, autoSkip: true, maxTicksLimit: 8 }
    }
  };

  // 1. Atmosfera: Temp Ar + Umidade Ar
  const dataAtmosfera = {
    labels,
    datasets: [
      {
        type: 'line',
        label: 'Temp Ar (°C)',
        data: history.map(h => h.temp_ref || h.temp_dht || null),
        borderColor: '#f43f5e',
        backgroundColor: 'rgba(244, 63, 94, 0.1)',
        borderWidth: 2,
        tension: 0.3,
        pointRadius: history.length > 50 ? 0 : 2,
        yAxisID: 'yTemp'
      },
      {
        type: 'line',
        label: 'Umid Ar (%)',
        data: history.map(h => h.umidade || null),
        borderColor: '#38bdf8',
        backgroundColor: 'rgba(56, 189, 248, 0.08)',
        fill: true,
        borderWidth: 1.5,
        tension: 0.3,
        pointRadius: 0,
        yAxisID: 'yUmid'
      }
    ]
  };

  const optionsAtmosfera = {
    responsive: true,
    maintainAspectRatio: false,
    plugins,
    scales: {
      ...baseScales,
      yTemp: {
        type: 'linear',
        position: 'left',
        grid: { color: chartGridColor },
        ticks: { color: '#f43f5e', font: { size: 10, family: 'monospace' } }
      },
      yUmid: {
        type: 'linear',
        position: 'right',
        grid: { drawOnChartArea: false },
        ticks: { color: '#38bdf8', font: { size: 10, family: 'monospace' } },
        min: 0,
        max: 100
      }
    }
  };

  // 2. Solo: Umidade do Solo + Temp do Solo
  const dataSolo = {
    labels,
    datasets: [
      {
        label: 'Umid Solo (%)',
        data: history.map(h => (h.umidade_solo !== null && h.umidade_solo !== undefined) ? h.umidade_solo : null),
        borderColor: '#10b981',
        backgroundColor: 'rgba(16, 185, 129, 0.15)',
        fill: true,
        borderWidth: 2,
        tension: 0.3,
        pointRadius: history.length > 50 ? 0 : 2,
        yAxisID: 'yUmidSolo'
      },
      {
        label: 'Temp Solo (°C)',
        data: history.map(h => h.temp_solo || null),
        borderColor: '#f59e0b',
        borderWidth: 1.5,
        tension: 0.3,
        pointRadius: 0,
        yAxisID: 'yTempSolo'
      }
    ]
  };

  const optionsSolo = {
    responsive: true,
    maintainAspectRatio: false,
    plugins,
    scales: {
      ...baseScales,
      yUmidSolo: {
        type: 'linear',
        position: 'left',
        grid: { color: chartGridColor },
        ticks: { color: '#10b981', font: { size: 10, family: 'monospace' } },
        min: 0,
        max: 100
      },
      yTempSolo: {
        type: 'linear',
        position: 'right',
        grid: { drawOnChartArea: false },
        ticks: { color: '#f59e0b', font: { size: 10, family: 'monospace' } }
      }
    }
  };

  // 3. Radiação Solar & PAR
  const dataRadiacao = {
    labels,
    datasets: [
      {
        label: 'Radiação Solar (W/m²)',
        data: history.map(h => h.rs || null),
        borderColor: '#f59e0b',
        backgroundColor: 'rgba(245, 158, 11, 0.15)',
        fill: true,
        borderWidth: 2,
        tension: 0.2,
        pointRadius: 0,
        yAxisID: 'yRs'
      },
      {
        label: 'PAR (μmol/m²/s)',
        data: history.map(h => h.par || null),
        borderColor: '#fbbf24',
        borderWidth: 1.5,
        tension: 0.2,
        pointRadius: 0,
        yAxisID: 'yPar'
      }
    ]
  };

  const optionsRadiacao = {
    responsive: true,
    maintainAspectRatio: false,
    plugins,
    scales: {
      ...baseScales,
      yRs: {
        type: 'linear',
        position: 'left',
        grid: { color: chartGridColor },
        ticks: { color: '#f59e0b', font: { size: 10, family: 'monospace' } },
        min: 0
      },
      yPar: {
        type: 'linear',
        position: 'right',
        grid: { drawOnChartArea: false },
        ticks: { color: '#fbbf24', font: { size: 10, family: 'monospace' } },
        min: 0
      }
    }
  };

  // 4. Pressão Atmosférica & Delta P
  const dataPressao = {
    labels,
    datasets: [
      {
        type: 'line',
        label: 'Pressão (hPa)',
        data: history.map(h => h.pressao || null),
        borderColor: '#c084fc',
        backgroundColor: 'rgba(192, 132, 252, 0.05)',
        borderWidth: 2,
        tension: 0.2,
        pointRadius: 0,
        yAxisID: 'yPressao'
      },
      {
        type: 'bar',
        label: 'Tendência ΔP (hPa)',
        data: history.map(h => h.dp || 0),
        backgroundColor: history.map(h => (h.dp < -2 ? '#f43f5e' : '#a1a1aa')),
        yAxisID: 'yDelta'
      }
    ]
  };

  const optionsPressao = {
    responsive: true,
    maintainAspectRatio: false,
    plugins,
    scales: {
      ...baseScales,
      yPressao: {
        type: 'linear',
        position: 'left',
        grid: { color: chartGridColor },
        ticks: { color: '#c084fc', font: { size: 10, family: 'monospace' } }
      },
      yDelta: {
        type: 'linear',
        position: 'right',
        grid: { drawOnChartArea: false },
        ticks: { color: '#a1a1aa', font: { size: 10, family: 'monospace' } }
      }
    }
  };

  // 5. Riscos Fisiológicos & Bioclima (VPD + ITU)
  const dataRiscos = {
    labels,
    datasets: [
      {
        label: 'VPD Estomático (kPa)',
        data: history.map(h => h.vpd || null),
        borderColor: '#fb923c',
        backgroundColor: 'rgba(251, 146, 60, 0.1)',
        fill: true,
        borderWidth: 2,
        tension: 0.3,
        pointRadius: 0,
        yAxisID: 'yVpd'
      },
      {
        label: 'ITU Bovino/Clima',
        data: history.map(h => h.itu || null),
        borderColor: '#38bdf8',
        borderWidth: 1.5,
        tension: 0.3,
        pointRadius: 0,
        yAxisID: 'yItu'
      }
    ]
  };

  const optionsRiscos = {
    responsive: true,
    maintainAspectRatio: false,
    plugins,
    scales: {
      ...baseScales,
      yVpd: {
        type: 'linear',
        position: 'left',
        grid: { color: chartGridColor },
        ticks: { color: '#fb923c', font: { size: 10, family: 'monospace' } },
        min: 0
      },
      yItu: {
        type: 'linear',
        position: 'right',
        grid: { drawOnChartArea: false },
        ticks: { color: '#38bdf8', font: { size: 10, family: 'monospace' } }
      }
    }
  };

  // 6. Hardware & Rede (Heap RAM + Qualidade %)
  const dataHardware = {
    labels,
    datasets: [
      {
        label: 'Heap RAM Livre (KB)',
        data: history.map(h => h.heap_mod || null),
        borderColor: '#2dd4bf',
        backgroundColor: 'rgba(45, 212, 191, 0.1)',
        fill: true,
        borderWidth: 2,
        tension: 0.2,
        pointRadius: 0,
        yAxisID: 'yHeap'
      },
      {
        label: 'Qualidade Telemetria (%)',
        data: history.map(h => h.qualidade || 100),
        borderColor: '#71717a',
        borderWidth: 1.5,
        tension: 0.2,
        pointRadius: 0,
        yAxisID: 'yQual'
      }
    ]
  };

  const optionsHardware = {
    responsive: true,
    maintainAspectRatio: false,
    plugins,
    scales: {
      ...baseScales,
      yHeap: {
        type: 'linear',
        position: 'left',
        grid: { color: chartGridColor },
        ticks: { color: '#2dd4bf', font: { size: 10, family: 'monospace' } }
      },
      yQual: {
        type: 'linear',
        position: 'right',
        grid: { drawOnChartArea: false },
        ticks: { color: '#71717a', font: { size: 10, family: 'monospace' } },
        min: 0,
        max: 100
      }
    }
  };

  return (
    <div className="grid grid-cols-1 lg:grid-cols-2 gap-4">
      
      {/* 1. Microclima Atmosférico */}
      <div className="bg-zinc-900/70 border border-zinc-800 rounded-lg p-3 flex flex-col h-72">
        <div className="flex items-center justify-between border-b border-zinc-800 pb-2 mb-2">
          <span className="font-mono text-xs font-bold text-zinc-200">
            1. MICROCLIMA ATMOSFÉRICO (TEMP & UMIDADE)
          </span>
          <span className="text-[10px] font-mono text-zinc-500">Dual Axis</span>
        </div>
        <div className="flex-1 min-h-0">
          <Line data={dataAtmosfera} options={optionsAtmosfera} />
        </div>
      </div>

      {/* 2. Perfil de Solo */}
      <div className="bg-zinc-900/70 border border-zinc-800 rounded-lg p-3 flex flex-col h-72">
        <div className="flex items-center justify-between border-b border-zinc-800 pb-2 mb-2">
          <span className="font-mono text-xs font-bold text-zinc-200">
            2. PERFIL DE SOLO (UMIDADE & TEMPERATURA)
          </span>
          <span className="text-[10px] font-mono text-zinc-500">Sonda 20cm</span>
        </div>
        <div className="flex-1 min-h-0">
          <Line data={dataSolo} options={optionsSolo} />
        </div>
      </div>

      {/* 3. Radiação Solar & PAR */}
      <div className="bg-zinc-900/70 border border-zinc-800 rounded-lg p-3 flex flex-col h-72">
        <div className="flex items-center justify-between border-b border-zinc-800 pb-2 mb-2">
          <span className="font-mono text-xs font-bold text-zinc-200">
            3. RADIAÇÃO GLOBAL (Rs) & FÓTONS (PAR)
          </span>
          <span className="text-[10px] font-mono text-zinc-500">Fotoperíodo</span>
        </div>
        <div className="flex-1 min-h-0">
          <Line data={dataRadiacao} options={optionsRadiacao} />
        </div>
      </div>

      {/* 4. Pressão & Tendência Barométrica */}
      <div className="bg-zinc-900/70 border border-zinc-800 rounded-lg p-3 flex flex-col h-72">
        <div className="flex items-center justify-between border-b border-zinc-800 pb-2 mb-2">
          <span className="font-mono text-xs font-bold text-zinc-200">
            4. PRESSÃO ATMOSFÉRICA & TENDÊNCIA (ΔP)
          </span>
          <span className="text-[10px] font-mono text-zinc-500">Detecção de Frente</span>
        </div>
        <div className="flex-1 min-h-0">
          <Line data={dataPressao} options={optionsPressao} />
        </div>
      </div>

      {/* 5. Riscos Fisiológicos (VPD & ITU) */}
      <div className="bg-zinc-900/70 border border-zinc-800 rounded-lg p-3 flex flex-col h-72">
        <div className="flex items-center justify-between border-b border-zinc-800 pb-2 mb-2">
          <span className="font-mono text-xs font-bold text-zinc-200">
            5. BIOCLIMA: VPD ESTOMÁTICO & ITU
          </span>
          <span className="text-[10px] font-mono text-zinc-500">Fisiologia</span>
        </div>
        <div className="flex-1 min-h-0">
          <Line data={dataRiscos} options={optionsRiscos} />
        </div>
      </div>

      {/* 6. Hardware & Rede */}
      <div className="bg-zinc-900/70 border border-zinc-800 rounded-lg p-3 flex flex-col h-72">
        <div className="flex items-center justify-between border-b border-zinc-800 pb-2 mb-2">
          <span className="font-mono text-xs font-bold text-zinc-200">
            6. TELEMETRIA DO HARDWARE (HEAP & QUALIDADE)
          </span>
          <span className="text-[10px] font-mono text-zinc-500">ESP32-C3</span>
        </div>
        <div className="flex-1 min-h-0">
          <Line data={dataHardware} options={optionsHardware} />
        </div>
      </div>

    </div>
  );
}
