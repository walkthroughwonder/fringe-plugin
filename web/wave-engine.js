// Wave Engine — WebGL 2 wave simulation for optics
// Uses 2-texture ping-pong (current + previous amplitude) for standard FDTD wave equation
// Rendering: Cathedral color grading with bloom

export class WaveEngine {
  constructor(canvas) {
    this.canvas = canvas;
    this.gl = canvas.getContext('webgl2', { 
      antialias: false, 
      alpha: false,
      preserveDrawingBuffer: false 
    });
    
    if (!this.gl) throw new Error('WebGL 2 not supported');
    
    this.width = 0;
    this.height = 0;
    this.time = 0;
    this.simTime = 0; // simulation-internal clock (fixed timestep)
    this.speedMultiplier = 0.9;

    // Source
    this.sourceFreq = 35.0;
    this.sourceX = -0.38; // UV-offset coords (centered at 0)
    this.sourceType = 'plane'; // 'plane' or 'point'
    this.continuousSource = true;
    
    // Detector
    this.detectorX = 0.92; // UV x-position — near right edge of screen
    this.detectorSensitivity = 1.0; // multiplier from UI slider
    
    // LFO
    this.lfoRate = 0.0;  // Hz (0 = off)
    this.lfoDepth = 0.0; // 0-1, modulates source frequency or slit
    this.lfoTarget = 0;  // 0=freq, 1=slit, 2=both
    this._lfoValue = 0;  // current LFO output (-1 to 1)
    
    // Envelope release
    this.envelopeRelease = 0.5; // seconds
    this._envelopeLevel = 1.0;  // current envelope value
    this._releasing = false;
    
    // Source amplitude (Feature 6: Velocity)
    // Base amplitude scalar; multiplied by envelope. Default 0.02.
    this.sourceAmplitude = 0.02;
    this.saturation = 1.5;    // 50% boost by default

    // Called after the engine's internal resize destroys/recreates buffers.
    // The speed texture is reset to all-1s in that path, so the host must
    // rebuild optics/walls whenever this fires.
    this.onResize = null;

    this._init();
  }

  _init() {
    const gl = this.gl;
    
    gl.getExtension('EXT_color_buffer_float');
    gl.getExtension('EXT_float_blend');
    gl.getExtension('OES_texture_float_linear');
    
    this._resize();
    
    // Full-screen quad
    const quadVerts = new Float32Array([-1,-1, 1,-1, 1,1, 1,1, -1,1, -1,-1]);
    const texCoords = new Float32Array([0,0, 1,0, 1,1, 1,1, 0,1, 0,0]);
    
    this.vao = gl.createVertexArray();
    gl.bindVertexArray(this.vao);
    
    const posBuf = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, posBuf);
    gl.bufferData(gl.ARRAY_BUFFER, quadVerts, gl.STATIC_DRAW);
    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);
    
    const texBuf = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, texBuf);
    gl.bufferData(gl.ARRAY_BUFFER, texCoords, gl.STATIC_DRAW);
    gl.enableVertexAttribArray(1);
    gl.vertexAttribPointer(1, 2, gl.FLOAT, false, 0, 0);
    
    this.waveProgram = this._createWaveProgram();
    this.renderProgram = this._createRenderProgram();
    this.bloomExtractProgram = this._createBloomExtractProgram();
    this.bloomBlurProgram = this._createBloomBlurProgram();
    this.compositeProgram = this._createCompositeProgram();
    
    this._createBuffers();
    this._clearSim();
  }

  _resize() {
    const dpr = Math.min(window.devicePixelRatio, 1.5);
    const w = Math.floor(this.canvas.clientWidth * dpr);
    const h = Math.floor(this.canvas.clientHeight * dpr);
    
    if (w === this.width && h === this.height) return false;
    
    this.canvas.width = w;
    this.canvas.height = h;
    this.width = w;
    this.height = h;
    this.gl.viewport(0, 0, w, h);
    
    return true;
  }

  _createShader(type, source) {
    const gl = this.gl;
    const shader = gl.createShader(type);
    gl.shaderSource(shader, source);
    gl.compileShader(shader);
    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
      console.error('Shader error:', gl.getShaderInfoLog(shader));
      console.error('Source:', source);
      gl.deleteShader(shader);
      return null;
    }
    return shader;
  }

  _createProgram(vertSrc, fragSrc) {
    const gl = this.gl;
    const vs = this._createShader(gl.VERTEX_SHADER, vertSrc);
    const fs = this._createShader(gl.FRAGMENT_SHADER, fragSrc);
    const prog = gl.createProgram();
    gl.attachShader(prog, vs);
    gl.attachShader(prog, fs);
    gl.linkProgram(prog);
    if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) {
      console.error('Link error:', gl.getProgramInfoLog(prog));
    }
    return prog;
  }

  get _vertSrc() {
    return `#version 300 es
      layout(location = 0) in vec2 a_pos;
      layout(location = 1) in vec2 a_uv;
      out vec2 v_uv;
      void main() {
        v_uv = a_uv;
        gl_Position = vec4(a_pos, 0.0, 1.0);
      }
    `;
  }

  // ---- WAVE UPDATE SHADER ----
  // Standard FDTD: u[t+1] = 2*u[t] - u[t-1] + c^2 * laplacian(u[t])
  // Two textures: u_curr (current frame), u_prev (previous frame)
  // Speed map: u_speed (0 = wall, 1 = vacuum)
  // Output: next amplitude
  _createWaveProgram() {
    const frag = `#version 300 es
      precision highp float;
      
      uniform vec2 u_res;
      uniform float u_speedMult;
      uniform sampler2D u_curr;   // amplitude at time t
      uniform sampler2D u_prev;   // amplitude at time t-1
      uniform sampler2D u_speed;  // speed map
      
      // Source
      uniform float u_time;
      uniform float u_sourceFreq;
      uniform float u_sourceX;
      uniform int u_sourceMode;   // 0=plane, 1=point
      uniform int u_continuous;
      uniform float u_sourceAmp;   // envelope-controlled amplitude
      
      // Detector
      uniform float u_detectorX;   // UV x-position of detector screen
      uniform float u_detectorSens; // sensitivity multiplier
      
      in vec2 v_uv;
      out vec4 o_next;
      
      void main() {
        vec2 px = vec2(1.0) / u_res;
        
        float curr  = texture(u_curr, v_uv).r;
        float prev  = texture(u_prev, v_uv).r;
        float up    = texture(u_curr, v_uv + vec2(0.0, px.y)).r;
        float down  = texture(u_curr, v_uv - vec2(0.0, px.y)).r;
        float right = texture(u_curr, v_uv + vec2(px.x, 0.0)).r;
        float left  = texture(u_curr, v_uv - vec2(px.x, 0.0)).r;
        
        float spd = texture(u_speed, v_uv).r * u_speedMult;
        
        // Wall: topological obstruction — force amplitude to zero
        // (setting c²=0 alone leaves residual oscillation from 2*curr-prev)
        if (spd < 0.01) {
          o_next = vec4(0.0, texture(u_curr, v_uv).g, 0.0, 1.0);
          return;
        }
        
        // Wave speed squared (c^2). For stability: c^2 < 0.5 
        float c2 = spd * 0.24;
        
        // Standard FDTD 2nd order
        float laplacian = up + down + left + right - 4.0 * curr;
        float next = 2.0 * curr - prev + c2 * laplacian;
        
        // Interior damping
        next *= 0.9995;
        
        // Absorbing boundary — sponge at top/bottom only
        // Left: source is near edge, no sponge. Right: detector handles it.
        float borderWidth = 0.08;
        float bTop = v_uv.y;
        float bBot = 1.0 - v_uv.y;
        float bmin = min(bTop, bBot);
        
        if (bmin < borderWidth) {
          float t = bmin / borderWidth;
          float sponge = mix(0.15, 1.0, t * t);
          next *= sponge;
        }
        
        // Hard zero at outermost edges (top/bottom only)
        next *= smoothstep(0.0, 0.02, min(bTop, bBot));
        
        // Right side: only kill past detector
        if (v_uv.x > u_detectorX + 0.01) {
          float rFade = 1.0 - smoothstep(u_detectorX, 1.0, v_uv.x);
          next *= rFade;
        }
        
        // Source injection (soft source — additive)
        // u_sourceX is now in raw UV space (0–1), not aspect-corrected
        if (u_continuous == 1) {
          float signal = sin(u_sourceFreq * u_time);
          
          float envelope = 0.0;
          if (u_sourceMode == 0) {
            // Plane wave: vertical strip in raw UV
            float dx = abs(v_uv.x - u_sourceX);
            envelope = smoothstep(0.005, 0.0, dx);
          } else {
            // Point source in raw UV
            vec2 diff = v_uv - vec2(u_sourceX, 0.5);
            diff.x *= u_res.x / u_res.y; // correct for aspect
            float r = length(diff);
            envelope = smoothstep(0.015, 0.0, r);
          }
          
          // Soft source: add signal (doesn't suppress scattered waves)
          next += signal * envelope * u_sourceAmp;
        }
        
        // --- Detector: record intensity + hard absorb (99.9%) ---
        float prevAccum = texture(u_curr, v_uv).g;
        float accum = prevAccum;
        
        float detDist = abs(v_uv.x - u_detectorX);
        float detAccumMask = smoothstep(0.005, 0.0, detDist);
        
        // Detector sensitivity: speed × sensitivity slider × base gain
        float detGain = u_speedMult * u_detectorSens * 20.0;
        
        // Green channel: accumulated total (running sum of amp²)
        accum += detAccumMask * next * next * 0.05 * detGain;
        
        // Blue channel: instantaneous hit intensity (rewritten each substep)
        float prevInstant = texture(u_curr, v_uv).b * 0.75;
        float instant = prevInstant + detAccumMask * next * next * 20.0 * detGain;
        
        // Past the detector: hard kill — zero amplitude
        if (v_uv.x > u_detectorX) {
          next = 0.0;
        }
        
        o_next = vec4(next, accum, instant, 1.0);
      }
    `;
    return this._createProgram(this._vertSrc, frag);
  }

  // ---- RENDER SHADER ----
  // Reads current amplitude, applies Cathedral color grading
  _createRenderProgram() {
    const frag = `#version 300 es
      precision highp float;
      
      uniform vec2 u_res;
      uniform sampler2D u_curr;
      uniform sampler2D u_speed;
      uniform float u_detectorX;
      uniform float u_sourceX;
      uniform float u_saturation;  // 1.0 = normal, 1.5 = +50%
      
      in vec2 v_uv;
      out vec4 o_color;
      
      // Saturation boost via luminance lerp
      vec3 saturate(vec3 c, float s) {
        float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
        return mix(vec3(lum), c, s);
      }
      
      // Age-shifted Cathedral palette
      // Young (near source): cyan/teal
      // Mid-field: amber/gold  
      // Old (near detector): rose/magenta
      
      vec3 posYoung(float t) {
        // Cyan-white for fresh waves
        vec3 lo  = vec3(0.03, 0.06, 0.08);
        vec3 mid = vec3(0.12, 0.35, 0.42);
        vec3 hi  = vec3(0.30, 0.78, 0.85);
        vec3 pk  = vec3(0.70, 0.95, 1.00);
        if (t < 0.25) return mix(lo, mid, t / 0.25);
        if (t < 0.6)  return mix(mid, hi, (t - 0.25) / 0.35);
        return mix(hi, pk, (t - 0.6) / 0.4);
      }
      
      vec3 posMid(float t) {
        // Warm amber/gold
        vec3 lo  = vec3(0.08, 0.06, 0.03);
        vec3 mid = vec3(0.45, 0.35, 0.15);
        vec3 hi  = vec3(0.95, 0.78, 0.30);
        vec3 pk  = vec3(1.0,  0.92, 0.70);
        if (t < 0.25) return mix(lo, mid, t / 0.25);
        if (t < 0.6)  return mix(mid, hi, (t - 0.25) / 0.35);
        return mix(hi, pk, (t - 0.6) / 0.4);
      }
      
      vec3 posOld(float t) {
        // Rose/magenta for aged waves
        vec3 lo  = vec3(0.08, 0.03, 0.05);
        vec3 mid = vec3(0.42, 0.15, 0.25);
        vec3 hi  = vec3(0.85, 0.30, 0.50);
        vec3 pk  = vec3(1.00, 0.70, 0.80);
        if (t < 0.25) return mix(lo, mid, t / 0.25);
        if (t < 0.6)  return mix(mid, hi, (t - 0.25) / 0.35);
        return mix(hi, pk, (t - 0.6) / 0.4);
      }
      
      vec3 negYoung(float t) {
        // Deep teal for fresh negative phase
        vec3 lo  = vec3(0.01, 0.03, 0.04);
        vec3 mid = vec3(0.04, 0.12, 0.18);
        vec3 hi  = vec3(0.10, 0.30, 0.40);
        vec3 pk  = vec3(0.20, 0.55, 0.60);
        if (t < 0.25) return mix(lo, mid, t / 0.25);
        if (t < 0.6)  return mix(mid, hi, (t - 0.25) / 0.35);
        return mix(hi, pk, (t - 0.6) / 0.4);
      }
      
      vec3 negMid(float t) {
        // Cool slate
        vec3 lo  = vec3(0.02, 0.03, 0.04);
        vec3 mid = vec3(0.06, 0.09, 0.14);
        vec3 hi  = vec3(0.15, 0.28, 0.40);
        vec3 pk  = vec3(0.30, 0.55, 0.65);
        if (t < 0.25) return mix(lo, mid, t / 0.25);
        if (t < 0.6)  return mix(mid, hi, (t - 0.25) / 0.35);
        return mix(hi, pk, (t - 0.6) / 0.4);
      }
      
      vec3 negOld(float t) {
        // Deep purple for aged negative phase
        vec3 lo  = vec3(0.03, 0.01, 0.04);
        vec3 mid = vec3(0.12, 0.05, 0.18);
        vec3 hi  = vec3(0.28, 0.12, 0.40);
        vec3 pk  = vec3(0.50, 0.25, 0.60);
        if (t < 0.25) return mix(lo, mid, t / 0.25);
        if (t < 0.6)  return mix(mid, hi, (t - 0.25) / 0.35);
        return mix(hi, pk, (t - 0.6) / 0.4);
      }
      
      vec3 cathedralPos(float t, float age) {
        vec3 young = posYoung(t);
        vec3 mid   = posMid(t);
        vec3 old   = posOld(t);
        if (age < 0.5) return mix(young, mid, age * 2.0);
        return mix(mid, old, (age - 0.5) * 2.0);
      }
      
      vec3 cathedralNeg(float t, float age) {
        vec3 young = negYoung(t);
        vec3 mid   = negMid(t);
        vec3 old   = negOld(t);
        if (age < 0.5) return mix(young, mid, age * 2.0);
        return mix(mid, old, (age - 0.5) * 2.0);
      }
      
      void main() {
        float amp = texture(u_curr, v_uv).r;
        
        // Signed rendering: positive = warm gold, negative = cool cyan
        // tanh for natural saturation, low gain to keep fringes thin
        float vis = tanh(amp * 5.0); // maps to [-1, 1], gentle
        
        // Inside lens/medium: invert phase colors
        float spdHere = texture(u_speed, v_uv).r;
        if (spdHere > 0.01 && spdHere < 0.95) vis = -vis;
        
        float posIntensity = max(vis, 0.0);
        float negIntensity = max(-vis, 0.0);
        
        // Age: 0 at source, 1 at detector — drives color shift
        float age = clamp((v_uv.x - u_sourceX) / (u_detectorX - u_sourceX), 0.0, 1.0);
        
        vec3 col = cathedralPos(posIntensity * posIntensity, age)
                 + cathedralNeg(negIntensity * negIntensity, age);
        
        // Optics outlines: detect edges where speed map changes sharply
        vec2 px = vec2(1.0) / u_res;
        float spd = texture(u_speed, v_uv).r;
        float spdL = texture(u_speed, v_uv - vec2(px.x, 0.0)).r;
        float spdR = texture(u_speed, v_uv + vec2(px.x, 0.0)).r;
        float spdU = texture(u_speed, v_uv + vec2(0.0, px.y)).r;
        float spdD = texture(u_speed, v_uv - vec2(0.0, px.y)).r;
        
        // Gradient magnitude of speed map
        float gx = abs(spdR - spdL);
        float gy = abs(spdU - spdD);
        float edgeStrength = sqrt(gx * gx + gy * gy);
        
        // Bone outline where speed changes (wall edges, lens boundaries)
        float outline = smoothstep(0.02, 0.15, edgeStrength);
        col += vec3(0.65, 0.60, 0.50) * outline * 0.8;

        // Solid fill for walls so they read as physical barriers, not just outlines
        float wallMask = 1.0 - step(0.01, spd);
        col = mix(col, vec3(0.36, 0.33, 0.26), wallMask);
        
        // --- Detector bar: accumulated pattern + real-time spikes ---
        float detDist = abs(v_uv.x - u_detectorX);
        vec4 detSample = texture(u_curr, vec2(u_detectorX, v_uv.y));
        float detAccum = detSample.g;
        float detInstant = detSample.b;
        
        float detBarMask = smoothstep(0.008, 0.001, detDist);
        
        // Accumulated pattern: warm amber, builds slowly
        float accumVis = detAccum * 1.5;
        accumVis = accumVis / (accumVis + 1.0);
        vec3 accumColor = vec3(0.60, 0.50, 0.30) * accumVis;
        
        // Instantaneous spike: bright flash, white-gold
        float instantVis = tanh(detInstant * 3.0);
        vec3 spikeColor = vec3(1.0, 0.92, 0.70) * instantVis;
        
        // Combine: accumulated base + bright spike on top
        vec3 detColor = accumColor + spikeColor * 0.7;
        col = mix(col, detColor, detBarMask);
        
        // Thin guide line
        float detLine = smoothstep(0.002, 0.0005, detDist) * 0.03;
        col += vec3(0.25, 0.22, 0.16) * detLine;
        
        // Saturation boost
        col = saturate(col, u_saturation);
        
        // Vignette — left/top/bottom only. Right side open to detector.
        float vigL = smoothstep(0.0, 0.10, v_uv.x);
        float vigT = smoothstep(0.0, 0.08, v_uv.y);
        float vigB = smoothstep(0.0, 0.08, 1.0 - v_uv.y);
        // Right: only fade past detector
        float vigR = v_uv.x > u_detectorX ? smoothstep(0.0, 0.04, 1.0 - v_uv.x) : 1.0;
        col *= vigL * vigR * vigT * vigB;
        
        o_color = vec4(col, 1.0);
      }
    `;
    return this._createProgram(this._vertSrc, frag);
  }

  _createBloomExtractProgram() {
    const frag = `#version 300 es
      precision highp float;
      uniform sampler2D u_tex;
      uniform float u_threshold;
      in vec2 v_uv;
      out vec4 o_color;
      void main() {
        vec3 col = texture(u_tex, v_uv).rgb;
        float brightness = dot(col, vec3(0.299, 0.587, 0.114));
        float contribution = smoothstep(u_threshold, u_threshold + 0.2, brightness);
        o_color = vec4(col * contribution, 1.0);
      }
    `;
    return this._createProgram(this._vertSrc, frag);
  }

  _createBloomBlurProgram() {
    const frag = `#version 300 es
      precision highp float;
      uniform sampler2D u_tex;
      uniform vec2 u_dir;
      uniform vec2 u_res;
      in vec2 v_uv;
      out vec4 o_color;
      void main() {
        vec2 px = u_dir / u_res;
        vec3 col = vec3(0.0);
        col += texture(u_tex, v_uv - 4.0 * px).rgb * 0.0162;
        col += texture(u_tex, v_uv - 3.0 * px).rgb * 0.0540;
        col += texture(u_tex, v_uv - 2.0 * px).rgb * 0.1216;
        col += texture(u_tex, v_uv - 1.0 * px).rgb * 0.1945;
        col += texture(u_tex, v_uv            ).rgb * 0.2270;
        col += texture(u_tex, v_uv + 1.0 * px).rgb * 0.1945;
        col += texture(u_tex, v_uv + 2.0 * px).rgb * 0.1216;
        col += texture(u_tex, v_uv + 3.0 * px).rgb * 0.0540;
        col += texture(u_tex, v_uv + 4.0 * px).rgb * 0.0162;
        o_color = vec4(col, 1.0);
      }
    `;
    return this._createProgram(this._vertSrc, frag);
  }

  _createCompositeProgram() {
    const frag = `#version 300 es
      precision highp float;
      uniform sampler2D u_scene;
      uniform sampler2D u_bloom;
      uniform float u_bloomIntensity;
      in vec2 v_uv;
      out vec4 o_color;
      void main() {
        vec3 scene = texture(u_scene, v_uv).rgb;
        vec3 bloom = texture(u_bloom, v_uv).rgb;
        vec3 col = scene + bloom * u_bloomIntensity;
        // Filmic tone mapping
        col = col / (col + 0.6);
        // Very gentle black crush (preserve diffraction)
        float lum = dot(col, vec3(0.299, 0.587, 0.114));
        col *= smoothstep(0.0, 0.008, lum);
        o_color = vec4(col, 1.0);
      }
    `;
    return this._createProgram(this._vertSrc, frag);
  }

  _makeTexture(w, h) {
    const gl = this.gl;
    const tex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, tex);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA32F, w, h, 0, gl.RGBA, gl.FLOAT, null);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    return tex;
  }

  _makeRGBA8Texture(w, h) {
    const gl = this.gl;
    const tex = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, tex);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA8, w, h, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    return tex;
  }

  _createBuffers() {
    const gl = this.gl;
    const w = this.width;
    const h = this.height;
    
    // Two amplitude textures for ping-pong (current + previous)
    this.texA = this._makeTexture(w, h);
    this.texB = this._makeTexture(w, h);
    this.texC = this._makeTexture(w, h); // output target
    
    // Speed map
    this.texSpeed = this._makeTexture(w, h);
    this._initSpeedTexture();
    
    // Framebuffers for wave sim
    this.waveFB_A = gl.createFramebuffer();
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.waveFB_A);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, this.texA, 0);
    
    this.waveFB_B = gl.createFramebuffer();
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.waveFB_B);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, this.texB, 0);
    
    this.waveFB_C = gl.createFramebuffer();
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.waveFB_C);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, this.texC, 0);
    
    // Render target
    this.texScene = this._makeRGBA8Texture(w, h);
    this.sceneFB = gl.createFramebuffer();
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.sceneFB);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, this.texScene, 0);
    
    // Bloom (half res)
    const bw = Math.floor(w / 2);
    const bh = Math.floor(h / 2);
    this.bloomW = bw;
    this.bloomH = bh;
    this.texBloomA = this._makeRGBA8Texture(bw, bh);
    this.texBloomB = this._makeRGBA8Texture(bw, bh);
    this.bloomFB_A = gl.createFramebuffer();
    this.bloomFB_B = gl.createFramebuffer();
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.bloomFB_A);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, this.texBloomA, 0);
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.bloomFB_B);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, this.texBloomB, 0);
    
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    
    // State: texA = current, texB = previous
    // After each step: output goes to texC, then rotate: prev=curr, curr=output
    this.currTex = this.texA;
    this.prevTex = this.texB;
    this.outTex = this.texC;
    this.currFB = this.waveFB_A;
    this.prevFB = this.waveFB_B;
    this.outFB = this.waveFB_C;
  }

  _initSpeedTexture() {
    const gl = this.gl;
    const w = this.width;
    const h = this.height;
    const data = new Float32Array(w * h * 4);
    for (let i = 0; i < w * h; i++) {
      data[i * 4] = 1.0;
      data[i * 4 + 1] = 0;
      data[i * 4 + 2] = 0;
      data[i * 4 + 3] = 1.0;
    }
    gl.bindTexture(gl.TEXTURE_2D, this.texSpeed);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA32F, w, h, 0, gl.RGBA, gl.FLOAT, data);
  }

  updateSpeedMap(degreeMap) {
    const gl = this.gl;
    const w = this.width;
    const h = this.height;
    const data = new Float32Array(w * h * 4);
    for (let i = 0; i < w * h; i++) {
      const spd = degreeMap ? (degreeMap[i] !== undefined ? degreeMap[i] : 1.0) : 1.0;
      data[i * 4] = spd;
      data[i * 4 + 1] = 0;
      data[i * 4 + 2] = 0;
      data[i * 4 + 3] = 1.0;
    }
    gl.bindTexture(gl.TEXTURE_2D, this.texSpeed);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA32F, w, h, 0, gl.RGBA, gl.FLOAT, data);
  }

  _clearSim() {
    const gl = this.gl;
    const w = this.width;
    const h = this.height;
    const zeros = new Float32Array(w * h * 4);
    
    gl.bindTexture(gl.TEXTURE_2D, this.texA);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA32F, w, h, 0, gl.RGBA, gl.FLOAT, zeros);
    gl.bindTexture(gl.TEXTURE_2D, this.texB);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA32F, w, h, 0, gl.RGBA, gl.FLOAT, zeros);
    gl.bindTexture(gl.TEXTURE_2D, this.texC);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA32F, w, h, 0, gl.RGBA, gl.FLOAT, zeros);
  }

  reset() {
    this.time = 0;
    this.simTime = 0;
    this._clearSim();
    this._initSpeedTexture();
  }

  step(dt) {
    this.time += dt;
    // Fixed simulation timestep — decoupled from wall clock
    // This ensures wavelength is consistent regardless of framerate
    const simDt = 1.0 / 60.0; // normalize to 60fps equivalent
    
    if (this._resize()) {
      this._destroyBuffers();
      this._createBuffers();
      this._clearSim();
      // Speed texture was reset to all-1s — notify host to rebuild optics.
      if (this.onResize) this.onResize();
    }
    
    const gl = this.gl;
    gl.bindVertexArray(this.vao);
    
    const sourceMode = this.sourceType === 'point' ? 1 : 0;
    const continuous = this.continuousSource ? 1 : 0;
    
    // Multiple substeps for faster wave propagation
    const substeps = 12;
    for (let sub = 0; sub < substeps; sub++) {
      this.simTime += simDt / substeps;
      const subTime = this.simTime;
      
      // Wave update: read from curr + prev, write to out
      gl.useProgram(this.waveProgram);
      gl.bindFramebuffer(gl.FRAMEBUFFER, this.outFB);
      gl.viewport(0, 0, this.width, this.height);
      
      gl.activeTexture(gl.TEXTURE0);
      gl.bindTexture(gl.TEXTURE_2D, this.currTex);
      gl.activeTexture(gl.TEXTURE1);
      gl.bindTexture(gl.TEXTURE_2D, this.prevTex);
      gl.activeTexture(gl.TEXTURE2);
      gl.bindTexture(gl.TEXTURE_2D, this.texSpeed);
      
      gl.uniform2f(gl.getUniformLocation(this.waveProgram, 'u_res'), this.width, this.height);
      gl.uniform1f(gl.getUniformLocation(this.waveProgram, 'u_speedMult'), this.speedMultiplier);
      // Frequency (modulated by knob panel LFOs via callbacks)
      let modFreq = this.sourceFreq;
      
      // Envelope: smooth release when source is off
      if (this.continuousSource && !this._releasing) {
        this._envelopeLevel += (1.0 - this._envelopeLevel) * 0.1;
      } else {
        this._releasing = !this.continuousSource;
        const releaseRate = simDt / Math.max(0.05, this.envelopeRelease);
        this._envelopeLevel = Math.max(0, this._envelopeLevel - releaseRate);
      }
      
      gl.uniform1f(gl.getUniformLocation(this.waveProgram, 'u_time'), subTime);
      gl.uniform1f(gl.getUniformLocation(this.waveProgram, 'u_sourceFreq'), modFreq);
      gl.uniform1f(gl.getUniformLocation(this.waveProgram, 'u_sourceX'), this.sourceX);
      gl.uniform1i(gl.getUniformLocation(this.waveProgram, 'u_sourceMode'), sourceMode);
      gl.uniform1i(gl.getUniformLocation(this.waveProgram, 'u_continuous'), continuous ? 1 : 0);
      gl.uniform1f(gl.getUniformLocation(this.waveProgram, 'u_sourceAmp'), this.sourceAmplitude * this._envelopeLevel);
      gl.uniform1i(gl.getUniformLocation(this.waveProgram, 'u_curr'), 0);
      gl.uniform1i(gl.getUniformLocation(this.waveProgram, 'u_prev'), 1);
      gl.uniform1i(gl.getUniformLocation(this.waveProgram, 'u_speed'), 2);
      gl.uniform1f(gl.getUniformLocation(this.waveProgram, 'u_detectorX'), this.detectorX);
      gl.uniform1f(gl.getUniformLocation(this.waveProgram, 'u_detectorSens'), this.detectorSensitivity);
      
      gl.drawArrays(gl.TRIANGLES, 0, 6);
      
      // Rotate: prev = curr, curr = out, out = prev
      const tmpTex = this.prevTex;
      const tmpFB = this.prevFB;
      this.prevTex = this.currTex;
      this.prevFB = this.currFB;
      this.currTex = this.outTex;
      this.currFB = this.outFB;
      this.outTex = tmpTex;
      this.outFB = tmpFB;
    }
    
    // --- RENDER: color grade current amplitude to scene ---
    gl.useProgram(this.renderProgram);
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.sceneFB);
    gl.viewport(0, 0, this.width, this.height);
    
    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_2D, this.currTex);
    gl.activeTexture(gl.TEXTURE1);
    gl.bindTexture(gl.TEXTURE_2D, this.texSpeed);
    
    gl.uniform2f(gl.getUniformLocation(this.renderProgram, 'u_res'), this.width, this.height);
    gl.uniform1i(gl.getUniformLocation(this.renderProgram, 'u_curr'), 0);
    gl.uniform1i(gl.getUniformLocation(this.renderProgram, 'u_speed'), 1);
    gl.uniform1f(gl.getUniformLocation(this.renderProgram, 'u_detectorX'), this.detectorX);
    gl.uniform1f(gl.getUniformLocation(this.renderProgram, 'u_sourceX'), this.sourceX);
    gl.uniform1f(gl.getUniformLocation(this.renderProgram, 'u_saturation'), this.saturation);
    
    gl.drawArrays(gl.TRIANGLES, 0, 6);
    
    // --- BLOOM ---
    gl.useProgram(this.bloomExtractProgram);
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.bloomFB_A);
    gl.viewport(0, 0, this.bloomW, this.bloomH);
    
    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_2D, this.texScene);
    gl.uniform1i(gl.getUniformLocation(this.bloomExtractProgram, 'u_tex'), 0);
    gl.uniform1f(gl.getUniformLocation(this.bloomExtractProgram, 'u_threshold'), 0.45);
    gl.drawArrays(gl.TRIANGLES, 0, 6);
    
    // H blur
    gl.useProgram(this.bloomBlurProgram);
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.bloomFB_B);
    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_2D, this.texBloomA);
    gl.uniform1i(gl.getUniformLocation(this.bloomBlurProgram, 'u_tex'), 0);
    gl.uniform2f(gl.getUniformLocation(this.bloomBlurProgram, 'u_dir'), 2.0, 0.0);
    gl.uniform2f(gl.getUniformLocation(this.bloomBlurProgram, 'u_res'), this.bloomW, this.bloomH);
    gl.drawArrays(gl.TRIANGLES, 0, 6);
    
    // V blur
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.bloomFB_A);
    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_2D, this.texBloomB);
    gl.uniform2f(gl.getUniformLocation(this.bloomBlurProgram, 'u_dir'), 0.0, 2.0);
    gl.drawArrays(gl.TRIANGLES, 0, 6);
    
    // Second pass wider bloom
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.bloomFB_B);
    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_2D, this.texBloomA);
    gl.uniform2f(gl.getUniformLocation(this.bloomBlurProgram, 'u_dir'), 4.0, 0.0);
    gl.drawArrays(gl.TRIANGLES, 0, 6);
    
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.bloomFB_A);
    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_2D, this.texBloomB);
    gl.uniform2f(gl.getUniformLocation(this.bloomBlurProgram, 'u_dir'), 0.0, 4.0);
    gl.drawArrays(gl.TRIANGLES, 0, 6);
    
    // --- COMPOSITE ---
    gl.useProgram(this.compositeProgram);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    gl.viewport(0, 0, this.width, this.height);
    
    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_2D, this.texScene);
    gl.activeTexture(gl.TEXTURE1);
    gl.bindTexture(gl.TEXTURE_2D, this.texBloomA);
    
    gl.uniform1i(gl.getUniformLocation(this.compositeProgram, 'u_scene'), 0);
    gl.uniform1i(gl.getUniformLocation(this.compositeProgram, 'u_bloom'), 1);
    gl.uniform1f(gl.getUniformLocation(this.compositeProgram, 'u_bloomIntensity'), 0.25);
    
    gl.drawArrays(gl.TRIANGLES, 0, 6);
  }

  // Read detector column: returns { accum, instant } arrays
  readDetectorColumn() {
    return this.readDetectorColumnAt(this.detectorX);
  }

  // Read detector at any UV x-position
  readDetectorColumnAt(uvX) {
    const gl = this.gl;
    const x = Math.floor(uvX * this.width);
    const h = this.height;
    const pixels = new Float32Array(4 * h);
    gl.bindFramebuffer(gl.FRAMEBUFFER, this.currFB);
    gl.readPixels(x, 0, 1, h, gl.RGBA, gl.FLOAT, pixels);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    const accum = new Float32Array(h);
    const instant = new Float32Array(h);
    for (let i = 0; i < h; i++) {
      accum[i] = pixels[i * 4 + 1];   // green: accumulated
      instant[i] = pixels[i * 4 + 2]; // blue: instantaneous
    }
    return { accum, instant };
  }

  _destroyBuffers() {
    const gl = this.gl;
    gl.deleteTexture(this.texA);
    gl.deleteTexture(this.texB);
    gl.deleteTexture(this.texC);
    gl.deleteTexture(this.texSpeed);
    gl.deleteTexture(this.texScene);
    gl.deleteTexture(this.texBloomA);
    gl.deleteTexture(this.texBloomB);
    gl.deleteFramebuffer(this.waveFB_A);
    gl.deleteFramebuffer(this.waveFB_B);
    gl.deleteFramebuffer(this.waveFB_C);
    gl.deleteFramebuffer(this.sceneFB);
    gl.deleteFramebuffer(this.bloomFB_A);
    gl.deleteFramebuffer(this.bloomFB_B);
  }
}
