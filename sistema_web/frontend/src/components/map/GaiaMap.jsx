import React, { useEffect, useRef, useState } from 'react';
import L from 'leaflet';
import 'leaflet/dist/leaflet.css';
import { createStationMarkerIcon } from './StationMarker';
import MapToolbar from './MapToolbar';

const TILE_LAYERS = {
  satellite: {
    url: 'https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}',
    attribution: '&copy; Esri, Maxar, Earthstar Geographics'
  },
  dark: {
    url: 'https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png',
    attribution: '&copy; OpenStreetMap contributors &copy; CARTO'
  },
  topo: {
    url: 'https://{s}.tile.opentopomap.org/{z}/{x}/{y}.png',
    attribution: '&copy; OpenTopoMap contributors'
  }
};

const DEFAULT_CENTER = [-18.7265, -47.4985]; // Monte Carmelo - MG
const DEFAULT_ZOOM = 13;

export default function GaiaMap({
  estacoes,
  selectedStation,
  onSelectStation,
  statusFilter,
  setStatusFilter,
  cropFilter,
  setCropFilter,
  cultures
}) {
  const mapContainerRef = useRef(null);
  const mapInstanceRef = useRef(null);
  const tileLayerRef = useRef(null);
  const markersRef = useRef({});
  const [currentLayer, setCurrentLayer] = useState('satellite');
  const [cursorCoords, setCursorCoords] = useState(null);

  // Inicialização do Mapa Leaflet
  useEffect(() => {
    if (!mapContainerRef.current || mapInstanceRef.current) return;

    const initialCenter = selectedStation?.latitude && selectedStation?.longitude
      ? [parseFloat(selectedStation.latitude), parseFloat(selectedStation.longitude)]
      : DEFAULT_CENTER;

    const map = L.map(mapContainerRef.current, {
      center: initialCenter,
      zoom: DEFAULT_ZOOM,
      zoomControl: false,
      attributionControl: false
    });

    // Zoom control reposicionado para canto inferior direito
    L.control.zoom({ position: 'bottomright' }).addTo(map);

    // Escala métrica no canto inferior esquerdo
    L.control.scale({ imperial: false, position: 'bottomleft' }).addTo(map);

    // Camada inicial de azulejos
    const tileConfig = TILE_LAYERS[currentLayer] || TILE_LAYERS.satellite;
    tileLayerRef.current = L.tileLayer(tileConfig.url, {
      maxZoom: 19,
      attribution: tileConfig.attribution
    }).addTo(map);

    // Rastreamento de coordenadas do cursor
    map.on('mousemove', (e) => {
      setCursorCoords({ lat: e.latlng.lat, lng: e.latlng.lng });
    });

    mapInstanceRef.current = map;

    return () => {
      map.remove();
      mapInstanceRef.current = null;
    };
  }, []);

  // Alternância de Camadas Cartográficas
  useEffect(() => {
    if (!mapInstanceRef.current || !tileLayerRef.current) return;

    const tileConfig = TILE_LAYERS[currentLayer] || TILE_LAYERS.satellite;
    mapInstanceRef.current.removeLayer(tileLayerRef.current);
    tileLayerRef.current = L.tileLayer(tileConfig.url, {
      maxZoom: 19,
      attribution: tileConfig.attribution
    }).addTo(mapInstanceRef.current);
  }, [currentLayer]);

  // Atualização dos Marcadores de Estação
  useEffect(() => {
    const map = mapInstanceRef.current;
    if (!map) return;

    // Limpar marcadores anteriores
    Object.values(markersRef.current).forEach(marker => map.removeLayer(marker));
    markersRef.current = {};

    estacoes.forEach(st => {
      if (!st.latitude || !st.longitude) return;

      const lat = parseFloat(st.latitude);
      const lng = parseFloat(st.longitude);
      if (isNaN(lat) || isNaN(lng)) return;

      const isSelected = selectedStation?.cod_estacao === st.cod_estacao;
      const icon = createStationMarkerIcon(st, isSelected);

      const marker = L.marker([lat, lng], { icon, zIndexOffset: isSelected ? 1000 : 0 });

      // Tooltip Técnico Rápido ao Hover
      const tempAr = st.temp_ref || st.temp_dht || '--';
      const umidAr = st.umidade || '--';
      const umidSolo = st.umidade_solo !== null && st.umidade_solo !== undefined ? `${st.umidade_solo}%` : '--';
      const vpd = st.vpd ? `${st.vpd} kPa` : '--';

      const tooltipContent = `
        <div style="font-family: monospace; font-size: 11px; padding: 4px 6px; background: #09090b; color: #f4f4f5; border: 1px solid #27272a; border-radius: 4px; box-shadow: 0 4px 12px rgba(0,0,0,0.8);">
          <div style="font-weight: bold; color: #34d399; margin-bottom: 2px;">${st.cod_estacao} • ${st.nome || 'Estação'}</div>
          <div style="color: #a1a1aa; font-size: 10px;">${st.cultura || 'Agrícola'} • ${st.status}</div>
          <div style="margin-top: 4px; display: grid; grid-template-columns: 1fr 1fr; gap: 4px; font-size: 10px; border-top: 1px solid #27272a; padding-top: 4px;">
            <div>Ar: <span style="color:#fff;">${tempAr}°C</span></div>
            <div>Umid: <span style="color:#fff;">${umidAr}%</span></div>
            <div>Solo: <span style="color:#38bdf8;">${umidSolo}</span></div>
            <div>VPD: <span style="color:#fbbf24;">${vpd}</span></div>
          </div>
        </div>
      `;

      marker.bindTooltip(tooltipContent, {
        direction: 'top',
        offset: [0, -48],
        opacity: 1,
        className: 'gaia-leaflet-tooltip'
      });

      marker.on('click', () => {
        onSelectStation(st);
      });

      marker.addTo(map);
      markersRef.current[st.cod_estacao] = marker;
    });
  }, [estacoes, selectedStation, onSelectStation]);

  // Pan suave quando estação selecionada mudar
  useEffect(() => {
    const map = mapInstanceRef.current;
    if (!map || !selectedStation?.latitude || !selectedStation?.longitude) return;

    const lat = parseFloat(selectedStation.latitude);
    const lng = parseFloat(selectedStation.longitude);
    if (!isNaN(lat) && !isNaN(lng)) {
      map.flyTo([lat, lng], 15, { duration: 1.2 });
    }
  }, [selectedStation?.cod_estacao]);

  const handleRecenter = () => {
    const map = mapInstanceRef.current;
    if (!map) return;

    if (selectedStation?.latitude && selectedStation?.longitude) {
      map.flyTo([parseFloat(selectedStation.latitude), parseFloat(selectedStation.longitude)], 15, { duration: 1 });
    } else if (estacoes.length > 0 && estacoes[0].latitude && estacoes[0].longitude) {
      map.flyTo([parseFloat(estacoes[0].latitude), parseFloat(estacoes[0].longitude)], 14, { duration: 1 });
    } else {
      map.flyTo(DEFAULT_CENTER, DEFAULT_ZOOM, { duration: 1 });
    }
  };

  return (
    <div className="relative w-full h-full overflow-hidden bg-zinc-950">
      <MapToolbar
        currentLayer={currentLayer}
        setCurrentLayer={setCurrentLayer}
        onRecenter={handleRecenter}
        statusFilter={statusFilter}
        setStatusFilter={setStatusFilter}
        cropFilter={cropFilter}
        setCropFilter={setCropFilter}
        cultures={cultures}
        cursorCoords={cursorCoords}
      />
      <div ref={mapContainerRef} className="w-full h-full z-0" />
    </div>
  );
}
