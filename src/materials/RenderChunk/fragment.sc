$input v_color0, v_color1, v_fog, v_refl, v_texcoord0, v_lightmapUV, v_extra, v_position, v_wpos, v_cpos

#include <bgfx_shader.sh>
#include <newb/main.sh>

SAMPLER2D_AUTOREG(s_MatTexture);
SAMPLER2D_AUTOREG(s_SeasonsTexture);
SAMPLER2D_AUTOREG(s_LightMapTexture);
SAMPLER2D_AUTOREG(s_caustic_noise);

uniform vec4 TimeOfDay;
uniform vec4 FogColor;
uniform vec4 FogAndDistanceControl;
uniform vec4 ViewPositionAndTime;
uniform vec4 CameraPosition;

void main() {
  #if defined(DEPTH_ONLY_OPAQUE) || defined(DEPTH_ONLY) || defined(INSTANCING)
    gl_FragColor = vec4(1.0,1.0,1.0,1.0);
    return;
  #endif

  vec4 diffuse = texture2D(s_MatTexture, v_texcoord0);
  vec4 color = v_color0;

  #ifdef ALPHA_TEST
    if (diffuse.a < 0.6) {
      discard;
    }
  #endif

  nl_environment env = nlDetectEnvironment(TimeOfDay.x,FogColor.rgb,FogAndDistanceControl.xyz);
  
  #if defined(SEASONS) && (defined(OPAQUE) || defined(ALPHA_TEST))
    diffuse.rgb *= mix(vec3(1.0,1.0,1.0), texture2D(s_SeasonsTexture, v_color1.xy).rgb * 2.0, v_color1.z);
  #endif

  vec3 glow = nlGlow(s_MatTexture, v_texcoord0, v_extra.a);

  diffuse.rgb *= diffuse.rgb;

  #if defined(TRANSPARENT) && !(defined(SEASONS) || defined(RENDER_AS_BILLBOARDS))
    if (v_extra.b > 0.9) {
      diffuse.rgb = vec3_splat(1.0 - NL_WATER_TEX_OPACITY*(1.0 - diffuse.b*1.8));
      diffuse.a = color.a;
    }
  #else
    diffuse.a = 1.0;
  #endif

  diffuse.rgb *= color.rgb;

  #if !defined(ALPHA_TEST) && !defined(SEASONS) && !defined(TRANSPARENT)
    // moving light pattern on the sea floor, only below the water surface of the sea
    {
      vec3 absPos = v_wpos + CameraPosition.xyz;
      if (absPos.y < NL_CAUSTIC_SURFACE_Y - 0.5) {
        vec3 viewDir = normalize(v_wpos);
        vec3 geoNormal = normalize(cross(dFdx(v_position), dFdy(v_position)));
        if (dot(geoNormal, viewDir) > 0.0) {
          geoNormal = -geoNormal;
        }
        diffuse.rgb = nlCaustics(s_caustic_noise, diffuse.rgb, absPos.xz, absPos.y, length(v_wpos), geoNormal, v_lightmapUV.y, env, ViewPositionAndTime.w);
      }
    }
  #endif

  float rainShadowMask = mix(1.0, 0.25, env.rainFactor);
  float nightShadowMask = mix(1.0, 0.25, 1.0 - smoothstep(-0.12, 0.25, env.dayFactor)); // eased night mask (same curve as the sky)
  // side shadow only exists outdoors, where sun/moon light reaches (skylight); none indoors or in caves
  float outdoorMask = smoothstep(0.86, 0.93, v_lightmapUV.y); // full only at skylight level 15 (15/16); 14 and below = indoors/leaking light
  float shadow = smoothstep(0.875, 0.860, v_lightmapUV.y*v_lightmapUV.y);
  float torchEdge = v_lightmapUV.x*v_lightmapUV.x*1.44; // (x*1.2)^2
  shadow = mix(shadow, 0.0, torchEdge*torchEdge*torchEdge); // ^6 without pow()
  diffuse.rgb *= 1.0 - 0.72 * shadow * rainShadowMask * nightShadowMask;

  if (v_extra.b <= 0.9) {
    float sideshadow = smoothstep(0.64, 0.62, v_color1.g);
    diffuse.rgb *= 1.0 - 0.65 * sideshadow * outdoorMask * rainShadowMask * nightShadowMask;
  }

  diffuse.rgb += glow;

  //Water Foam Detection
  float wcPos = v_wpos.y + CameraPosition.y;
  float dy_w = abs(dFdy(v_wpos.y));

  bool isOneBlockUnderwater = ((wcPos <= 62.89 && wcPos > 62.0) || (wcPos <= -60.11 && wcPos > -61.0)) 
    && v_extra.b < 0.9 
    && !(dy_w < 0.0002) 
    && step(0.85, v_lightmapUV.y) * step(v_lightmapUV.y, 0.927) == 1.0; 

  if (isOneBlockUnderwater) {
    float d = 0.865;
    float bpy = fract(v_cpos.y);
    vec3 viewdir = normalize(-v_wpos);

    // Proyección de rayos 3D
    vec2 offset = (d - bpy) * viewdir.xz / viewdir.y;
    vec2 watpos = v_cpos.xz + offset;

    float fade = clamp((d - bpy) / abs(viewdir.y), 0.0, 1.0);
    fade = 1.0 - fade;

    float t = 0.3 * ViewPositionAndTime.w;
    vec2 st = watpos;
    float n = sin(st.x + 3.0*sin(0.5*st.y) + t) * sin(st.y + 3.0*sin(0.5*st.x) - t);
    n = 0.5 + 0.5 * sin(st.x + 2.0*n + 0.2*t) * sin(st.y - 4.0*n + 0.1*t);
    fade = pow(fade, 1.0 + 2.5 * n);

    // Textura procedural de espuma
    float foam = fract(238.084 * sin(dot(floor(16.0 * watpos), vec2(1.32, 141.3))));
    foam *= fade;
    foam = mix(foam, 1.0, fade * fade);

    // foam dims at night (same curve as nightShadowMask); full strength during the day
    float nightFoam = mix(0.03, 1.0, smoothstep(-0.12, 0.25, env.dayFactor));
    diffuse.rgb = mix(diffuse.rgb, vec3(1.0, 1.0, 1.1)*3.0*nightFoam, foam);
  }

  if (v_extra.b > 0.9) {
    diffuse.rgb += v_refl.rgb*v_refl.a;
  } else if (v_refl.a > 0.0) {
    // reflective effect - only on xz plane
    float dy = abs(dFdy(v_extra.g));
    if (dy < 0.0002) {
      float mask = v_refl.a*(clamp(v_extra.r*10.0,8.2,8.8)-7.8);
      #ifdef NL_GROUND_RAIN_PUDDLES
        // the whole surface is wet: a stronger mirror and a darker floor, the same everywhere (no per-pixel noise)
        mask *= 1.0 + NL_GROUND_RAIN_PUDDLES;
        diffuse.rgb *= 1.0 - 0.3*env.rainFactor*NL_GROUND_RAIN_PUDDLES;
      #endif
      mask = min(mask, 1.0);
      diffuse.rgb *= 1.0 - 0.6*mask;
      diffuse.rgb += v_refl.rgb*mask;
    }
  }

  diffuse.rgb = mix(diffuse.rgb, v_fog.rgb, v_fog.a);

  diffuse.rgb = colorCorrection(diffuse.rgb);

  gl_FragColor = diffuse;
}
