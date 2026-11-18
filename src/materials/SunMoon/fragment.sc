$input v_texcoord0, v_sunVis

#include <bgfx_shader.sh>

#ifndef INSTANCING
  #include <newb/config.h>
  #include <newb/functions/tonemap.h>

  uniform vec4 SunMoonColor;

  SAMPLER2D_AUTOREG(s_SunMoonTexture);
#endif

void main() {
  #ifndef INSTANCING
    vec4 tex = texture2D(s_SunMoonTexture, v_texcoord0);
    bool isSun = v_sunVis < 1.5;
    float sunVis = min(v_sunVis, 1.0);

    vec4 color = tex;
    color.rgb *= SunMoonColor.rgb;
    if (isSun) {
      // The sun texture has a baked glow around its core. SunMoonColor tints it (cool at day, warm at dawn), which
      // shows as a colored outline around the sun. Turn the inner part of that glow white (keeping only a little of
      // the tint), the outer glow keeps the full tint. Same color math as below, so the brightness stays the same.
      float peak = max(SunMoonColor.r, max(SunMoonColor.g, SunMoonColor.b));
      vec3 white = tex.rgb*peak*mix(vec3(1.0, 1.0, 1.0), SunMoonColor.rgb/max(peak, 0.001), 0.15);
      color.rgb = mix(color.rgb, white, smoothstep(0.04, 0.20, tex.r)*clamp(NL_SUN_HALO_WHITE, 0.0, 1.0));
    }
    color.rgb *= 4.4*color.rgb;
    float tr = 1.0 - SunMoonColor.a;
    color.a *= 1.0 - tr*tr;
    color.a *= sunVis; // sun hidden below the horizon, underwater and in rain (1.0 for the moon)
    color.rgb = colorCorrection(color.rgb);
    gl_FragColor = color;
  #else
    gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
  #endif
}
