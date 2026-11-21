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

  // Clouds anchored in the world: the view ray hits a cloud plane at a fixed height (cloudH = height of the plane
  // relative to the camera, negative when the camera is above it) and the noise is read at the world position of
  // that hit, so the clouds stay put when the player moves, and the player can fly above or below them.
  // returns the clouds as a color with alpha (alpha blended over the sky by the Clouds material)
  vec4 bsbeClouds(vec3 nDir, float cloudH, vec2 camXZ, float fTime, vec3 horC, vec3 zenC, vec2 fragCoord) {
    float ay = abs(nDir.y);
    if (nDir.y*cloudH <= 0.0 || ay < 0.001) {
      return vec4(0.0, 0.0, 0.0, 0.0); // looking away from the cloud plane
    }

    vec2 ray = nDir.xz / nDir.y;
    ray -= ray * bsbeHash(fragCoord.x + fragCoord.y * 57.0) * 0.05; // dither (only the view part, not the world position)
    float planeH = cloudH;

    float sDens = NL_CLOUD4_DENSITY;
    vec3 cloDirC = horC * NL_CLOUD4_BRIGHTNESS;
    vec3 cloAmbC = bsbeSatur(zenC, 0.7);

    // layers are mixed over nothing: col = premultiplied color, trans = how much of the sky still shows through
    // (sky*trans + col is the same as mixing every layer over the sky color)
    vec3 col = vec3(0.0, 0.0, 0.0);
    float trans = 1.0;

    float lodY = ay / NL_CLOUD4_LOD_Y;
    int nOct = int(step(1.0, lodY) + step(1.58, lodY) + step(2.5, lodY) + step(3.95, lodY));
    if (nOct > 0) {
      float horizonFade = smoothstep(0.0, NL_CLOUD4_HORIZON_FADE, ay);
      for (int i = 0; i < NL_CLOUD4_LAYERS; i++) {
        vec2 cloudP = (ray*planeH + camXZ) * (NL_CLOUD4_SCALE / NL_CLOUD4_WORLD_SIZE);
        float cloudM = bsbeFbm(cloudP, sDens, fTime, nOct);
        cloDirC = mix(cloDirC, cloAmbC, NL_CLOUD4_AMBIENT_MIX);
        float a = cloudM * horizonFade;
        col = mix(col, cloDirC, a);
        trans *= 1.0 - a;
        sDens += (i <= 6) ? -0.1 : 0.1;
        planeH *= 1.0 - NL_CLOUD4_LAYER_SQUEEZE; // next layer is a little lower (volume)
      }
    }
    // fade out while the camera is inside the cloud layer
    float alpha = (1.0 - trans) * smoothstep(1.0, 10.0, abs(cloudH));
    return vec4(col / max(1.0 - trans, 0.0001), alpha);
  }

#endif

#endif
