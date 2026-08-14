// Main orchestration — wave optics instrument
// All UI via Pretext Canvas2D overlay — zero DOM controls
import { WaveEngine } from './wave-engine.js';
import { UIOverlay } from './ui-overlay.js';
import { AudioEngine } from './audio-engine.js';
import { KnobPanel } from './knob-panel.js';

let waveEngine;
let uiOverlay;
let knobPanel;
let audioEngine;
let running = false;
let simPaused = false;
let simFrozen = false;
let bgPreview = false; // background wave preview on loading screen
let lastTime = 0;
let midiConnected = false;

// Scene memory (4 slots)
let scenes = [null, null, null, null];

// Optics elements
let opticsElements = [];
let currentPreset = 'single-slit';

// ─── Note frequency helper ───
// MIDI note number to Hz
const NOTE_TO_HZ = (note) => 440 * Math.pow(2, (note - 69) / 12);

// Pentatonic note rows for QWERTY (Feature 2)
// C pentatonic: C D E G A (MIDI offsets 0 2 4 7 9)
// Bottom row: Z X C V B N M , . / → C3 D3 E3 G3 A3 C4 D4 E4 G4 A4
// Middle row: A S D F G H J K L ; → C4 D4 E4 G4 A4 C5 D5 E5 G5 A5
// Top row:    Q W E R T Y U I O P → C5 D5 E5 G5 A5 C6 D6 E6 G6 A6
const PENTA_OFFSETS = [0, 2, 4, 7, 9]; // C D E G A
function buildPentaRow(octaveStart) {
  const notes = [];
  for (let rep = 0; rep < 2; rep++) {
    for (let i = 0; i < 5; i++) {
      notes.push(NOTE_TO_HZ(12 * (octaveStart + rep) + PENTA_OFFSETS[i] + 12)); // +12 = MIDI C0 is 12
    }
  }
  return notes;
}
// MIDI note: C3 = 48, C4 = 60, C5 = 72, C6 = 84
const QWERTY_ROW_Z = buildPentaRow(3); // C3..A4
const QWERTY_ROW_A = buildPentaRow(4); // C4..A5
const QWERTY_ROW_Q = buildPentaRow(5); // C5..A6

const QWERTY_MAP = {
  'KeyZ': QWERTY_ROW_Z[0], 'KeyX': QWERTY_ROW_Z[1], 'KeyC': QWERTY_ROW_Z[2],
  'KeyV': QWERTY_ROW_Z[3], 'KeyB': QWERTY_ROW_Z[4], 'KeyN': QWERTY_ROW_Z[5],
  'KeyM': QWERTY_ROW_Z[6], 'Comma': QWERTY_ROW_Z[7], 'Period': QWERTY_ROW_Z[8],
  'Slash': QWERTY_ROW_Z[9],

  'KeyA': QWERTY_ROW_A[0], 'KeyS': QWERTY_ROW_A[1], 'KeyD': QWERTY_ROW_A[2],
  'KeyF': QWERTY_ROW_A[3], 'KeyG': QWERTY_ROW_A[4], 'KeyH': QWERTY_ROW_A[5],
  'KeyJ': QWERTY_ROW_A[6], 'KeyK': QWERTY_ROW_A[7], 'KeyL': QWERTY_ROW_A[8],
  'Semicolon': QWERTY_ROW_A[9],

  'KeyQ': QWERTY_ROW_Q[0], 'KeyW': QWERTY_ROW_Q[1], 'KeyE': QWERTY_ROW_Q[2],
  'KeyR': QWERTY_ROW_Q[3], 'KeyT': QWERTY_ROW_Q[4], 'KeyY': QWERTY_ROW_Q[5],
  'KeyU': QWERTY_ROW_Q[6], 'KeyI': QWERTY_ROW_Q[7], 'KeyO': QWERTY_ROW_Q[8],
  'KeyP': QWERTY_ROW_Q[9],
};

const PRESETS = {
  'double-slit': {
    label: 'Double Slit',
    sourceX: 0.06,
    freq: 93.0,
    speed: 0.9,
    sens: 4.5,
    sourceType: 'plane',
    slitConfig: { label: 'Gap', min: 0.02, max: 0.15, step: 0.005, value: 0.105 },
    slitWidth: 0.060,
    elements: () => {
      const wallX = 0.15, slitWidth = 0.060, slitSep = 0.105, wallThick = 0.006;
      return [
        { type: 'wall', x: wallX, y: 0, w: wallThick, h: 0.5 - slitSep / 2 - slitWidth / 2 },
        { type: 'wall', x: wallX, y: 0.5 - slitSep / 2 + slitWidth / 2, w: wallThick, h: slitSep - slitWidth },
        { type: 'wall', x: wallX, y: 0.5 + slitSep / 2 + slitWidth / 2, w: wallThick, h: 0.5 - slitSep / 2 - slitWidth / 2 },
      ];
    }
  },
  'single-slit': {
    label: 'Single Slit',
    sourceX: 0.06,
    freq: 60.0,
    sourceType: 'plane',
    slitConfig: { label: 'Slit Width', min: 0.008, max: 0.08, step: 0.002, value: 0.03 },
    elements: () => {
      const wallX = 0.15, slitWidth = 0.03, wallThick = 0.006;
      return [
        { type: 'wall', x: wallX, y: 0, w: wallThick, h: 0.5 - slitWidth / 2 },
        { type: 'wall', x: wallX, y: 0.5 + slitWidth / 2, w: wallThick, h: 0.5 - slitWidth / 2 },
      ];
    }
  },
  'lens': {
    label: 'Convex Lens',
    sourceX: 0.06,
    freq: 35.0,
    sourceType: 'plane',
    slitConfig: { label: 'Curvature', min: 0.3, max: 0.65, step: 0.02, value: 0.45 },
    elements: () => [{ type: 'lens', cx: 0.20, cy: 0.5, radius: 0.12, ior: 0.45 }]
  },
  'corner': {
    label: 'Diffraction',
    sourceX: 0.06,
    freq: 60.0,
    sourceType: 'plane',
    slitConfig: { label: 'Aperture', min: 0.02, max: 0.15, step: 0.005, value: 0.06 },
    elements: () => {
      const wallX = 0.15, wallThick = 0.006;
      return [{ type: 'wall', x: wallX, y: 0, w: wallThick, h: 0.42 }];
    }
  },
  'draw': {
    label: 'Draw',
    sourceX: 0.06,
    freq: 60.0,
    sourceType: 'plane',
    slitConfig: { label: 'Brush', min: 0.002, max: 0.02, step: 0.001, value: 0.006 },
    elements: () => [] // empty — user draws walls
  },
  'mach-zehnder': {
    label: 'Mach-Zehnder',
    sourceX: 0.06,
    freq: 60.0,
    sourceType: 'plane',
    slitConfig: { label: 'Path Δ', min: 0.0, max: 0.08, step: 0.002, value: 0.0 },
    elements: () => {
      // Mach-Zehnder interferometer:
      // Two beam splitters (partial speed) + two mirrors (walls at 45°)
      // Beam enters left, splits at BS1, takes two paths, recombines at BS2
      const wallThick = 0.006;
      return [
        // Beam splitter 1
        { type: 'beamsplitter', cx: 0.18, cy: 0.5, angle: 0.785, length: 0.10, thickness: 0.008, ior: 0.55 },
        // Mirror 1 (top path)
        { type: 'mirror', cx: 0.18, cy: 0.35, angle: -0.785, length: 0.08, thickness: wallThick },
        // Mirror 2 (bottom path)
        { type: 'mirror', cx: 0.45, cy: 0.5, angle: -0.785, length: 0.08, thickness: wallThick },
        // Beam splitter 2 (recombines)
        { type: 'beamsplitter', cx: 0.45, cy: 0.35, angle: 0.785, length: 0.10, thickness: 0.008, ior: 0.55 },
        // Guide walls
        { type: 'wall', x: 0.13, y: 0.0, w: wallThick, h: 0.28 },
        { type: 'wall', x: 0.13, y: 0.72, w: wallThick, h: 0.28 },
      ];
    }
  }
};

function init() {
  const waveCanvas = document.getElementById('wave-canvas');
  const uiCanvas = document.getElementById('ui-canvas');

  waveCanvas.style.width = '100%';
  waveCanvas.style.height = '100%';
  uiCanvas.style.width = '100%';
  uiCanvas.style.height = '100%';

  waveEngine = new WaveEngine(waveCanvas);
  // When the engine internally resizes (DPR/layout change), its speed texture
  // is wiped — rebuild optics so walls don't silently disappear.
  waveEngine.onResize = () => buildSpeedMap();

  // Knob panel below wave field
  const knobCanvas = document.getElementById('knob-canvas');
  knobPanel = new KnobPanel(knobCanvas, {
    onSpeed: (val) => { waveEngine.speedMultiplier = val; },
    onFreq: (val) => { waveEngine.sourceFreq = val; },
    onSlit: (val) => {
      if (currentPreset === 'draw') return; // brush size — live-read in paintDot, no rebuild
      const w = knobPanel.knobs.find(k => k.id === 'slitW')?.value || 0.012;
      if (currentPreset === 'double-slit') rebuildDoubleSlit(val, w);
      else if (currentPreset === 'single-slit') rebuildSingleSlit(val);
      else if (currentPreset === 'lens') rebuildLens(val);
      buildSpeedMap();
    },
    onSlitWidth: (val) => {
      if (currentPreset === 'draw') return; // rebuild would clobber drawSpeedMap
      const gap = knobPanel.knobs.find(k => k.id === 'slit')?.value || 0.07;
      if (currentPreset === 'double-slit') rebuildDoubleSlit(gap, val);
      else if (currentPreset === 'single-slit') rebuildSingleSlit(val);
      buildSpeedMap();
    },
    onVolume: (val) => { if (audioEngine) audioEngine.setVolume(val); },
    onSensitivity: (val) => { waveEngine.detectorSensitivity = val; },
    onFilter: (val) => { if (audioEngine) audioEngine.setFilterCutoff(val); },
    onDryWet: (val) => { if (audioEngine) audioEngine.setDryWet(val); },
    onRelease: (val) => { waveEngine.envelopeRelease = val; },
  });

  uiOverlay = new UIOverlay(uiCanvas, {
    getDetectorX: () => waveEngine.detectorX,
    onStart: async () => {
      bgPreview = false;
      // Show controls
      document.getElementById('preset-canvas').style.display = 'block';
      document.getElementById('knob-canvas').style.display = 'block';
      running = true;
      lastTime = performance.now();
      waveEngine.reset();
      applyPreset(currentPreset);
      // Start with source off — user initiates via START or PULSE
      waveEngine.continuousSource = false;
      uiOverlay.sourceActive = false;
      // Start audio engine (requires user gesture)
      audioEngine = new AudioEngine();
      await audioEngine.start();
      initMIDI();
      requestAnimationFrame(loop);
    },
    onPreset: (id) => {
      if (id === currentPreset) return; // clicking active preset is a no-op (preserves drawing)
      currentPreset = id;
      eraserMode = false;
      if (uiOverlay) uiOverlay.eraserMode = false;
      waveEngine.reset();
      applyPreset(id);
    },
    onSpeed: (val) => {
      waveEngine.speedMultiplier = val;
    },
    onFreq: (val) => {
      waveEngine.sourceFreq = val;
    },
    onSlit: (val) => {
      if (currentPreset === 'double-slit') rebuildDoubleSlit(val);
      else if (currentPreset === 'single-slit') rebuildSingleSlit(val);
      else if (currentPreset === 'lens') rebuildLens(val);
      buildSpeedMap();
    },
    onVolume: (val) => {
      if (audioEngine) audioEngine.setVolume(val);
    },
    onSensitivity: (val) => {
      waveEngine.detectorSensitivity = val;
    },
    onFilter: (val) => {
      if (audioEngine) audioEngine.setFilterCutoff(val);
    },
    onDryWet: (val) => {
      if (audioEngine) audioEngine.setDryWet(val);
    },
    onLfoRate: (val) => {
      waveEngine.lfoRate = val;
    },
    onLfoDepth: (val) => {
      waveEngine.lfoDepth = val;
    },
    onRelease: (val) => {
      waveEngine.envelopeRelease = val;
    },
    onStopStart: (sourceActive) => {
      waveEngine.continuousSource = sourceActive;
    },
    onPlayPause: (simRunning) => {
      simPaused = !simRunning;
    },
    onFreeze: (frozen) => {
      simFrozen = frozen;
    },
    onReset: () => {
      // Preserve user-adjusted settings across reset
      const speed = waveEngine.speedMultiplier;
      const freq = waveEngine.sourceFreq;
      const savedDraw = drawSpeedMap; // preserve drawing in draw mode
      waveEngine.reset();
      applyPreset(currentPreset);
      // Restore drawing if in draw mode
      if (currentPreset === 'draw' && savedDraw) {
        drawSpeedMap = savedDraw;
        waveEngine.updateSpeedMap(drawSpeedMap);
      }
      // Restore knob settings
      waveEngine.speedMultiplier = speed;
      waveEngine.sourceFreq = freq;
      uiOverlay.speed = speed;
      uiOverlay.freq = freq;
    },
    onClearDraw: () => {
      clearDrawing();
    },
    onToggleEraser: () => {
      eraserMode = !eraserMode;
      if (uiOverlay) uiOverlay.eraserMode = eraserMode;
    },
    onPulse: () => {
      // Fire source for ~10ms then stop (Feature 6: Pulse uses 0.02)
      waveEngine.sourceAmplitude = 0.02;
      waveEngine.continuousSource = true;
      uiOverlay.sourceActive = true;
      setTimeout(() => {
        waveEngine.continuousSource = false;
        uiOverlay.sourceActive = false;
      }, 10);
    },
    onScale: (on) => {
      if (audioEngine) audioEngine.setScaleMode(on);
    },
    onDrone: (on) => {
      if (audioEngine) audioEngine.setDroneMode(on);
    },

    onRecord: (isRecording) => {
      if (!audioEngine) return;
      if (isRecording) {
        const waveCanvas = document.getElementById('wave-canvas');
        audioEngine.startRecording(waveCanvas);
      } else {
        audioEngine.stopRecording();
      }
    },
    onSceneSave: (idx) => {
      // Save current state to scene slot
      if (!knobPanel) return;
      const knobVals = {};
      for (const k of knobPanel.knobs) {
        knobVals[k.id] = k.value;
      }
      scenes[idx] = {
        preset: currentPreset,
        knobs: knobVals,
        lfoRate: waveEngine.lfoRate,
        lfoDepth: waveEngine.lfoDepth,
      };
      uiOverlay.scenes = scenes;
    },
    onSceneRecall: (idx) => {
      const scene = scenes[idx];
      if (!scene) return;
      // Apply preset
      currentPreset = scene.preset;
      eraserMode = false;
      if (uiOverlay) uiOverlay.eraserMode = false;
      uiOverlay.activePreset = scene.preset;
      waveEngine.reset();
      applyPreset(scene.preset);
      // Restore all knob values
      if (knobPanel && scene.knobs) {
        for (const [id, val] of Object.entries(scene.knobs)) {
          knobPanel.setKnobValue(id, val);
          knobPanel._fireCallback(id, val);
        }
      }
      // Restore LFO
      if (scene.lfoRate !== undefined) waveEngine.lfoRate = scene.lfoRate;
      if (scene.lfoDepth !== undefined) waveEngine.lfoDepth = scene.lfoDepth;
      buildSpeedMap();
    },
  });

  // Bind preset bar to its own canvas
  const presetCanvas = document.getElementById('preset-canvas');
  uiOverlay.bindPresetCanvas(presetCanvas);

  // Drawing mode
  initDrawMode();

  // Hide controls until user clicks to begin
  document.getElementById('preset-canvas').style.display = 'none';
  document.getElementById('knob-canvas').style.display = 'none';

  // Start background wave preview on loading screen — match the default preset
  // so the slit the user sees on load matches what they get after clicking START.
  bgPreview = true;
  applyPreset('single-slit');
  waveEngine.continuousSource = true;
  waveEngine.sourceAmplitude = 0.015; // slightly quieter than normal
  lastTime = performance.now();
  requestAnimationFrame(bgPreviewLoop);

  // Start UI render loop immediately (for click-start screen)
  requestAnimationFrame(uiLoop);
}

function bgPreviewLoop(now) {
  if (!bgPreview) return;
  const dt = Math.min((now - lastTime) / 1000, 0.05);
  lastTime = now;
  waveEngine.step(dt);
  requestAnimationFrame(bgPreviewLoop);
}

function uiLoop(now) {
  uiOverlay.draw(now);
  const pc = document.getElementById('preset-canvas');
  if (pc) uiOverlay.drawPresetBar(pc);
  if (knobPanel) knobPanel.draw();
  if (!running) {
    requestAnimationFrame(uiLoop);
  }
}

function applyPreset(name) {
  const preset = PRESETS[name];
  if (!preset) return;

  waveEngine.sourceX = preset.sourceX;
  waveEngine.sourceFreq = preset.freq;
  waveEngine.sourceType = preset.sourceType || 'plane';
  waveEngine.continuousSource = true;

  opticsElements = preset.elements();
  drawSpeedMap = null; // clear any freehand drawing
  buildSpeedMap();

  // Update UI slider config
  const sc = preset.slitConfig;
  uiOverlay.updateSlitConfig(sc.label, sc.min, sc.max, sc.step, sc.value);
  uiOverlay.freq = preset.freq;
  
  // Compute wavelength: λ = 2π·c/ω where c = sqrt(0.24) ≈ 0.49, ω = sourceFreq
  const c = Math.sqrt(0.24);
  const lambda = 2 * Math.PI * c / preset.freq;
  uiOverlay.wavelength = lambda;
  
  // Compute d/λ ratio for double-slit
  if (name === 'double-slit') {
    uiOverlay.dLambdaRatio = sc.value / lambda;
  } else {
    uiOverlay.dLambdaRatio = 0;
  }
  
  // Reset detector accumulation
  uiOverlay.detectorData = null;
  uiOverlay.detectorMax = 0.001;
  
  // Apply preset-specific speed & sensitivity
  if (preset.speed !== undefined) {
    waveEngine.speedMultiplier = preset.speed;
    uiOverlay.speed = preset.speed;
  }
  if (preset.sens !== undefined && uiOverlay) {
    uiOverlay.sensitivity = preset.sens;
  }

  // Sync knob values
  if (knobPanel) {
    knobPanel.setKnobValue('freq', preset.freq);
    if (preset.speed !== undefined) knobPanel.setKnobValue('speed', preset.speed);
    if (preset.sens !== undefined) knobPanel.setKnobValue('sens', preset.sens);
    const slitKnob = knobPanel.knobs.find(k => k.id === 'slit');
    if (slitKnob) {
      slitKnob.label = sc.label;
      slitKnob.min = sc.min;
      slitKnob.max = sc.max;
      slitKnob.step = sc.step;
      slitKnob.value = sc.value;
    }
    // Reset width knob for double-slit
    if (name === 'double-slit') {
      knobPanel.setKnobValue('slitW', preset.slitWidth || 0.012);
    }
  }
}

function rebuildDoubleSlit(sep, slitWidth) {
  slitWidth = slitWidth || 0.060;
  const wallX = 0.15, wallThick = 0.006;
  opticsElements = [
    { type: 'wall', x: wallX, y: 0, w: wallThick, h: 0.5 - sep / 2 - slitWidth / 2 },
    { type: 'wall', x: wallX, y: 0.5 - sep / 2 + slitWidth / 2, w: wallThick, h: sep - slitWidth },
    { type: 'wall', x: wallX, y: 0.5 + sep / 2 + slitWidth / 2, w: wallThick, h: 0.5 - sep / 2 - slitWidth / 2 },
  ];
}

function rebuildSingleSlit(width) {
  const wallX = 0.15, wallThick = 0.006;
  opticsElements = [
    { type: 'wall', x: wallX, y: 0, w: wallThick, h: 0.5 - width / 2 },
    { type: 'wall', x: wallX, y: 0.5 + width / 2, w: wallThick, h: 0.5 - width / 2 },
  ];
}

function rebuildLens(ior) {
  opticsElements = [{ type: 'lens', cx: 0.20, cy: 0.5, radius: 0.12, ior }];
}

function buildSpeedMap() {
  const w = waveEngine.width;
  const h = waveEngine.height;
  const data = new Float32Array(w * h);
  data.fill(1.0);

  for (const el of opticsElements) {
    if (el.type === 'wall') {
      const x0 = Math.floor(el.x * w);
      const y0 = Math.floor(el.y * h);
      const x1 = Math.floor((el.x + el.w) * w);
      const y1 = Math.floor((el.y + el.h) * h);
      for (let y = Math.max(0, y0); y < Math.min(h, y1); y++) {
        for (let x = Math.max(0, x0); x < Math.min(w, x1); x++) {
          data[y * w + x] = 0.0;
        }
      }
    } else if (el.type === 'lens') {
      const cx = Math.floor(el.cx * w);
      const cy = Math.floor(el.cy * h);
      const r = Math.floor(el.radius * Math.min(w, h));
      for (let y = Math.max(0, cy - r); y < Math.min(h, cy + r); y++) {
        for (let x = Math.max(0, cx - r); x < Math.min(w, cx + r); x++) {
          const dx = (x - cx) / r;
          const dy = (y - cy) / r;
          const dist = Math.sqrt(dx * dx + dy * dy);
          if (dist < 1.0) {
            const thickness = Math.sqrt(1.0 - dist * dist);
            data[y * w + x] = 1.0 - (1.0 - el.ior) * thickness;
          }
        }
      }
    } else if (el.type === 'prism') {
      const cx = Math.floor(el.cx * w);
      const cy = Math.floor(el.cy * h);
      const size = Math.floor(el.size * Math.min(w, h));
      for (let y = Math.max(0, cy - size); y < Math.min(h, cy + size); y++) {
        for (let x = Math.max(0, cx - size); x < Math.min(w, cx + size); x++) {
          const lx = (x - cx) / size;
          const ly = (y - cy) / size;
          if (ly > -0.577 && ly < 0.577 * (1.0 - lx) && ly > -0.577 * (1.0 - lx)) {
            if (lx > -0.5 && lx < 1.0) {
              const thickness = (lx + 0.5) / 1.5;
              data[y * w + x] = 1.0 - (1.0 - el.ior) * (0.3 + 0.7 * thickness);
            }
          }
        }
      }
    } else if (el.type === 'beamsplitter' || el.type === 'mirror') {
      // Diagonal line element (rotated rectangle)
      const cx = el.cx * w;
      const cy = el.cy * h;
      const len = el.length * Math.min(w, h);
      const thick = el.thickness * Math.min(w, h);
      const cosA = Math.cos(el.angle);
      const sinA = Math.sin(el.angle);
      const halfLen = len / 2;
      const halfThick = thick / 2;
      const radius = Math.ceil(Math.max(halfLen, halfThick) * 1.5);
      for (let y = Math.max(0, Math.floor(cy - radius)); y < Math.min(h, Math.ceil(cy + radius)); y++) {
        for (let x = Math.max(0, Math.floor(cx - radius)); x < Math.min(w, Math.ceil(cx + radius)); x++) {
          const dx = x - cx;
          const dy = y - cy;
          // Rotate into element's local frame
          const lx = dx * cosA + dy * sinA;
          const ly = -dx * sinA + dy * cosA;
          if (Math.abs(lx) < halfLen && Math.abs(ly) < halfThick) {
            if (el.type === 'mirror') {
              data[y * w + x] = 0.0; // wall
            } else {
              data[y * w + x] = el.ior; // partial speed for beam splitter
            }
          }
        }
      }
    }
  }

  waveEngine.updateSpeedMap(data);
}

let frameCount = 0;

function loop(now) {
  if (!running) return;

  const dt = Math.min((now - lastTime) / 1000, 0.05);
  lastTime = now;

  if (!simPaused) {
    // Step simulation (unless frozen)
    if (!simFrozen) {
      waveEngine.step(dt);
    }
    
    // Read 3 detector columns for stereo: L (0.88), C (0.92), R (0.96)
    frameCount++;
    if (frameCount % 3 === 0) {
      try {
        const detC = waveEngine.readDetectorColumnAt(0.92);
        uiOverlay.detectorData = detC.accum;
        uiOverlay.detectorInstant = detC.instant;
        if (audioEngine) {
          const detL = waveEngine.readDetectorColumnAt(0.88);
          const detR = waveEngine.readDetectorColumnAt(0.96);
          audioEngine.updateDetectorVoice(0, detL.instant); // left
          audioEngine.updateDetectorVoice(1, detC.instant); // center
          audioEngine.updateDetectorVoice(2, detR.instant); // right
        }
      } catch (e) { /* ignore readPixels failures */ }
    }
  }
  
  uiOverlay.draw(now);
  const pc = document.getElementById('preset-canvas');
  if (pc) uiOverlay.drawPresetBar(pc);
  knobPanel.draw();
  requestAnimationFrame(loop);
}

// Drawing state
let drawingActive = false;
let drawSpeedMap = null;
let lastDrawX = -1, lastDrawY = -1;
let eraserMode = false;

function clearDrawing() {
  drawSpeedMap = null;
  opticsElements = [];
  buildSpeedMap();
  waveEngine.reset();
  waveEngine.continuousSource = true;
  if (uiOverlay) uiOverlay.sourceActive = true;
}

function initDrawMode() {
  const uiCanvas = document.getElementById('ui-canvas');
  
  const getUV = (e) => {
    const rect = uiCanvas.getBoundingClientRect();
    const x = (e.clientX ?? e.touches?.[0]?.clientX ?? 0) - rect.left;
    const y = (e.clientY ?? e.touches?.[0]?.clientY ?? 0) - rect.top;
    return { uvX: x / rect.width, uvY: 1.0 - y / rect.height }; // flip Y for WebGL
  };
  
  const paintDot = (uvX, uvY) => {
    if (!drawSpeedMap) {
      drawSpeedMap = new Float32Array(waveEngine.width * waveEngine.height);
      drawSpeedMap.fill(1.0);
    }
    const brushKnob = knobPanel.knobs.find(k => k.id === 'slitW');
    const brushSize = brushKnob ? brushKnob.value : 0.006;
    const w = waveEngine.width;
    const h = waveEngine.height;
    const cx = Math.floor(uvX * w);
    const cy = Math.floor(uvY * h);
    const r = Math.max(1, Math.floor(brushSize * Math.min(w, h)));
    
    for (let dy = -r; dy <= r; dy++) {
      for (let dx = -r; dx <= r; dx++) {
        if (dx * dx + dy * dy <= r * r) {
          const px = cx + dx;
          const py = cy + dy;
          if (px >= 0 && px < w && py >= 0 && py < h) {
            drawSpeedMap[py * w + px] = eraserMode ? 1.0 : 0.0;
          }
        }
      }
    }
  };
  
  const paintLine = (x0, y0, x1, y1) => {
    // Interpolate between two points for continuous stroke
    const w = waveEngine.width;
    const h = waveEngine.height;
    const dx = Math.abs(x1 - x0) * w;
    const dy = Math.abs(y1 - y0) * h;
    const steps = Math.max(1, Math.ceil(Math.max(dx, dy) / 2));
    for (let i = 0; i <= steps; i++) {
      const t = i / steps;
      paintDot(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t);
    }
    waveEngine.updateSpeedMap(drawSpeedMap);
  };
  
  const onStart = (e) => {
    if (currentPreset !== 'draw' || !running) return;
    drawingActive = true;
    const { uvX, uvY } = getUV(e);
    lastDrawX = uvX;
    lastDrawY = uvY;
    paintDot(uvX, uvY);
    waveEngine.updateSpeedMap(drawSpeedMap);
  };
  
  const onMove = (e) => {
    if (!drawingActive) return;
    const { uvX, uvY } = getUV(e);
    if (lastDrawX >= 0) {
      paintLine(lastDrawX, lastDrawY, uvX, uvY);
    }
    lastDrawX = uvX;
    lastDrawY = uvY;
  };
  
  const onEnd = () => {
    drawingActive = false;
    lastDrawX = -1;
    lastDrawY = -1;
  };
  
  // Double-click to clear drawing
  uiCanvas.addEventListener('dblclick', (e) => {
    if (currentPreset !== 'draw' || !running) return;
    clearDrawing();
  });

  uiCanvas.addEventListener('mousedown', onStart);
  uiCanvas.addEventListener('mousemove', onMove);
  uiCanvas.addEventListener('mouseup', onEnd);
  uiCanvas.addEventListener('mouseleave', onEnd);
  uiCanvas.addEventListener('touchstart', (e) => { e.preventDefault(); onStart(e); }, { passive: false });
  uiCanvas.addEventListener('touchmove', (e) => { e.preventDefault(); onMove(e); }, { passive: false });
  uiCanvas.addEventListener('touchend', onEnd);
}

// ─── Feature 1: MIDI Input ───
function initMIDI() {
  if (!navigator.requestMIDIAccess) return;
  navigator.requestMIDIAccess().then((midiAccess) => {
    const connectHandler = () => {
      let hasInputs = false;
      for (const input of midiAccess.inputs.values()) {
        hasInputs = true;
        input.onmidimessage = onMIDIMessage;
      }
      midiConnected = hasInputs;
      if (uiOverlay) uiOverlay.midiConnected = midiConnected;
    };
    connectHandler();
    midiAccess.onstatechange = connectHandler;
  }).catch((err) => {
    console.warn('[MIDI] Access denied:', err);
  });
}

function onMIDIMessage(event) {
  if (!running) return;
  const [status, data1, data2] = event.data;
  const type = status & 0xF0;
  
  if (type === 0x90 && data2 > 0) {
    // Note-on: fire pulse
    // Velocity → amplitude: exponential curve, 25:1 dynamic range
    // Soft touch = almost nothing, hard hit = violent
    const vel = data2 / 127;
    const amp = 0.002 + Math.pow(vel, 3) * 0.048; // range: 0.002 – 0.05
    waveEngine.sourceAmplitude = amp;
    // Convert MIDI note to wave simulation frequency
    // Map MIDI notes 48-96 (C3-C7) to sourceFreq 15-100
    const noteHz = NOTE_TO_HZ(data1);
    // Map to a useful simulation freq range (15-100)
    const simFreq = Math.max(15, Math.min(100, 15 + (data1 - 21) * (85 / 87)));
    waveEngine.sourceFreq = simFreq;
    if (knobPanel) knobPanel.setKnobValue('freq', simFreq);
    // Fire 10ms pulse
    waveEngine.continuousSource = true;
    if (uiOverlay) uiOverlay.sourceActive = true;
    setTimeout(() => {
      waveEngine.continuousSource = false;
      waveEngine.sourceAmplitude = 0.02; // reset
      if (uiOverlay) uiOverlay.sourceActive = false;
    }, 10);
  } else if (type === 0xB0) {
    // CC messages
    if (data1 === 1) {
      // CC1 (Mod Wheel) → Freq knob
      const freqVal = 15 + (data2 / 127) * 85;
      waveEngine.sourceFreq = freqVal;
      if (knobPanel) knobPanel.setKnobValue('freq', freqVal);
      if (uiOverlay) uiOverlay.freq = freqVal;
    } else if (data1 === 74) {
      // CC74 → Filter cutoff
      const filterVal = 200 + (data2 / 127) * 11800;
      if (audioEngine) audioEngine.setFilterCutoff(filterVal);
      if (knobPanel) knobPanel.setKnobValue('filter', filterVal);
    }
  }
}

// Init even when this module is imported after DOMContentLoaded
// (chooser lazily loads main.js on "Launch instrument" / ?play=1).
if (document.readyState === 'loading') {
  window.addEventListener('DOMContentLoaded', init);
} else {
  init();
}

window.addEventListener('resize', () => {
  // Rebuild even during bgPreview — the engine's internal resize can wipe walls.
  setTimeout(() => buildSpeedMap(), 100);
});

// Spacebar gate: hold to emit, release to fade
window.addEventListener('keydown', (e) => {
  if (e.code === 'Space' && !e.repeat && running) {
    e.preventDefault();
    waveEngine.sourceAmplitude = 0.02; // Feature 6: spacebar always uses 0.02
    waveEngine.continuousSource = true;
    waveEngine._releasing = false;
    if (uiOverlay) uiOverlay.sourceActive = true;
    return;
  }
  
  // Feature 2: QWERTY keyboard as note input
  if (!running || e.repeat) return;
  const freq = QWERTY_MAP[e.code];
  if (freq !== undefined) {
    e.preventDefault();
    // Map Hz to simulation frequency: sourceFreq is in simulation units (15-100)
    // Use the note's Hz to pick a simulation freq in the same proportional range
    // Notes span C3(130Hz) to A6(1760Hz); map log to 15-100
    const logMin = Math.log2(130);
    const logMax = Math.log2(1760);
    const logFreq = Math.log2(Math.max(1, freq));
    const t = Math.max(0, Math.min(1, (logFreq - logMin) / (logMax - logMin)));
    const simFreq = Math.round(15 + t * 85);
    waveEngine.sourceFreq = simFreq;
    if (knobPanel) knobPanel.setKnobValue('freq', simFreq);
    if (uiOverlay) uiOverlay.freq = simFreq;
    // Feature 6: QWERTY always uses 0.02
    waveEngine.sourceAmplitude = 0.02;
    // Fire 10ms pulse
    waveEngine.continuousSource = true;
    if (uiOverlay) uiOverlay.sourceActive = true;
    setTimeout(() => {
      waveEngine.continuousSource = false;
      if (uiOverlay) uiOverlay.sourceActive = false;
    }, 10);
  }
});

window.addEventListener('keyup', (e) => {
  if (e.code === 'Space' && running) {
    e.preventDefault();
    waveEngine.continuousSource = false;
    if (uiOverlay) uiOverlay.sourceActive = false;
  }
});
