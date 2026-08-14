// Audio Engine — sonifies the detector's instantaneous intensity pattern
// Target range: 40 Hz – 7 kHz
// Chain: AudioWorklet/ScriptProcessor → Compressor → Resonant Peaks → Delay/Reverb → HPF → LPF → Gain → Output
// Features: AudioWorklet (low-latency), ScriptProcessor fallback, recording, scale mode

export class AudioEngine {
  constructor() {
    this.ctx = null;
    this.gainNode = null;
    this.running = false;
    this.volume = 0.3;
    this.filterCutoff = 3500;
    this.dryWet = 0.4;
    this.scaleMode = false;
    this.droneMode = false;
    this.useWorklet = false;
    
    // 3 stereo voices: left (UV 0.88), center (0.92), right (0.96)
    this.voices = [];
    this.smoothEnergy = 0;
    
    // Recording
    this.recording = false;
    this.mediaRecorder = null;
    this.recordedChunks = [];
  }

  async start() {
    if (this.ctx) return;
    
    this.ctx = new (window.AudioContext || window.webkitAudioContext)();
    const ctx = this.ctx;
    
    // === Signal chain (bottom to top) ===
    
    // Final output gain
    this.gainNode = ctx.createGain();
    this.gainNode.gain.value = this.volume;
    this.gainNode.connect(ctx.destination);
    
    // High-pass filter — 20 Hz floor (sub-bass allowed through)
    this.hpFilter = ctx.createBiquadFilter();
    this.hpFilter.type = 'highpass';
    this.hpFilter.frequency.value = 20;
    this.hpFilter.Q.value = 0.5;
    this.hpFilter.connect(this.gainNode);
    
    // Sub-harmonic oscillator — sine at half the resonant freq
    this.subOsc = ctx.createOscillator();
    this.subOsc.type = 'sine';
    this.subOsc.frequency.value = 60; // will track resonant/2
    this.subGain = ctx.createGain();
    this.subGain.gain.value = 0.0; // off by default, enabled when energy present
    this.subOsc.connect(this.subGain);
    this.subGain.connect(this.gainNode); // bypass LPF — this IS the low end
    this.subOsc.start();

    // Low-pass filter — ceiling at 7 kHz
    this.lpFilter = ctx.createBiquadFilter();
    this.lpFilter.type = 'lowpass';
    this.lpFilter.frequency.value = this.filterCutoff;
    this.lpFilter.Q.value = 0.5;
    this.lpFilter.connect(this.hpFilter);
    
    // ═══ FDN Cathedral Reverb ═══
    // 4-tap feedback delay network with darkening feedback
    // Prime-length delays for dense, non-repeating tail
    const fdnTimes = [1.37, 1.71, 2.23, 3.19]; // seconds, mutually prime-ish
    const fdnFeedback = 0.55; // per-tap feedback (yields ~6-8s tail)
    const fdnDarken = 2800;   // LPF cutoff in feedback path (darkens each loop)
    
    this.fdnDelays = [];
    this.fdnFeedbacks = [];
    this.fdnFilters = [];
    this.fdnWet = ctx.createGain();
    this.fdnWet.gain.value = 0.0; // controlled by Reverb knob
    this.fdnWet.connect(this.lpFilter);
    
    this.fdnDry = ctx.createGain();
    this.fdnDry.gain.value = 1.0;
    this.fdnDry.connect(this.lpFilter);
    
    // FDN input — all 4 taps receive the same signal
    this.fdnInput = ctx.createGain();
    this.fdnInput.gain.value = 0.25; // split equally across 4 taps
    
    for (let i = 0; i < 4; i++) {
      const delay = ctx.createDelay(5.0);
      delay.delayTime.value = fdnTimes[i];
      
      const fb = ctx.createGain();
      fb.gain.value = fdnFeedback;
      
      // Darkening filter in feedback path — each loop loses highs
      const darken = ctx.createBiquadFilter();
      darken.type = 'lowpass';
      darken.frequency.value = fdnDarken;
      darken.Q.value = 0.5;
      
      // Signal flow: fdnInput → delay → (output to wet) + (feedback: → darken → fb → delay)
      this.fdnInput.connect(delay);
      delay.connect(this.fdnWet);        // tap output → wet mix
      delay.connect(darken);             // feedback path
      darken.connect(fb);
      fb.connect(delay);                 // loop back
      
      // Cross-feed: each tap feeds into the next (circular) for density
      // This approximates the Hadamard mixing matrix
      if (i > 0) {
        const crossGain = ctx.createGain();
        crossGain.gain.value = 0.15;
        this.fdnDelays[i - 1].connect(crossGain);
        crossGain.connect(delay);
        this['_fdnCross' + i] = crossGain; // prevent GC
      }
      
      this.fdnDelays.push(delay);
      this.fdnFeedbacks.push(fb);
      this.fdnFilters.push(darken);
    }
    // Close the cross-feed loop: tap 3 → tap 0
    const crossLast = ctx.createGain();
    crossLast.gain.value = 0.15;
    this.fdnDelays[3].connect(crossLast);
    crossLast.connect(this.fdnDelays[0]);
    this._fdnCross0 = crossLast;
    
    // Resonant peak (fundamental)
    this.bpFilter = ctx.createBiquadFilter();
    this.bpFilter.type = 'peaking';
    this.bpFilter.frequency.value = 300;
    this.bpFilter.Q.value = 1.2;
    this.bpFilter.gain.value = 3;
    this.bpFilter.connect(this.fdnDry);
    this.bpFilter.connect(this.fdnInput);
    
    // Second resonant peak — harmonic fifth (scale mode)
    this.bpFilter2 = ctx.createBiquadFilter();
    this.bpFilter2.type = 'peaking';
    this.bpFilter2.frequency.value = 450;
    this.bpFilter2.Q.value = 2.0;
    this.bpFilter2.gain.value = 0;
    this.bpFilter2.connect(this.fdnDry);
    this.bpFilter2.connect(this.fdnInput);
    
    // Drone mode partials — harmonic series (2f, 3f, 4f)
    // bpFilter = fundamental (f), these are the upper partials
    this.droneFilters = [];
    for (let h = 2; h <= 4; h++) {
      const f = ctx.createBiquadFilter();
      f.type = 'peaking';
      f.frequency.value = 300 * h;
      f.Q.value = 4.0;
      f.gain.value = 0; // off by default
      f.connect(this.fdnDry);
      f.connect(this.fdnInput);
      this.droneFilters.push(f);
    }
    
    // Compressor
    this.compressor = ctx.createDynamicsCompressor();
    this.compressor.threshold.value = -20;
    this.compressor.knee.value = 12;
    this.compressor.ratio.value = 4;
    this.compressor.attack.value = 0.01;
    this.compressor.release.value = 0.15;
    this.compressor.connect(this.bpFilter);
    this.compressor.connect(this.bpFilter2);
    for (const df of this.droneFilters) this.compressor.connect(df);
    
    // === 3 Stereo Voices ===
    // Each voice: ScriptProcessor → StereoPanner → compressor
    const panPositions = [-0.7, 0.0, 0.7]; // L, C, R
    const voiceGainValues = [0.7, 1.0, 0.7]; // center louder
    
    this.voices = [];
    for (let v = 0; v < 3; v++) {
      const voice = {
        bufferA: null, bufferB: null,
        activeBuffer: 'A', crossfade: 1.0,
        readPos: 0, prevSample: 0, localEnergy: 0,
        scriptNode: null, panner: null, gain: null
      };
      
      const bufferSize = 2048;
      voice.scriptNode = ctx.createScriptProcessor(bufferSize, 0, 1);
      
      // Closure to capture voice state
      const voiceRef = voice;
      const self = this;
      voice.scriptNode.onaudioprocess = (e) => {
        const output = e.outputBuffer.getChannelData(0);
        const data = voiceRef.activeBuffer === 'A' ? voiceRef.bufferA : voiceRef.bufferB;
        const fadeData = voiceRef.activeBuffer === 'A' ? voiceRef.bufferB : voiceRef.bufferA;
        
        if (!data || data.length === 0) {
          output.fill(0);
          return;
        }
        
        const len = data.length;
        const playbackRate = 1.0;
        
        // Local energy
        let totalEnergy = 0;
        for (let i = 0; i < len; i++) totalEnergy += data[i] * data[i];
        totalEnergy = Math.sqrt(totalEnergy / len);
        voiceRef.localEnergy += (totalEnergy - voiceRef.localEnergy) * 0.03;
        const envGain = Math.min(1.0, voiceRef.localEnergy * 15.0);
        
        for (let i = 0; i < output.length; i++) {
          const posFloor = Math.floor(voiceRef.readPos);
          const frac = voiceRef.readPos - posFloor;
          const idx0 = ((posFloor % len) + len) % len;
          const idx1 = ((posFloor + 1) % len + len) % len;
          
          let sample = (data[idx0] || 0) + ((data[idx1] || 0) - (data[idx0] || 0)) * frac;
          
          if (voiceRef.crossfade < 1.0 && fadeData && fadeData.length > 0) {
            const fLen = fadeData.length;
            const fIdx0 = ((posFloor % fLen) + fLen) % fLen;
            const fIdx1 = ((posFloor + 1) % fLen + fLen) % fLen;
            const fVal = (fadeData[fIdx0] || 0) + ((fadeData[fIdx1] || 0) - (fadeData[fIdx0] || 0)) * frac;
            sample = sample * voiceRef.crossfade + fVal * (1.0 - voiceRef.crossfade);
          }
          
          sample = Math.tanh(sample * 1.8) * envGain;
          sample = voiceRef.prevSample * 0.1 + sample * 0.9;
          voiceRef.prevSample = sample;
          
          output[i] = sample * 0.35;
          
          voiceRef.readPos += playbackRate;
          if (voiceRef.readPos >= len * 1000) voiceRef.readPos -= len * 1000;
        }
        
        if (voiceRef.crossfade < 1.0) {
          voiceRef.crossfade = Math.min(1.0, voiceRef.crossfade + output.length / ctx.sampleRate * 8.0);
        }
        
        // Update global energy from center voice (index 1)
        if (v === 1) {
          self.smoothEnergy = voiceRef.localEnergy;
          self._modulateFilters();
        }
      };
      
      // Per-voice gain
      voice.gain = ctx.createGain();
      voice.gain.gain.value = voiceGainValues[v];
      
      // Stereo panner
      voice.panner = ctx.createStereoPanner();
      voice.panner.pan.value = panPositions[v];
      
      // Connect: script → gain → panner → compressor
      voice.scriptNode.connect(voice.gain);
      voice.gain.connect(voice.panner);
      voice.panner.connect(this.compressor);
      
      this.voices.push(voice);
    }
    
    this.running = true;
  }

  _modulateFilters() {
    if (!this.ctx) return;
    const ctx = this.ctx;
    const t = ctx.currentTime;
    
    let targetFreq = 60 + this.smoothEnergy * 2000;
    targetFreq = Math.min(3500, Math.max(40, targetFreq));
    
    if (this.droneMode) {
      // Drone: lock to harmonic series, energy modulates partial balance
      targetFreq = this.scaleMode ? this._quantizeToScale(targetFreq) : targetFreq;
      
      // Fundamental: always present, tight Q
      this.bpFilter.Q.setTargetAtTime(5.0, t, 0.1);
      this.bpFilter.gain.setTargetAtTime(10, t, 0.1);
      this.bpFilter.frequency.setTargetAtTime(targetFreq, t, 0.12);
      
      // Upper partials: gain scales with energy
      // Low energy = mostly fundamental. High energy = upper partials emerge.
      const energy = this.smoothEnergy;
      const partialGains = [
        Math.min(8, energy * 40),    // 2f: emerges early
        Math.min(6, energy * 60),    // 3f: emerges mid
        Math.min(4, energy * 80),    // 4f: only at high energy
      ];
      for (let i = 0; i < this.droneFilters.length; i++) {
        const h = i + 2;
        this.droneFilters[i].frequency.setTargetAtTime(
          Math.min(3500, targetFreq * h), t, 0.12
        );
        this.droneFilters[i].gain.setTargetAtTime(partialGains[i], t, 0.1);
      }
      
      // Disable scale mode's fifth in drone mode
      this.bpFilter2.gain.setTargetAtTime(0, t, 0.1);
      
    } else if (this.scaleMode) {
      targetFreq = this._quantizeToScale(targetFreq);
      this.bpFilter.Q.setTargetAtTime(3.0, t, 0.1);
      this.bpFilter.gain.setTargetAtTime(8, t, 0.1);
      this.bpFilter2.frequency.setTargetAtTime(
        Math.min(3500, targetFreq * 1.5), t, 0.08
      );
      this.bpFilter2.gain.setTargetAtTime(4, t, 0.1);
      // Disable drone partials
      for (const df of this.droneFilters) df.gain.setTargetAtTime(0, t, 0.1);
      
    } else {
      this.bpFilter.Q.setTargetAtTime(1.2, t, 0.1);
      this.bpFilter.gain.setTargetAtTime(3, t, 0.1);
      this.bpFilter2.gain.setTargetAtTime(0, t, 0.1);
      for (const df of this.droneFilters) df.gain.setTargetAtTime(0, t, 0.1);
    }
    
    if (!this.droneMode) {
      this.bpFilter.frequency.setTargetAtTime(targetFreq, t, 0.08);
    }
    
    // Sub-harmonic oscillator: track half the resonant freq
    // Gain proportional to energy (subtle, -12dB below main signal)
    if (this.subOsc) {
      const subFreq = Math.max(20, targetFreq * 0.5);
      this.subOsc.frequency.setTargetAtTime(subFreq, t, 0.12);
      const subLevel = Math.min(0.15, this.smoothEnergy * 1.5); // ~-12dB
      this.subGain.gain.setTargetAtTime(subLevel, t, 0.08);
    }
  }

  stop() {
    // Cleanup voices
    for (const voice of this.voices) {
      if (voice.scriptNode) { voice.scriptNode.disconnect(); }
      if (voice.gain) { voice.gain.disconnect(); }
      if (voice.panner) { voice.panner.disconnect(); }
    }
    this.voices = [];
    if (this.compressor) { this.compressor.disconnect(); this.compressor = null; }
    if (this.bpFilter) { this.bpFilter.disconnect(); this.bpFilter = null; }
    if (this.bpFilter2) { this.bpFilter2.disconnect(); this.bpFilter2 = null; }
    // FDN cleanup
    if (this.fdnDelays) { this.fdnDelays.forEach(d => d.disconnect()); this.fdnDelays = null; }
    if (this.fdnFeedbacks) { this.fdnFeedbacks.forEach(f => f.disconnect()); this.fdnFeedbacks = null; }
    if (this.fdnFilters) { this.fdnFilters.forEach(f => f.disconnect()); this.fdnFilters = null; }
    if (this.fdnWet) { this.fdnWet.disconnect(); this.fdnWet = null; }
    if (this.fdnDry) { this.fdnDry.disconnect(); this.fdnDry = null; }
    if (this.fdnInput) { this.fdnInput.disconnect(); this.fdnInput = null; }
    if (this.lpFilter) { this.lpFilter.disconnect(); this.lpFilter = null; }
    if (this.hpFilter) { this.hpFilter.disconnect(); this.hpFilter = null; }
    if (this.subOsc) { this.subOsc.stop(); this.subOsc.disconnect(); this.subOsc = null; }
    if (this.subGain) { this.subGain.disconnect(); this.subGain = null; }
    if (this.droneFilters) { this.droneFilters.forEach(f => f.disconnect()); this.droneFilters = null; }
    if (this.gainNode) { this.gainNode.disconnect(); this.gainNode = null; }
    if (this.ctx) { this.ctx.close(); this.ctx = null; }
    this.running = false;
  }

  // Update a specific voice (0=left, 1=center, 2=right)
  updateDetectorVoice(voiceIdx, instant) {
    const voice = this.voices[voiceIdx];
    if (!voice) return;
    if (voice.activeBuffer === 'A') {
      voice.bufferB = instant;
      voice.activeBuffer = 'B';
    } else {
      voice.bufferA = instant;
      voice.activeBuffer = 'A';
    }
    voice.crossfade = 0.0;
  }

  // Legacy single-voice update (feeds center voice)
  updateDetectorData(instant) {
    this.updateDetectorVoice(1, instant);
  }

  // --- Recording (Video + Audio) ---
  // Captures the wave canvas visuals + audio output as a WebM video
  startRecording(waveCanvas) {
    if (!this.ctx || this.recording) return;
    
    try {
      // Video stream from canvas at 30fps
      const videoStream = waveCanvas.captureStream(30);
      
      // Audio stream from output
      const audioDest = this.ctx.createMediaStreamDestination();
      this.gainNode.connect(audioDest);
      this._recAudioDest = audioDest;
      
      // Combine video + audio into one stream
      const combined = new MediaStream([
        ...videoStream.getTracks(),
        ...audioDest.stream.getTracks()
      ]);
      
      this._recChunks = [];
      // Try VP9+Opus, fallback to VP8+Opus, fallback to default
      let mimeType = 'video/webm;codecs=vp9,opus';
      if (!MediaRecorder.isTypeSupported(mimeType)) {
        mimeType = 'video/webm;codecs=vp8,opus';
      }
      if (!MediaRecorder.isTypeSupported(mimeType)) {
        mimeType = 'video/webm';
      }
      
      this._mediaRecorder = new MediaRecorder(combined, { mimeType });
      this._mediaRecorder.ondataavailable = (e) => {
        if (e.data.size > 0) this._recChunks.push(e.data);
      };
      this._mediaRecorder.onstop = () => {
        const blob = new Blob(this._recChunks, { type: 'video/webm' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `fringe-${Date.now()}.webm`;
        a.click();
        URL.revokeObjectURL(url);
        // Disconnect audio tap
        try { this.gainNode.disconnect(this._recAudioDest); } catch(e) {}
        this._recAudioDest = null;
      };
      this._mediaRecorder.start(100);
      this.recording = true;
    } catch (err) {
      console.warn('Video recording not supported:', err);
      this.recording = false;
    }
  }

  stopRecording() {
    if (!this.recording || !this._mediaRecorder) return;
    this._mediaRecorder.stop();
    this.recording = false;
  }

  setVolume(v) {
    this.volume = v;
    if (this.gainNode && this.ctx) {
      this.gainNode.gain.setTargetAtTime(v, this.ctx.currentTime, 0.05);
    }
  }

  setFilterCutoff(freq) {
    this.filterCutoff = freq;
    if (this.lpFilter && this.ctx) {
      this.lpFilter.frequency.setTargetAtTime(freq, this.ctx.currentTime, 0.05);
    }
  }

  setDryWet(mix) {
    this.dryWet = mix;
    if (this.fdnDry && this.ctx) {
      // Crossfade: dry attenuates, FDN wet increases
      this.fdnDry.gain.setTargetAtTime(1.0 - mix * 0.6, this.ctx.currentTime, 0.05);
      this.fdnWet.gain.setTargetAtTime(mix * 0.7, this.ctx.currentTime, 0.05);
    }
  }

  setScaleMode(on) {
    this.scaleMode = on;
  }

  setDroneMode(on) {
    this.droneMode = on;
  }

  _quantizeToScale(freq) {
    const semitones = [0, 2, 4, 7, 9]; // C pentatonic
    const A4 = 440;
    const semiFromA4 = 12 * Math.log2(freq / A4);
    const octave = Math.floor((semiFromA4 + 3) / 12);
    const noteInOctave = ((semiFromA4 % 12) + 12) % 12;
    
    let bestDist = 999;
    let bestSemi = 0;
    for (const s of semitones) {
      let d = Math.abs(noteInOctave - s);
      if (d > 6) d = 12 - d;
      if (d < bestDist) {
        bestDist = d;
        bestSemi = s;
      }
    }
    
    const snappedSemiFromA4 = octave * 12 + bestSemi - 3;
    const snappedFreq = A4 * Math.pow(2, snappedSemiFromA4 / 12);
    return Math.min(3500, Math.max(40, snappedFreq));
  }
}
