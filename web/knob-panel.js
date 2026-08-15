// Knob Panel — circular rotary controls + 3 routable LFOs
// All wave + performance controls as knobs below the wave field

const BONE       = '#D4C9A8';
const BONE_50    = 'rgba(212, 201, 168, 0.50)';
const BONE_25    = 'rgba(212, 201, 168, 0.25)';
const BONE_15    = 'rgba(212, 201, 168, 0.15)';
const BONE_08    = 'rgba(212, 201, 168, 0.08)';
const BG         = '#0A0A0F';
const FONT_FAMILY = "'JetBrains Mono', monospace";

const LFO_COLORS = [
  'rgba(100, 200, 255, 0.6)', // LFO 1: cyan
  'rgba(255, 160, 80, 0.6)',  // LFO 2: orange
  'rgba(180, 100, 255, 0.6)', // LFO 3: purple
];

export class KnobPanel {
  constructor(canvas, callbacks) {
    this.canvas = canvas;
    this.ctx = canvas.getContext('2d');
    this.callbacks = callbacks;
    this.dpr = 1;
    this.w = 0;
    this.h = 0;
    
    this.draggingKnob = null;
    this.dragStartY = 0;
    this.dragStartVal = 0;
    this.hoveredKnob = null;

    // Main parameter knobs
    this.knobs = [
      { id: 'speed',    label: 'Speed',     min: 0.2,  max: 2,    step: 0.1,  value: 0.9,   format: v => v.toFixed(1) },
      { id: 'freq',     label: 'Freq',      min: 15,   max: 100,  step: 1,    value: 35,    format: v => Math.round(v).toString() },
      { id: 'slit',     label: 'Gap',       min: 0.008,max: 0.15, step: 0.005,value: 0.03,  format: v => v.toFixed(3) },
      { id: 'slitW',    label: 'Width',     min: 0.004,max: 0.06, step: 0.002,value: 0.012, format: v => v.toFixed(3) },
      { id: 'volume',   label: 'Volume',    min: 0,    max: 1,    step: 0.05, value: 0.3,   format: v => Math.round(v * 100) + '%' },
      { id: 'sens',     label: 'Sens',      min: 0.1,  max: 5,    step: 0.1,  value: 1.0,   format: v => v.toFixed(1) },
      { id: 'filter',   label: 'Filter',    min: 200,  max: 12000, step: 100,  value: 7000,  format: v => (v/1000).toFixed(1)+'k' },
      { id: 'reverb',   label: 'Reverb',    min: 0,    max: 1,    step: 0.05, value: 0.4,   format: v => Math.round(v * 100) + '%' },
      { id: 'release',  label: 'Release',   min: 0.05, max: 3,    step: 0.05, value: 0.5,   format: v => v.toFixed(2) + 's' },
    ];

    // Target IDs for LFO routing
    this.targetIds = this.knobs.map(k => k.id);
    this.targetLabels = this.knobs.map(k => k.label);

    // 3 independent LFOs
    this.lfos = [
      { rate: 0, depth: 0, targetIdx: 0, phase: 0 },
      { rate: 0, depth: 0, targetIdx: 1, phase: 0 },
      { rate: 0, depth: 0, targetIdx: 2, phase: 0 },
    ];

    // LFO control knobs (drawn in second row)
    // Each LFO has: rate knob, depth knob, target knob
    this.lfoKnobs = [];
    for (let i = 0; i < 3; i++) {
      this.lfoKnobs.push(
        { id: `lfo${i}Rate`,   lfoIdx: i, param: 'rate',      label: `LFO${i+1} Rate`,  min: 0, max: 8,   step: 0.1, format: v => v.toFixed(1) },
        { id: `lfo${i}Depth`,  lfoIdx: i, param: 'depth',     label: `LFO${i+1} Depth`, min: 0, max: 1,   step: 0.02,format: v => Math.round(v*100)+'%' },
        { id: `lfo${i}Target`, lfoIdx: i, param: 'targetIdx', label: `LFO${i+1} →`,     min: 0, max: this.knobs.length - 1, step: 1, format: v => this.targetLabels[Math.round(v)] || '?' },
      );
    }

    this.knobPositions = [];
    this.lfoKnobPositions = [];
    this.lfoExpanded = false; // collapsed by default
    this.lfoToggleRect = { x: 0, y: 0, w: 0, h: 0 };
    this.lastTime = performance.now();
    
    // LFO modulation state per knob (for visual feedback)
    this.lfoModValues = new Array(this.knobs.length).fill(null); // null = no mod
    this.lfoModColors = new Array(this.knobs.length).fill(null);

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
    this._computeLayout();
  }

  _computeLayout() {
    // Row 1: main knobs
    const knobR = 22;
    const spacing = Math.min(80, (this.w - 40) / this.knobs.length);
    const row1Y = 35;
    const startX1 = Math.max(20, (this.w - this.knobs.length * spacing) / 2 + spacing / 2);
    this.knobPositions = this.knobs.map((k, i) => ({
      x: startX1 + i * spacing, y: row1Y, r: knobR,
    }));

    // LFO toggle button position
    const toggleY = row1Y + knobR + 42;
    this.lfoToggleRect = { x: this.w / 2 - 40, y: toggleY, w: 80, h: 18 };

    // Row 2: LFO knobs (only if expanded)
    if (this.lfoExpanded) {
      const lfoR = 18;
      const lfoSpacing = Math.min(65, (this.w - 40) / this.lfoKnobs.length);
      const row2Y = toggleY + 30;
      const startX2 = Math.max(20, (this.w - this.lfoKnobs.length * lfoSpacing) / 2 + lfoSpacing / 2);
      this.lfoKnobPositions = this.lfoKnobs.map((k, i) => ({
        x: startX2 + i * lfoSpacing, y: row2Y, r: lfoR,
      }));
    } else {
      this.lfoKnobPositions = [];
    }
  }

  _bindEvents() {
    const onDown = (x, y) => {
      // Check LFO toggle
      const tr = this.lfoToggleRect;
      if (x >= tr.x && x <= tr.x + tr.w && y >= tr.y && y <= tr.y + tr.h) {
        this.lfoExpanded = !this.lfoExpanded;
        this._computeLayout();
        // Resize canvas height
        this.canvas.style.height = this.lfoExpanded ? '280px' : '140px';
        this._resize();
        return;
      }
      let idx = this._hitKnob(x, y, this.knobPositions);
      if (idx >= 0) {
        this.draggingKnob = { type: 'main', idx };
        this.dragStartY = y;
        this.dragStartVal = this.knobs[idx].value;
        return;
      }
      idx = this._hitKnob(x, y, this.lfoKnobPositions);
      if (idx >= 0) {
        const lk = this.lfoKnobs[idx];
        const lfo = this.lfos[lk.lfoIdx];
        this.draggingKnob = { type: 'lfo', idx };
        this.dragStartY = y;
        this.dragStartVal = lk.param === 'targetIdx' ? lfo.targetIdx : lfo[lk.param];
      }
    };
    const onMove = (x, y) => {
      if (this.draggingKnob) {
        const { type, idx } = this.draggingKnob;
        if (type === 'main') {
          const knob = this.knobs[idx];
          const dy = this.dragStartY - y;
          const range = knob.max - knob.min;
          let newVal = this.dragStartVal + dy * (range / 120);
          newVal = Math.round(newVal / knob.step) * knob.step;
          newVal = Math.max(knob.min, Math.min(knob.max, newVal));
          knob.value = newVal;
          this._fireCallback(knob.id, newVal);
        } else {
          const lk = this.lfoKnobs[idx];
          const lfo = this.lfos[lk.lfoIdx];
          const dy = this.dragStartY - y;
          const range = lk.max - lk.min;
          let newVal = this.dragStartVal + dy * (range / 120);
          newVal = Math.round(newVal / lk.step) * lk.step;
          newVal = Math.max(lk.min, Math.min(lk.max, newVal));
          if (lk.param === 'targetIdx') lfo.targetIdx = Math.round(newVal);
          else lfo[lk.param] = newVal;
        }
        this.canvas.style.cursor = 'ns-resize';
      } else {
        const h1 = this._hitKnob(x, y, this.knobPositions);
        const h2 = this._hitKnob(x, y, this.lfoKnobPositions);
        this.hoveredKnob = h1 >= 0 ? { type: 'main', idx: h1 } : (h2 >= 0 ? { type: 'lfo', idx: h2 } : null);
        this.canvas.style.cursor = this.hoveredKnob ? 'ns-resize' : 'default';
      }
    };

    this.canvas.addEventListener('mousedown', (e) => { const p = this._getPos(e); onDown(p.x, p.y); });
    this.canvas.addEventListener('mousemove', (e) => { const p = this._getPos(e); onMove(p.x, p.y); });
    this.canvas.addEventListener('mouseup', () => { this.draggingKnob = null; });
    this.canvas.addEventListener('mouseleave', () => { this.hoveredKnob = null; this.draggingKnob = null; });
    
    this.canvas.addEventListener('touchstart', (e) => { e.preventDefault(); const t = e.touches[0]; const p = this._getPos(t); onDown(p.x, p.y); }, { passive: false });
    this.canvas.addEventListener('touchmove', (e) => { e.preventDefault(); const t = e.touches[0]; const p = this._getPos(t); onMove(p.x, p.y); }, { passive: false });
    this.canvas.addEventListener('touchend', () => { this.draggingKnob = null; });
    
    window.addEventListener('resize', () => { this._resize(); });
  }

  _getPos(e) {
    const rect = this.canvas.getBoundingClientRect();
    return { x: e.clientX - rect.left, y: e.clientY - rect.top };
  }

  _hitKnob(x, y, positions) {
    for (let i = 0; i < positions.length; i++) {
      const p = positions[i];
      const dx = x - p.x, dy = y - p.y;
      if (dx * dx + dy * dy < (p.r + 8) * (p.r + 8)) return i;
    }
    return -1;
  }

  _fireCallback(id, val) {
    const cb = this.callbacks;
    if (id === 'speed')    cb.onSpeed?.(val);
    if (id === 'freq')     cb.onFreq?.(val);
    if (id === 'slit')     cb.onSlit?.(val);
    if (id === 'slitW')    cb.onSlitWidth?.(val);
    if (id === 'volume')   cb.onVolume?.(val);
    if (id === 'sens')     cb.onSensitivity?.(val);
    if (id === 'filter')   cb.onFilter?.(val);
    if (id === 'reverb')   cb.onDryWet?.(val);
    if (id === 'release')  cb.onRelease?.(val);
  }

  setKnobValue(id, val) {
    const knob = this.knobs.find(k => k.id === id);
    if (knob) knob.value = val;
  }

  // Run LFOs and apply modulation
  _tickLFOs() {
    const now = performance.now();
    const dt = (now - this.lastTime) / 1000;
    this.lastTime = now;

    // Clear modulation state
    this.lfoModValues.fill(null);
    this.lfoModColors.fill(null);

    for (let li = 0; li < this.lfos.length; li++) {
      const lfo = this.lfos[li];
      if (lfo.rate <= 0 || lfo.depth <= 0) continue;
      lfo.phase += lfo.rate * dt * 2 * Math.PI;
      if (lfo.phase > 1000 * Math.PI) lfo.phase -= 1000 * Math.PI;

      const mod = Math.sin(lfo.phase); // -1 to 1
      const targetKnob = this.knobs[lfo.targetIdx];
      if (!targetKnob) continue;

      const range = targetKnob.max - targetKnob.min;
      const modAmount = mod * lfo.depth * range;
      const modVal = Math.max(targetKnob.min, Math.min(targetKnob.max, targetKnob.value + modAmount));
      
      // Store modulated value and color for visual feedback
      this.lfoModValues[lfo.targetIdx] = modVal;
      this.lfoModColors[lfo.targetIdx] = LFO_COLORS[li];
      
      // Fire callback with modulated value (doesn't change the knob's base value)
      this._fireCallback(targetKnob.id, modVal);
    }
  }

  draw() {
    const ctx = this.ctx;
    const dpr = this.dpr;

    ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
    ctx.save();
    ctx.scale(dpr, dpr);

    this._tickLFOs();

    // Draw main knobs
    for (let i = 0; i < this.knobs.length; i++) {
      const knob = this.knobs[i];
      const pos = this.knobPositions[i];
      if (!pos) continue;
      const active = this.hoveredKnob?.type === 'main' && this.hoveredKnob.idx === i;
      const dragging = this.draggingKnob?.type === 'main' && this.draggingKnob.idx === i;
      
      const modVal = this.lfoModValues[i];
      const modColor = this.lfoModColors[i];
      
      this._drawKnob(ctx, pos.x, pos.y, pos.r, knob.value, knob.min, knob.max, knob.label, knob.format(knob.value), active || dragging, modColor, modVal);
    }

    // LFO toggle button
    const tr = this.lfoToggleRect;
    ctx.fillStyle = 'rgba(212, 201, 168, 0.04)';
    ctx.strokeStyle = BONE_15;
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.roundRect(tr.x, tr.y, tr.w, tr.h, 4);
    ctx.fill();
    ctx.stroke();
    ctx.font = `400 8px ${FONT_FAMILY}`;
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillStyle = BONE_25;
    ctx.fillText(this.lfoExpanded ? '▴ LFO RACK' : '▾ LFO RACK', tr.x + tr.w / 2, tr.y + tr.h / 2);

    if (!this.lfoExpanded) { ctx.restore(); return; }

    // Draw LFO group separators and knobs
    for (let li = 0; li < 3; li++) {
      const baseIdx = li * 3;
      // Group label
      const firstPos = this.lfoKnobPositions[baseIdx];
      if (firstPos) {
        ctx.font = `400 7px ${FONT_FAMILY}`;
        ctx.textAlign = 'left';
        ctx.textBaseline = 'bottom';
        ctx.fillStyle = LFO_COLORS[li];
        ctx.fillText(`LFO ${li + 1}`, firstPos.x - firstPos.r, firstPos.y - firstPos.r - 6);
      }
    }

    for (let i = 0; i < this.lfoKnobs.length; i++) {
      const lk = this.lfoKnobs[i];
      const pos = this.lfoKnobPositions[i];
      if (!pos) continue;
      const lfo = this.lfos[lk.lfoIdx];
      const val = lk.param === 'targetIdx' ? lfo.targetIdx : lfo[lk.param];
      const active = this.hoveredKnob?.type === 'lfo' && this.hoveredKnob.idx === i;
      const dragging = this.draggingKnob?.type === 'lfo' && this.draggingKnob.idx === i;
      
      this._drawKnob(ctx, pos.x, pos.y, pos.r, val, lk.min, lk.max, lk.label.replace(/LFO\d /, ''), lk.format(val), active || dragging, LFO_COLORS[lk.lfoIdx]);
    }

    ctx.restore();
  }

  _drawKnob(ctx, x, y, r, value, min, max, label, displayVal, active, accentColor, modValue) {
    const t = (value - min) / (max - min);
    
    const startAngle = 0.75 * Math.PI;
    const endAngle = 2.25 * Math.PI;
    const sweepAngle = endAngle - startAngle;
    const valueAngle = startAngle + t * sweepAngle;

    const hasLfo = accentColor != null && modValue != null;
    const baseColor = active ? BONE : BONE_50;
    const trackColor = hasLfo ? accentColor.replace('0.6', '0.12') : BONE_15;

    // Outer ring (track)
    ctx.beginPath();
    ctx.arc(x, y, r, startAngle, endAngle);
    ctx.strokeStyle = trackColor;
    ctx.lineWidth = 3;
    ctx.lineCap = 'round';
    ctx.stroke();

    // Base value arc (bone)
    ctx.beginPath();
    ctx.arc(x, y, r, startAngle, valueAngle);
    ctx.strokeStyle = hasLfo ? BONE_25 : baseColor;
    ctx.lineWidth = 3;
    ctx.lineCap = 'round';
    ctx.stroke();

    // LFO modulated arc (colored, overlaid)
    if (hasLfo) {
      const modT = (modValue - min) / (max - min);
      const modAngle = startAngle + Math.max(0, Math.min(1, modT)) * sweepAngle;
      ctx.beginPath();
      ctx.arc(x, y, r, startAngle, modAngle);
      ctx.strokeStyle = accentColor;
      ctx.lineWidth = 3;
      ctx.lineCap = 'round';
      ctx.stroke();

      // Modulated indicator dot on arc
      const mDotX = x + Math.cos(modAngle) * (r - 5);
      const mDotY = y + Math.sin(modAngle) * (r - 5);
      ctx.beginPath();
      ctx.arc(mDotX, mDotY, 2.5, 0, Math.PI * 2);
      ctx.fillStyle = accentColor.replace('0.6', '1.0');
      ctx.fill();
    } else {
      // Base indicator dot
      const dotX = x + Math.cos(valueAngle) * (r - 5);
      const dotY = y + Math.sin(valueAngle) * (r - 5);
      ctx.beginPath();
      ctx.arc(dotX, dotY, 2, 0, Math.PI * 2);
      ctx.fillStyle = baseColor;
      ctx.fill();
    }

    // Inner fill
    ctx.beginPath();
    ctx.arc(x, y, r - 8, 0, Math.PI * 2);
    ctx.fillStyle = active ? 'rgba(212, 201, 168, 0.05)' : 'rgba(212, 201, 168, 0.02)';
    ctx.fill();

    // Label
    ctx.font = `400 ${r < 20 ? 7 : 8}px ${FONT_FAMILY}`;
    ctx.textAlign = 'center';
    ctx.textBaseline = 'top';
    ctx.fillStyle = BONE_25;
    ctx.letterSpacing = '0.5px';
    ctx.fillText(label.toUpperCase(), x, y + r + 4);

    // Glowing dot below label (LFO indicator)
    if (hasLfo) {
      const dotY2 = y + r + (r < 20 ? 16 : 18);
      // Glow
      ctx.beginPath();
      ctx.arc(x, dotY2, 4, 0, Math.PI * 2);
      ctx.fillStyle = accentColor.replace('0.6', '0.25');
      ctx.fill();
      // Core
      ctx.beginPath();
      ctx.arc(x, dotY2, 2, 0, Math.PI * 2);
      ctx.fillStyle = accentColor.replace('0.6', '0.9');
      ctx.fill();
    }

    // Value
    ctx.font = `400 ${r < 20 ? 8 : 9}px ${FONT_FAMILY}`;
    ctx.textBaseline = 'middle';
    ctx.fillStyle = active ? BONE : BONE_50;
    ctx.letterSpacing = '0px';
    ctx.fillText(displayVal, x, y);
  }
}
