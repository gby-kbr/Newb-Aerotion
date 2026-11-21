#ifndef SKYCLOUDS_H
#define SKYCLOUDS_H

// Cloud type 4 (bicubic): drawn by the Clouds material (it replaces the vanilla cloud mesh).
// Only included by Clouds/fragment.sc, so the noise sampler is declared in the Clouds material only.

#if NL_CLOUD_TYPE == 4

  float bsbeHash(float h) {
    return fract(sin(h) * 43758.5453);
  }

  #ifdef NL_CLOUD4_NOISE_TEX
    // buffer: src/materials/Clouds/buffers/CloudsTexture.json (sampler name = s_ + buffer name)
    SAMPLER2D_AUTOREG(s_CloudsTexture);

    float bsbeVoronoi(vec2 pos) {
      return texture2DLod(s_CloudsTexture, pos * (1.0 / NL_CLOUD4_NOISE_CELLS), 0.0).r;
    }
  #else
    float bsbeVoronoi(vec2 pos) {
      vec2 p = floor(pos);
      vec2 f = fract(pos);
      float dist = 1.0;
      for (float y = -1.0; y <= 1.0; y++) {
        for (float x = -1.0; x <= 1.0; x++) {
          vec2 ne = vec2(x, y);
          vec2 pn = p + ne;
          float n = pn.x + pn.y * 57.0;
          dist = min(dist, length(ne + bsbeHash(n) - f));
        }
      }
      return dist;
    }
  #endif

  float bsbeFbm(vec2 pos, float pDens, float fTime, int nOct) {
    float sum = 0.0;
    float sDens = 1.0;
    pos += fTime * 0.001 * NL_CLOUD4_SPEED;
    for (int i = 0; i < 4; i++) {
      if (i < nOct) sum += bsbeVoronoi(pos) * sDens * pDens;
      else          sum += 0.42 * sDens * pDens;
      sDens *= 0.5;
      pos *= 2.5;
      pos += fTime * 0.05 * NL_CLOUD4_SPEED;
    }
    return saturate(1.0 - sum);
  }

  vec3 bsbeSatur(vec3 c, float sat) {
    float lum = dot(c, vec3(0.2125, 0.7154, 0.0721));
    return mix(vec3_splat(lum), c, sat);
  }

  // Clouds in the world: a stack of layers at fixed heights (the top layer is at the cloud height of the game,
  // cloudH = its height relative to the camera). Each layer is hit by the view ray at its own height and the noise is
  // read at the world position of the hit, so the clouds stay where they are, can be flown over and entered.
  // returns the clouds as a color with alpha (alpha blended over the sky by the Clouds material)
  vec4 bsbeClouds(vec3 nDir, float cloudH, vec2 camXZ, float fTime, vec3 horC, vec3 zenC, vec2 fragCoord) {
    float ay = abs(nDir.y);
    if (ay < 0.001) {
      return vec4(0.0, 0.0, 0.0, 0.0);
    }

    vec2 ray = nDir.xz / nDir.y;
    ray -= ray * bsbeHash(fragCoord.x + fragCoord.y * 57.0) * 0.05; // dither (only the view part, not the world position)

    // 80 blocks = the cloud height the pattern size was made for
    float worldScale = NL_CLOUD4_SCALE / 80.0;
    float layerGap = NL_CLOUD4_LAYER_SQUEEZE * 80.0; // blocks between two layers
    vec3 cloAmbC = bsbeSatur(zenC, 0.7);

    // layers are mixed over nothing: col = premultiplied color, trans = how much of the sky still shows through
    // (sky*trans + col is the same as mixing every layer over the sky color)
    vec3 col = vec3(0.0, 0.0, 0.0);
    float trans = 1.0;

    float lodY = ay / NL_CLOUD4_LOD_Y;
    int nOct = int(step(1.0, lodY) + step(1.58, lodY) + step(2.5, lodY) + step(3.95, lodY));
    if (nOct > 0) {
      float horizonFade = smoothstep(0.0, NL_CLOUD4_HORIZON_FADE, ay);
      for (int k = 0; k < NL_CLOUD4_LAYERS; k++) {
        // far to near: looking up the highest layer is the farthest, looking down the lowest one is
        int i = nDir.y > 0.0 ? k : NL_CLOUD4_LAYERS - 1 - k;
        float j = float(i);
        float layerH = cloudH - j*layerGap;
        if (layerH*nDir.y > 0.0) { // the layer is in front of the camera (not behind it)
          float sDens = NL_CLOUD4_DENSITY - 0.1*(min(j, 7.0) - max(j - 7.0, 0.0));
          vec3 cloDirC = mix(horC*NL_CLOUD4_BRIGHTNESS, cloAmbC, 1.0 - pow(1.0 - NL_CLOUD4_AMBIENT_MIX, j + 1.0));
          float cloudM = bsbeFbm((ray*layerH + camXZ)*worldScale, sDens, fTime, nOct);
          float a = cloudM * horizonFade;
          col = mix(col, cloDirC, a);
          trans *= 1.0 - a;
        }
      }
    }
    float alpha = 1.0 - trans;
    return vec4(col / max(alpha, 0.0001), alpha);
  }

#endif

#endif
