// UI Overlay — Pretext-powered Canvas2D instrument panel
// Replaces all DOM UI. Every label, button, and slider is drawn on canvas
// using Pretext for text measurement and Canvas2D for rendering.

import { prepare, layout } from 'https://esm.sh/@chenglou/pretext';

// ─── Cathedral palette ───
const BONE       = '#D4C9A8';
const BONE_50    = 'rgba(212, 201, 168, 0.50)';
const BONE_25    = 'rgba(212, 201, 168, 0.25)';
const BONE_18    = 'rgba(212, 201, 168, 0.18)';
const BONE_10    = 'rgba(212, 201, 168, 0.10)';
const BONE_08    = 'rgba(212, 201, 168, 0.08)';
const BONE_04    = 'rgba(212, 201, 168, 0.04)';
const PANEL_BG   = 'rgba(14, 14, 20, 0.75)';
const BG         = '#0A0A0F';

const FONT_FAMILY = "'JetBrains Mono', monospace";

export class UIOverlay {
  constructor(canvas, callbacks) {
    this.canvas = canvas;
    this.ctx = canvas.getContext('2d');
    this.dpr = Math.min(window.devicePixelRatio, 2);
    this.callbacks = callbacks; // { onPreset, onSpeed, onFreq, onSlit, onReset }

    // State
    this.activePreset = 'single-slit';
    this.speed = 0.9;
    this.freq = 35.0;
    this.slitVal = 0.07;
    this.slitLabel = 'Slit Gap';
    this.volume = 0.3;
    this.sensitivity = 1.0;
    this.filterCutoff = 3500;
    this.dryWet = 0.4;
    this.lfoRate = 0.0;
    this.lfoDepth = 0.0;
    this.release = 0.5;
    this.sourceActive = false; // starts off — user initiates
    this.simRunning = true;    // toggled by play/pause
    this.eraserMode = false;   // draw mode: pen vs eraser
    this.scaleMode = false;    // audio: pentatonic scale quantize
    this.frozen = false;        // freeze: sim stopped, audio alive
    this.droneMode = false;     // drone: harmonic series lock
    this.hoveredElement = null;
    this.draggingSlider = null; // 'speed' | 'freq' | 'slit' | null
    this.trayOpacity = 0.5;
    this.trayTargetOpacity = 0.5;
    
    // Tooltip: hovered button description
    this._tooltipText = null;
    this._tooltipX = 0; // center x of hovered button
    this._tooltipY = 0; // y above tray
    
    // Button descriptions
    this._descriptions = {
      'preset-double-slit': 'Two slits — classic interference',
      'preset-single-slit': 'Single slit diffraction',
      'preset-lens': 'Convex lens — focuses wavefront',
      'preset-diffraction': 'Diffraction grating',
      'preset-draw': 'Draw barriers freehand',
      'preset-mach-zehnder': 'Mach-Zehnder interferometer',
      'stopStart': 'Toggle wave source on/off',
      'pulse': 'Fire a single 10ms wavefront',
      'scale': 'Quantize resonance to pentatonic scale',
      'drone': 'Lock to harmonic series (f, 2f, 3f, 4f)',
      'playPause': 'Freeze/unfreeze entire simulation',
      'freeze': 'Freeze sim — audio keeps playing the pattern',
      'reset': 'Clear wave field, keep settings',
      'record': 'Record video + audio as WebM',
      'clearDraw': 'Erase all drawn barriers',
      'toggleEraser': 'Switch between pen and eraser',
      'scene-0': 'Scene 1 — click: recall, hold: save',
      'scene-1': 'Scene 2 — click: recall, hold: save',
      'scene-2': 'Scene 3 — click: recall, hold: save',
      'scene-3': 'Scene 4 — click: recall, hold: save',
    };
    this.showClickStart = true;
    this.clickStartOpacity = 1.0;
    this.clickStartPhase = 0;

    // Feature 1: MIDI indicator
    this.midiConnected = false;
    
    // Feature 4: Recording state
    this.recording = false;
    this._recDotPhase = 0;
    
    // Feature 5: Scene memory (4 slots)
    this.scenes = [null, null, null, null];
    this._scenePressStart = [0, 0, 0, 0]; // timestamp of mousedown per slot
    this._sceneLongPressTimer = [null, null, null, null];

    // Presets definition
    this.presets = [
      { id: 'double-slit', label: 'Double Slit' },
      { id: 'single-slit', label: 'Single Slit' },
      { id: 'lens',         label: 'Lens' },
      { id: 'corner',       label: 'Diffraction' },
      { id: 'draw',         label: 'Draw' },
      { id: 'mach-zehnder', label: 'Mach-Zehnder' },
    ];

    // Sliders removed — all controls now on knob panel below
    this.sliders = [];
    this.perfSliders = [];

    // Hit regions (populated on each draw)
    this.hitRegions = []; // { id, type, x, y, w, h, ...extra }

    // Detector data (set externally from main.js)
    this.detectorData = null; // Float32Array of accumulated intensity
    this.detectorInstant = null; // Float32Array of instantaneous intensity
    this.detectorMax = 0.001;
    this.instantMax = 0.001;
    this.detectorScreenX = 0.92; // UV position, updated from engine
    
    // Physics readouts
    this.wavelength = 0;
    this.dLambdaRatio = 0;
    
    // Pretext caches
    this._textCache = new Map();

    this._resize();
    this._bindEvents();
  }

  _resize() {
    const dpr = Math.min(window.devicePixelRatio, 2);
    const w = this.canvas.clientWidth;
    const h = this.canvas.clientHeight;
    this.canvas.width = Math.floor(w * dpr);
    this.canvas.height = Math.floor(h * dpr);
    this.dpr = dpr;
    this.w = w;
    this.h = h;
  }

  // ─── Text measurement via Pretext ───
  _measureText(text, font) {
    const key = `${text}|${font}`;
    if (this._textCache.has(key)) return this._textCache.get(key);
    try {
      const prepared = prepare(text, font);
      const { height } = layout(prepared, 9999, 20);
      // Also measure width via canvas for single-line
      this.ctx.save();
      this.ctx.font = font;
      const metrics = this.ctx.measureText(text);
      this.ctx.restore();
      const result = { width: metrics.width, height, prepared };
      this._textCache.set(key, result);
      return result;
    } catch {
      // Fallback to pure canvas measurement
      this.ctx.save();
      this.ctx.font = font;
      const metrics = this.ctx.measureText(text);
      this.ctx.restore();
      const result = { width: metrics.width, height: 14, prepared: null };
      this._textCache.set(key, result);
      return result;
    }
  }

  // ─── Event handling ───
  _bindEvents() {
    this.canvas.addEventListener('mousemove', (e) => {
      // Track mouse for per-letter hover effects
      const rect = this.canvas.getBoundingClientRect();
      this._mouseX = e.clientX - rect.left;
      this._mouseY = e.clientY - rect.top;
      this._onMouseMove(e);
    });
    this.canvas.addEventListener('mousedown', (e) => {
      this._onMouseDown(e);
      // Don't forward to _onMouseDownTray: tray hit regions are in preset-canvas
      // coord space and would spuriously trigger scene long-press timers here.
    });
    this.canvas.addEventListener('mouseup',   (e) => {
      this._onMouseUp(e);
    });
    this.canvas.addEventListener('mouseleave', () => {
      this.trayTargetOpacity = 0.5;
      this.draggingSlider = null;
    });
    this.canvas.addEventListener('click', (e) => this._onClick(e));
    window.addEventListener('resize', () => {
      this._resize();
      this._textCache.clear();
    });
  }

  _getMousePos(e) {
    const rect = this.canvas.getBoundingClientRect();
    return { x: e.clientX - rect.left, y: e.clientY - rect.top };
  }

  _hitTest(x, y) {
    for (let i = this.hitRegions.length - 1; i >= 0; i--) {
      const r = this.hitRegions[i];
      if (x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h) {
        return r;
      }
    }
    return null;
  }

  _onMouseMove(e) {
    // Don't hit-test against hitRegions here: those are preset-canvas regions
    // and would spuriously highlight tray buttons when hovering ui-canvas.
    const pos = this._getMousePos(e);

    // Slider dragging (inert — sliders moved to knob panel)
    if (this.draggingSlider) {
      this._updateSliderFromPos(this.draggingSlider, pos.x);
    }

    // Cursor
    this.canvas.style.cursor = this.showClickStart ? 'pointer' : 'default';
  }

  _onMouseDown(e) {
    const pos = this._getMousePos(e);
    const hit = this._hitTest(pos.x, pos.y);
    if (hit && hit.type === 'slider') {
      this.draggingSlider = hit.sliderId;
      this._updateSliderFromPos(hit.sliderId, pos.x);
    }
  }

  _onMouseUp() {
    this.draggingSlider = null;
  }

  _updateSliderFromPos(sliderId, mouseX) {
    const region = this.hitRegions.find(r => r.type === 'slider' && r.sliderId === sliderId);
    if (!region) return;
    const sliderDef = this.sliders.find(s => s.id === sliderId) || this.perfSliders.find(s => s.id === sliderId);
    if (!sliderDef) return;

    const t = Math.max(0, Math.min(1, (mouseX - region.x) / region.w));
    const raw = sliderDef.min + t * (sliderDef.max - sliderDef.min);
    const val = Math.round(raw / sliderDef.step) * sliderDef.step;

    if (sliderId === 'speed') {
      this.speed = val;
      this.callbacks.onSpeed?.(val);
    } else if (sliderId === 'freq') {
      this.freq = val;
      this.callbacks.onFreq?.(val);
    } else if (sliderId === 'slit') {
      this.slitVal = val;
      this.callbacks.onSlit?.(val);
    } else if (sliderId === 'volume') {
      this.volume = val;
      this.callbacks.onVolume?.(val);
    } else if (sliderId === 'sensitivity') {
      this.sensitivity = val;
      this.callbacks.onSensitivity?.(val);
    } else if (sliderId === 'filter') {
      this.filterCutoff = val;
      this.callbacks.onFilter?.(val);
    } else if (sliderId === 'drywet') {
      this.dryWet = val;
      this.callbacks.onDryWet?.(val);
    } else if (sliderId === 'lfoRate') {
      this.lfoRate = val;
      this.callbacks.onLfoRate?.(val);
    } else if (sliderId === 'lfoDepth') {
      this.lfoDepth = val;
      this.callbacks.onLfoDepth?.(val);
    } else if (sliderId === 'release') {
      this.release = val;
      this.callbacks.onRelease?.(val);
    }
  }

  _onClick(e) {
    if (this.showClickStart) {
      this.showClickStart = false;
      this.callbacks.onStart?.();
      return;
    }
    // All tray buttons live on preset-canvas and are handled in bindPresetCanvas.
    // Do NOT hit-test here — hitRegions are in preset-canvas coords and would
    // spuriously match clicks on ui-canvas (e.g. wiping drawings in draw mode).
  }

  _onMouseDownTray(pos) {
    // Handle mousedown for scene long-press detection
    for (let i = this.hitRegions.length - 1; i >= 0; i--) {
      const r = this.hitRegions[i];
      if (pos.x >= r.x && pos.x <= r.x + r.w && pos.y >= r.y && pos.y <= r.y + r.h) {
        if (r.type === 'scene') {
          this._scenePressStart[r.sceneIdx] = Date.now();
          clearTimeout(this._sceneLongPressTimer[r.sceneIdx]);
          this._sceneLongPressTimer[r.sceneIdx] = setTimeout(() => {
            this.callbacks.onSceneSave?.(r.sceneIdx);
          }, 1000);
        }
        break;
      }
    }
  }

  _onMouseUpTray() {
    // Cancel any pending long-press timers
    for (let i = 0; i < 4; i++) {
      clearTimeout(this._sceneLongPressTimer[i]);
      this._sceneLongPressTimer[i] = null;
    }
  }

  // ─── Draw presets/buttons on external canvas ───
  drawPresetBar(canvas) {
    const dpr = Math.min(window.devicePixelRatio, 2);
    const w = canvas.clientWidth;
    const h = canvas.clientHeight;
    canvas.width = Math.floor(w * dpr);
    canvas.height = Math.floor(h * dpr);
    const ctx = canvas.getContext('2d');
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.save();
    ctx.scale(dpr, dpr);
    
    // Override tray opacity to full for this external bar
    const oldOpacity = this.trayOpacity;
    this.trayOpacity = 1.0;
    this._drawControlTray(ctx, w, h);
    this.trayOpacity = oldOpacity;
    
    // Tooltip above hovered button
    if (this._tooltipText) {
      ctx.font = `300 9px ${FONT_FAMILY}`;
      ctx.textAlign = 'center';
      ctx.textBaseline = 'bottom';
      ctx.letterSpacing = '0.3px';
      ctx.fillStyle = 'rgba(212, 201, 168, 0.45)';
      ctx.fillText(this._tooltipText, this._tooltipX, this._tooltipY);
    }
    
    ctx.restore();
  }

  // Bind events on the preset canvas for clicks
  bindPresetCanvas(canvas) {
    canvas.addEventListener('click', (e) => {
      const rect = canvas.getBoundingClientRect();
      const x = e.clientX - rect.left;
      const y = e.clientY - rect.top;
      const hit = this._hitTest(x, y);
      if (!hit) return;
      if (hit.type === 'preset') {
        this.activePreset = hit.presetId;
        this.callbacks.onPreset?.(hit.presetId);
      } else if (hit.type === 'reset') {
        this.sourceActive = true;
        this.callbacks.onReset?.();
      } else if (hit.type === 'stopStart') {
        this.sourceActive = !this.sourceActive;
        this.callbacks.onStopStart?.(this.sourceActive);
      } else if (hit.type === 'playPause') {
        this.simRunning = !this.simRunning;
        this.callbacks.onPlayPause?.(this.simRunning);
      } else if (hit.type === 'clearDraw') {
        this.callbacks.onClearDraw?.();
      } else if (hit.type === 'pulse') {
        this.callbacks.onPulse?.();
      } else if (hit.type === 'toggleEraser') {
        this.callbacks.onToggleEraser?.();
      } else if (hit.type === 'scale') {
        this.scaleMode = !this.scaleMode;
        this.callbacks.onScale?.(this.scaleMode);
      } else if (hit.type === 'drone') {
        this.droneMode = !this.droneMode;
        this.callbacks.onDrone?.(this.droneMode);
      } else if (hit.type === 'freeze') {
        this.frozen = !this.frozen;
        this.callbacks.onFreeze?.(this.frozen);
      } else if (hit.type === 'record') {
        this.recording = !this.recording;
        this.callbacks.onRecord?.(this.recording);
      } else if (hit.type === 'scene') {
        // Short click: recall scene
        this.callbacks.onSceneRecall?.(hit.sceneIdx);
      }
    });
    canvas.addEventListener('mousedown', (e) => {
      const rect = canvas.getBoundingClientRect();
      const x = e.clientX - rect.left;
      const y = e.clientY - rect.top;
      this._onMouseDownTray({ x, y });
    });
    canvas.addEventListener('mouseup', () => this._onMouseUpTray());
    canvas.addEventListener('mouseleave', () => {
      this.hoveredElement = null;
      this._onMouseUpTray();
    });
    canvas.addEventListener('mousemove', (e) => {
      const rect = canvas.getBoundingClientRect();
      const x = e.clientX - rect.left;
      const y = e.clientY - rect.top;
      const hit = this._hitTest(x, y);
      this.hoveredElement = hit ? hit.id : null;
      canvas.style.cursor = hit && hit.type !== '__tray' ? 'pointer' : 'default';
      
      // Tooltip
      if (hit && hit.id && hit.type !== '__tray') {
        const desc = this._descriptions[hit.id];
        if (desc) {
          this._tooltipText = desc;
          this._tooltipX = hit.x + hit.w / 2;
          this._tooltipY = hit.y - 12;
        } else {
          this._tooltipText = null;
        }
      } else {
        this._tooltipText = null;
      }
    });
    canvas.addEventListener('mouseleave', () => {
      this.hoveredElement = null;
      this._tooltipText = null;
    });
  }

  // ─── Update slider config from preset ───
  updateSlitConfig(label, min, max, step, value) {
    this.slitLabel = label;
    this.slitVal = value;
    // Sliders moved to knob panel — update handled there
  }

  // ─── Rendering ───
  draw(time) {
    const ctx = this.ctx;
    const dpr = this.dpr;
    const w = this.w;
    const h = this.h;

    ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
    ctx.save();
    ctx.scale(dpr, dpr);

    this.hitRegions = [];

    // Sync detector position from engine
    if (this.callbacks.getDetectorX) {
      this.detectorScreenX = this.callbacks.getDetectorX();
    }
    
    // Animate tray opacity
    this.trayOpacity += (this.trayTargetOpacity - this.trayOpacity) * 0.12;

    if (this.showClickStart) {
      this._drawClickStart(ctx, w, h, time);
    } else {
      this._drawDetectorGraph(ctx, w, h);
    }

    ctx.restore();
  }

  _drawClickStart(ctx, w, h, time) {
    // Semi-transparent background — lets the WebGL wave engine show through
    // so the user sees the slit + wave preview behind the title card.
    ctx.clearRect(0, 0, w, h);
    ctx.fillStyle = 'rgba(10, 10, 15, 0.55)';
    ctx.fillRect(0, 0, w, h);

    // Pulsing text
    this.clickStartPhase += 0.015;
    const pulse = 0.5 + 0.4 * Math.sin(this.clickStartPhase);
    const alpha = pulse;

    ctx.save();
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    
    // Title: FRINGE
    ctx.font = `300 36px ${FONT_FAMILY}`;
    ctx.letterSpacing = '12px';
    ctx.fillStyle = `rgba(212, 201, 168, 0.70)`;
    ctx.fillText('FRINGE', w / 2, h / 2 - 50);
    
    // Subtitle: by ER
    ctx.font = `300 10px ${FONT_FAMILY}`;
    ctx.letterSpacing = '4px';
    ctx.fillStyle = `rgba(212, 201, 168, 0.30)`;
    ctx.fillText('BY ER', w / 2, h / 2 - 24);
    
    // Feynman quote — on the double-slit experiment
    ctx.font = `italic 300 11px ${FONT_FAMILY}`;
    ctx.letterSpacing = '0.5px';
    ctx.fillStyle = `rgba(212, 201, 168, 0.18)`;
    ctx.fillText('\u201CIt contains the only mystery.\u201D', w / 2, h / 2 + 14);
    ctx.font = `300 9px ${FONT_FAMILY}`;
    ctx.fillStyle = `rgba(212, 201, 168, 0.12)`;
    ctx.fillText('\u2014 Richard Feynman, on the double-slit experiment', w / 2, h / 2 + 32);
    
    // CLICK TO BEGIN — dramatic per-letter hover reveal
    const ctbText = 'CLICK TO BEGIN';
    const ctbY = h / 2 + 128;
    ctx.font = `400 11px ${FONT_FAMILY}`;
    ctx.letterSpacing = '0px';
    
    const letterSpc = 3.5;
    const letters = ctbText.split('');
    const letterWidths = letters.map(l => ctx.measureText(l).width);
    const totalTextW = letterWidths.reduce((a, b) => a + b, 0) + (letters.length - 1) * letterSpc;
    let lx = w / 2 - totalTextW / 2;
    
    const mx = this._mouseX ?? -999;
    const my = this._mouseY ?? -999;
    const dy = Math.abs(ctbY - my);
    const isNearText = dy < 40 && mx > lx - 20 && mx < lx + totalTextW + 20;
    
    // Smooth transition: animate hover amount
    if (!this._ctbHover) this._ctbHover = 0;
    this._ctbHover += ((isNearText ? 1 : 0) - this._ctbHover) * 0.12;
    
    // Per-letter animation state
    if (!this._ctbLetterReveal) this._ctbLetterReveal = new Float32Array(letters.length);
    
    ctx.textAlign = 'left';
    ctx.textBaseline = 'middle';
    
    for (let i = 0; i < letters.length; i++) {
      const letterCenterX = lx + letterWidths[i] / 2;
      const distX = Math.abs(letterCenterX - mx);
      
      // Staggered reveal: letters closer to cursor activate first
      const targetReveal = isNearText ? Math.max(0, 1.0 - distX / 120) : 0;
      this._ctbLetterReveal[i] += (targetReveal - this._ctbLetterReveal[i]) * 0.15;
      const reveal = this._ctbLetterReveal[i];
      
      // When not hovered: normal pulsing text
      // When hovered: letters pop to full white with slight vertical lift
      const baseAlpha = alpha * (1 - this._ctbHover * 0.8); // dims the pulse
      const revealAlpha = reveal;
      const finalAlpha = Math.min(1, baseAlpha + revealAlpha);
      
      // Color: shifts from bone to bright white on reveal
      const r = 212 + reveal * 43; // 212 → 255
      const g = 201 + reveal * 54; // 201 → 255
      const b = 168 + reveal * 87; // 168 → 255
      
      // Slight upward lift on reveal
      const lift = reveal * -3;
      
      // Scale effect via font size
      const size = 11 + reveal * 2;
      ctx.font = `${400 + reveal * 200} ${size}px ${FONT_FAMILY}`;
      
      ctx.fillStyle = `rgba(${r|0}, ${g|0}, ${b|0}, ${finalAlpha})`;
      ctx.fillText(letters[i], lx, ctbY + lift);
      
      // Subtle glow behind bright letters
      if (reveal > 0.3) {
        ctx.fillStyle = `rgba(${r|0}, ${g|0}, ${b|0}, ${reveal * 0.08})`;
        ctx.fillText(letters[i], lx - 0.5, ctbY + lift);
        ctx.fillText(letters[i], lx + 0.5, ctbY + lift);
      }
      
      // Reset font for next measurement consistency
      ctx.font = `400 11px ${FONT_FAMILY}`;
      lx += letterWidths[i] + letterSpc;
    }
    
    ctx.restore();

    // Hit region: entire screen
    this.hitRegions.push({ id: 'click-start', type: 'click-start', x: 0, y: 0, w, h });
  }

  _drawTitle(ctx) {
    // Title: "WAVE OPTICS"
    const titleFont = `400 12px ${FONT_FAMILY}`;
    const subFont = `300 9px ${FONT_FAMILY}`;

    ctx.save();
    ctx.font = titleFont;
    ctx.fillStyle = BONE_25;
    ctx.textBaseline = 'top';
    ctx.letterSpacing = '2.4px';
    ctx.fillText('WAVE OPTICS', 20, 20);

    ctx.font = subFont;
    ctx.fillStyle = BONE_18;
    ctx.letterSpacing = '0.7px';
    ctx.fillText('Light discovering structure', 20, 37);
    ctx.restore();
  }

  _drawControlTray(ctx, w, h) {
    const trayH = 40;
    const padX = 16;
    const gap = 14;
    const btnFont = `400 9px ${FONT_FAMILY}`;
    const bh = 22;

    // Measure total width needed
    const presetWidths = this.presets.map(p => {
      return this._measureText(p.label.toUpperCase(), btnFont).width + 20;
    });
    const presetTotalW = presetWidths.reduce((a, b) => a + b, 0) + (this.presets.length - 1) * 6;

    const resetW = this._measureText('RESET', btnFont).width + 24;
    const stopLabel = this.sourceActive ? 'STOP' : 'START';
    const stopW = this._measureText(stopLabel, btnFont).width + 24;
    const pulseW = this._measureText('PULSE', btnFont).width + 24;
    const scaleW = this._measureText('SCALE', btnFont).width + 24;
    const droneW = this._measureText('DRONE', btnFont).width + 24;

    const playLabel = this.simRunning ? 'PAUSE' : 'PLAY';
    const playW = this._measureText(playLabel, btnFont).width + 24;
    const freezeW = this._measureText('FREEZE', btnFont).width + 24;
    const recW = this._measureText('REC', btnFont).width + 24;
    // Scene memory: 4 small numbered buttons
    const sceneSlotW = this._measureText('1', btnFont).width + 16;
    const showDraw = this.activePreset === 'draw';
    const eraserLabel = this.eraserMode ? 'PEN' : 'ERASER';
    const eraserW = showDraw ? this._measureText(eraserLabel, btnFont).width + 24 : 0;
    const clearW = showDraw ? this._measureText('CLEAR', btnFont).width + 24 : 0;

    // In draw mode, ERASER sits inline next to the DRAW preset; CLEAR stays at the end.
    const eraserInlineW = showDraw ? eraserW + 6 : 0;
    const drawBtnsW = showDraw ? 6 + clearW : 0;
    // Layout order: presets [eraser if draw] | stop | pulse | scale | drone | pause | freeze | reset | rec | scene1-4 | [clear if draw]
    const totalW = padX + presetTotalW + eraserInlineW + gap + stopW + 6 + pulseW + 6 + scaleW + 6 + droneW + 6 + playW + 6 + freezeW + 6 + resetW + 6 + recW + 6 + sceneSlotW * 4 + 3 * 3 + drawBtnsW + padX;
    const trayX = Math.floor((w - totalW) / 2);
    const trayY = Math.floor((h - trayH) / 2);

    // Register tray hit region
    this.hitRegions.push({ id: '__tray', type: '__tray', x: trayX, y: trayY, w: totalW, h: trayH });

    ctx.save();
    ctx.globalAlpha = this.trayOpacity;

    // Tray background
    ctx.fillStyle = PANEL_BG;
    this._roundRect(ctx, trayX, trayY, totalW, trayH, 8);
    ctx.fill();

    // Tray border
    ctx.strokeStyle = BONE_08;
    ctx.lineWidth = 1;
    this._roundRect(ctx, trayX, trayY, totalW, trayH, 8);
    ctx.stroke();

    let cx = trayX + padX;
    const centerY = trayY + trayH / 2;

    // ─── Helper: draw a tray button ───
    const drawBtn = (label, btnW, hitId, hitType, isEngaged, x, color) => {
      const by = centerY - bh / 2;
      const isHov = this.hoveredElement === hitId;
      if (color) {
        // Colored button with hover brightening
        const [r, g, b] = color;
        const bgA   = isHov ? 0.22 : (isEngaged ? 0.18 : 0.06);
        const bordA = isHov ? 0.60 : (isEngaged ? 0.50 : 0.15);
        const textA = isHov ? 1.0  : (isEngaged ? 0.90 : 0.50);
        ctx.fillStyle = `rgba(${r}, ${g}, ${b}, ${bgA})`;
        this._roundRect(ctx, x, by, btnW, bh, 4);
        ctx.fill();
        ctx.strokeStyle = `rgba(${r}, ${g}, ${b}, ${bordA})`;
        ctx.lineWidth = 1;
        this._roundRect(ctx, x, by, btnW, bh, 4);
        ctx.stroke();
        ctx.font = btnFont;
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillStyle = textA >= 1.0 ? `rgb(${r}, ${g}, ${b})` : `rgba(${r}, ${g}, ${b}, ${textA})`;
        ctx.letterSpacing = '1px';
        ctx.fillText(label, x + btnW / 2, centerY);
      } else {
        // Default bone-colored button
        ctx.fillStyle = isEngaged ? 'rgba(212, 201, 168, 0.12)' : (isHov ? 'rgba(212, 201, 168, 0.10)' : 'rgba(212, 201, 168, 0.04)');
        this._roundRect(ctx, x, by, btnW, bh, 4);
        ctx.fill();
        ctx.strokeStyle = isEngaged ? 'rgba(212, 201, 168, 0.30)' : (isHov ? 'rgba(212, 201, 168, 0.25)' : 'rgba(212, 201, 168, 0.08)');
        ctx.lineWidth = 1;
        this._roundRect(ctx, x, by, btnW, bh, 4);
        ctx.stroke();
        ctx.font = btnFont;
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillStyle = isEngaged ? BONE : (isHov ? BONE : BONE_25);
        ctx.letterSpacing = '1px';
        ctx.fillText(label, x + btnW / 2, centerY);
      }
      this.hitRegions.push({ id: hitId, type: hitType, x, y: by, w: btnW, h: bh });
    };

    // ─── Preset buttons ───
    for (let i = 0; i < this.presets.length; i++) {
      const p = this.presets[i];
      const bw = presetWidths[i];
      const bx = cx;
      const by = centerY - bh / 2;
      const isActive = this.activePreset === p.id;
      const isHovered = this.hoveredElement === `preset-${p.id}`;

      ctx.fillStyle = isActive ? 'rgba(212, 201, 168, 0.10)' : (isHovered ? 'rgba(212, 201, 168, 0.08)' : 'rgba(212, 201, 168, 0.04)');
      this._roundRect(ctx, bx, by, bw, bh, 4);
      ctx.fill();

      ctx.strokeStyle = isActive ? 'rgba(212, 201, 168, 0.30)' : (isHovered ? 'rgba(212, 201, 168, 0.18)' : 'rgba(212, 201, 168, 0.08)');
      ctx.lineWidth = 1;
      this._roundRect(ctx, bx, by, bw, bh, 4);
      ctx.stroke();

      ctx.font = btnFont;
      ctx.textAlign = 'center';
      ctx.textBaseline = 'middle';
      ctx.fillStyle = isActive ? BONE : (isHovered ? BONE_50 : BONE_25);
      ctx.letterSpacing = '0.7px';
      ctx.fillText(p.label.toUpperCase(), bx + bw / 2, centerY);

      this.hitRegions.push({
        id: `preset-${p.id}`, type: 'preset', presetId: p.id,
        x: bx, y: by, w: bw, h: bh
      });

      cx += bw + 6;

      // ERASER sits inline next to the DRAW (pencil) preset in draw mode
      if (p.id === 'draw' && showDraw) {
        const eraserColor = this.eraserMode ? [220, 160, 80] : null;
        drawBtn(eraserLabel, eraserW, 'toggleEraser', 'toggleEraser', this.eraserMode, cx, eraserColor);
        cx += eraserW + 6;
      }
    }

    cx += gap;

    // ─── Stop/Start button (red=running/stop, green=stopped/start) ───
    const stopColor = this.sourceActive ? [220, 80, 80] : [90, 200, 120];
    drawBtn(stopLabel, stopW, 'stopStart', 'stopStart', true, cx, stopColor);
    cx += stopW + 6;

    // ─── Pulse button ───
    drawBtn('PULSE', pulseW, 'pulse', 'pulse', false, cx, [180, 160, 100]);
    cx += pulseW + 6;

    // ─── Scale button ───
    const scaleColor = this.scaleMode ? [160, 120, 220] : null;
    drawBtn('SCALE', scaleW, 'scale', 'scale', this.scaleMode, cx, scaleColor);
    cx += scaleW + 6;

    // ─── Drone button ───
    const droneColor = this.droneMode ? [200, 140, 60] : null;
    drawBtn('DRONE', droneW, 'drone', 'drone', this.droneMode, cx, droneColor);
    cx += droneW + 6;

    // ─── Play/Pause button ───
    drawBtn(playLabel, playW, 'playPause', 'playPause', !this.simRunning, cx);
    cx += playW + 6;

    // ─── Freeze button ───
    const freezeColor = this.frozen ? [100, 180, 220] : null;
    drawBtn('FREEZE', freezeW, 'freeze', 'freeze', this.frozen, cx, freezeColor);
    cx += freezeW + 6;

    // ─── Reset button ───
    drawBtn('RESET', resetW, 'reset', 'reset', false, cx);
    cx += resetW + 6;

    // ─── REC button (Feature 4) ───
    {
      this._recDotPhase += 0.08;
      const recIsRec = this.recording;
      // Pulsing dot when recording
      let recLabel = 'REC';
      if (recIsRec) {
        // Draw pulsing dot on the button after text
        const dotPulse = 0.5 + 0.5 * Math.sin(this._recDotPhase);
        drawBtn('REC', recW, 'record', 'record', recIsRec, cx, [220, 60, 60]);
        // Overlay a small pulsing dot
        const dotX = cx + recW - 9;
        const dotY = centerY;
        ctx.beginPath();
        ctx.arc(dotX, dotY, 2.5, 0, Math.PI * 2);
        ctx.fillStyle = `rgba(255, 80, 80, ${0.4 + 0.6 * dotPulse})`;
        ctx.fill();
      } else {
        drawBtn('REC', recW, 'record', 'record', false, cx);
      }
      cx += recW + 6;
    }

    // ─── Scene memory buttons 1–4 (Feature 5) ───
    for (let si = 0; si < 4; si++) {
      const hasScene = this.scenes[si] !== null;
      // Amber when saved, dim when empty
      const sceneColor = hasScene ? [220, 160, 60] : null;
      const sceneLabel = String(si + 1);
      drawBtn(sceneLabel, sceneSlotW, `scene-${si}`, 'scene', hasScene, cx, sceneColor);
      // Store sceneIdx on the hit region (drawBtn already pushed a hit region, update it)
      const lastHit = this.hitRegions[this.hitRegions.length - 1];
      if (lastHit) lastHit.sceneIdx = si;
      cx += sceneSlotW + (si < 3 ? 3 : 6);
    }

    // ─── Clear (draw mode only; eraser is inline next to DRAW preset) ───
    if (showDraw) {
      drawBtn('CLEAR', clearW, 'clearDraw', 'clearDraw', false, cx);
    }

    ctx.restore();
  }

  // ─── Performance controls (second row) ───
  _drawPerfTray(ctx, w, h) {
    const trayH = 36;
    const padX = 16;
    const gap = 16;
    const sliderW = 70;
    const sliderGroupW = sliderW + 2;
    const totalW = padX + this.perfSliders.length * sliderGroupW + (this.perfSliders.length - 1) * gap + padX;
    const trayX = Math.floor((w - totalW) / 2);
    const trayY = h - 82 - trayH; // above the main tray (which is at h-20-52)
    const centerY = trayY + trayH / 2;

    // Register tray hit region for hover detection
    this.hitRegions.push({ id: '__perfTray', type: '__tray', x: trayX, y: trayY, w: totalW, h: trayH });

    ctx.save();
    ctx.globalAlpha = this.trayOpacity * 0.85;

    // Background
    ctx.fillStyle = PANEL_BG;
    this._roundRect(ctx, trayX, trayY, totalW, trayH, 6);
    ctx.fill();
    ctx.strokeStyle = BONE_08;
    ctx.lineWidth = 1;
    this._roundRect(ctx, trayX, trayY, totalW, trayH, 6);
    ctx.stroke();

    let cx = trayX + padX;

    for (let i = 0; i < this.perfSliders.length; i++) {
      const s = this.perfSliders[i];
      const label = typeof s.label === 'function' ? s.label() : s.label;
      const val = s.getValue();
      const t = (val - s.min) / (s.max - s.min);

      const sliderX = cx;
      const labelY = centerY - 8;
      const trackY = centerY + 5;

      // Label
      ctx.font = `400 8px ${FONT_FAMILY}`;
      ctx.textAlign = 'left';
      ctx.textBaseline = 'middle';
      ctx.fillStyle = BONE_25;
      ctx.letterSpacing = '0.8px';
      ctx.fillText(label.toUpperCase(), sliderX, labelY);

      // Track
      ctx.fillStyle = 'rgba(212, 201, 168, 0.12)';
      this._roundRect(ctx, sliderX, trackY - 1, sliderW, 2, 1);
      ctx.fill();

      // Thumb
      const thumbX = sliderX + t * sliderW;
      ctx.beginPath();
      ctx.arc(thumbX, trackY, 3.5, 0, Math.PI * 2);
      const isHovered = this.hoveredElement === `slider-${s.id}` || this.draggingSlider === s.id;
      ctx.fillStyle = isHovered ? BONE : BONE_50;
      ctx.fill();

      // Hit region
      this.hitRegions.push({
        id: `slider-${s.id}`, type: 'slider', sliderId: s.id,
        x: sliderX, y: trackY - 8, w: sliderW, h: 16
      });

      cx += sliderGroupW + gap;
    }

    ctx.restore();
  }

  // ─── 1D detector intensity graph (right of detector) ───
  _drawDetectorGraph(ctx, w, h) {
    const hasAccum = this.detectorData && this.detectorData.length > 0;
    const hasInstant = this.detectorInstant && this.detectorInstant.length > 0;
    if (!hasAccum && !hasInstant) return;
    
    const len = hasAccum ? this.detectorData.length : this.detectorInstant.length;
    
    // Track maxes
    if (hasAccum) {
      for (let i = 0; i < len; i++) {
        if (this.detectorData[i] > this.detectorMax) this.detectorMax = this.detectorData[i];
      }
    }
    if (hasInstant) {
      let iMax = 0;
      for (let i = 0; i < len; i++) {
        if (this.detectorInstant[i] > iMax) iMax = this.detectorInstant[i];
      }
      this.instantMax = Math.max(this.instantMax * 0.95, iMax); // smooth decay
    }
    
    if (this.detectorMax < 0.0001 && this.instantMax < 0.0001) return;
    
    const graphX = Math.floor(this.detectorScreenX * w) + 6;
    const graphW = Math.max(30, Math.min(60, w - graphX - 5));
    const graphY = 30;
    const graphH = h - 100;
    
    ctx.save();
    
    // Label
    ctx.font = `400 8px ${FONT_FAMILY}`;
    ctx.fillStyle = BONE_25;
    ctx.textAlign = 'center';
    ctx.textBaseline = 'top';
    ctx.letterSpacing = '1px';
    ctx.fillText('I(y)', graphX + graphW / 2, graphY - 14);
    
    // --- Accumulated fill (slow-building pattern) ---
    if (hasAccum && this.detectorMax > 0.0001) {
      ctx.beginPath();
      ctx.moveTo(graphX, graphY);
      for (let py = 0; py < graphH; py++) {
        const dataIdx = Math.floor((1 - py / graphH) * len);
        const val = dataIdx >= 0 && dataIdx < len ? this.detectorData[dataIdx] : 0;
        const norm = val / this.detectorMax;
        ctx.lineTo(graphX + norm * graphW, graphY + py);
      }
      ctx.lineTo(graphX, graphY + graphH);
      ctx.closePath();
      ctx.fillStyle = 'rgba(212, 201, 168, 0.12)';
      ctx.fill();
      ctx.strokeStyle = 'rgba(212, 201, 168, 0.25)';
      ctx.lineWidth = 1;
      ctx.stroke();
    }
    
    // --- Instantaneous spike line (real-time) ---
    if (hasInstant && this.instantMax > 0.0001) {
      ctx.beginPath();
      for (let py = 0; py < graphH; py++) {
        const dataIdx = Math.floor((1 - py / graphH) * len);
        const val = dataIdx >= 0 && dataIdx < len ? this.detectorInstant[dataIdx] : 0;
        const norm = Math.min(1.0, val / (this.instantMax + 0.001));
        const x = graphX + norm * graphW;
        if (py === 0) ctx.moveTo(x, graphY);
        else ctx.lineTo(x, graphY + py);
      }
      ctx.strokeStyle = 'rgba(255, 230, 170, 0.8)';
      ctx.lineWidth = 1.5;
      ctx.stroke();
    }
    
    ctx.restore();
  }

  // ─── Physics info (wavelength, d/λ) ───
  _drawPhysicsInfo(ctx, w, h) {
    if (this.wavelength <= 0) return;
    
    ctx.save();
    ctx.font = `300 9px ${FONT_FAMILY}`;
    ctx.textAlign = 'left';
    ctx.textBaseline = 'top';
    ctx.fillStyle = BONE_18;
    ctx.letterSpacing = '0.5px';
    
    const x = 20;
    let y = 52;
    
    ctx.fillText(`λ = ${this.wavelength.toFixed(3)}`, x, y);
    
    if (this.dLambdaRatio > 0) {
      y += 14;
      ctx.fillText(`d/λ = ${this.dLambdaRatio.toFixed(1)}`, x, y);
    }
    
    ctx.restore();
  }

  _roundRect(ctx, x, y, w, h, r) {
    ctx.beginPath();
    ctx.moveTo(x + r, y);
    ctx.lineTo(x + w - r, y);
    ctx.quadraticCurveTo(x + w, y, x + w, y + r);
    ctx.lineTo(x + w, y + h - r);
    ctx.quadraticCurveTo(x + w, y + h, x + w - r, y + h);
    ctx.lineTo(x + r, y + h);
    ctx.quadraticCurveTo(x, y + h, x, y + h - r);
    ctx.lineTo(x, y + r);
    ctx.quadraticCurveTo(x, y, x + r, y);
    ctx.closePath();
  }
}
