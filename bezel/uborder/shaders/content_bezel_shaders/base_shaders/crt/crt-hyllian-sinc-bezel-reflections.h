
layout(push_constant) uniform Push
{
    float BEAM_MIN_WIDTH;
    float BEAM_MAX_WIDTH;
    float SCANLINES_STRENGTH;
    float BRIGHTBOOST;
    float SHARPNESS_HACK;
    float PHOSPHOR_LAYOUT;
    float MASK_STRENGTH;
    float MONITOR_SUBPIXELS;
    float InputGamma;
    float OutputGamma;
    float MaskGamma;
    float VSCANLINES;
    float CRT_ANTI_RINGING;
    float VIG_TOGGLE;
    float VIG_BASE;
    float VIG_EXP;
} param;



#define USE_BEZEL_REFLECTIONS_COMMON

layout(std140, set = 0, binding = 0) uniform UBO
{
	mat4 MVP;
    vec4 SourceSize;
    vec4 OriginalSize;
    vec4 OutputSize;
    uint FrameCount;

#include "../../include/uborder_bezel_reflections_global_declarations.inc"


} global;

#include "../../include/uborder_bezel_reflections_params.inc"

#define ub_OutputSize     global.OutputSize
#define ub_OriginalSize   global.OriginalSize
#define ub_Rotation       global.Rotation

#include "../../include/uborder_bezel_reflections_common.inc"

#pragma parameter NO_NONONO          " "                              0.0  0.0 0.0 1.0
#pragma parameter CS_NONONO          "** CRT-HYLLIAN-SINC **"         0.0  0.0 0.0 1.0

#pragma parameter col_nonono         "COLOR SETTINGS:"                0.0  0.0 0.0 1.0
#pragma parameter InputGamma         "    Input Gamma"                2.4  0.0 4.0 0.1
#pragma parameter OutputGamma        "    Output Gamma"               2.2  0.0 3.0 0.1
#pragma parameter BRIGHTBOOST        "    BrightBoost"                1.0  0.5 1.5 0.01
#pragma parameter VIG_TOGGLE         "    Vignette Toggle"            1.0  0.0 1.0 1.0
#pragma parameter VIG_BASE           "        Vignette Range"        32.0  2.0 100.0 2.0
#pragma parameter VIG_EXP            "        Vignette Strength"      0.16  0.0 2.0 0.02

#pragma parameter scan_nonono        "SCANLINES SETTINGS:"           0.0  0.0 0.0 1.0
#pragma parameter BEAM_MIN_WIDTH     "    Min Beam Width"           0.80 0.0 1.0 0.01
#pragma parameter BEAM_MAX_WIDTH     "    Max Beam Width"           1.0  0.0 1.0 0.01
#pragma parameter SCANLINES_STRENGTH "    Scanlines Strength"       0.30 0.0 1.0 0.01
#pragma parameter VSCANLINES         "    Orientation [ HORIZONTAL, VERTICAL ]"    0.0 0.0 1.0 1.0

#pragma parameter msk_nonono           "MASK SETTINGS:"               0.0 0.0  0.0 1.0
#pragma parameter PHOSPHOR_LAYOUT      "    Mask [1-6 APERT, 7-10 DOT, 11-14 SLOT, 15-17 LOTTES]" 1.0 0.0 17.0 1.0
#pragma parameter MASK_STRENGTH        "    Mask Strength"            1.0 0.0 1.0 0.02
#pragma parameter MaskGamma            "    Mask Gamma"               2.4 1.0 5.0 0.1
#pragma parameter MONITOR_SUBPIXELS    "    Monitor Subpixels Layout [ RGB, BGR ]" 0.0 0.0 1.0 1.0

#pragma parameter fil_nonono        "FILTERING SETTINGS:"                            0.0 0.0 0.0 1.0
#pragma parameter SHARPNESS_HACK    "    Sharpness Hack"                             1.0 1.0 4.0 1.0
#pragma parameter CRT_ANTI_RINGING  "    Anti Ringing"                               1.0 0.0 1.0 1.0


#define CRT_ANTI_RINGING param.CRT_ANTI_RINGING

vec2  mask_size  = ub_OutputSize.xy* fr_scale * (1.0 - 0.5*global.h_curvature);

#pragma stage vertex
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec2 uv;
layout(location = 2) out vec2 border_uv;
layout(location = 3) out vec2 bezel_uv;

void main()
{
    gl_Position = global.MVP * Position;
    vTexCoord = TexCoord * vec2(1.000001);

    vec2 diff = vTexCoord.xy - middle;
    uv        = 2.0*(middle + diff / fr_scale - fr_center) - 1.0;
    bezel_uv  = uv - 2.0*bz_center;

    border_uv = (global.border_allow_rot < 0.5) ? get_unrotated_coords(TexCoord.xy, ub_Rotation) : TexCoord.xy;

    border_uv.y = mix(border_uv.y, 1.0-border_uv.y, border_mirror_y);

    border_uv = middle + (border_uv.xy - middle - border_pos) / (global.border_scale*all_zoom);

    border_uv = border_uv.xy * vec2(1.000001);

#ifdef KEEP_BORDER_ASPECT_RATIO
    border_uv -= 0.5.xx;
#endif
}

#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec2 border_uv;
layout(location = 3) in vec2 bezel_uv;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform sampler2D Source;
layout(set = 0, binding = 3) uniform sampler2D BORDER;
layout(set = 0, binding = 4) uniform sampler2D LAYER2;
#ifdef USE_AMBIENT_LIGHT
layout(set = 0, binding = 5) uniform sampler2D ambi_temporal_pass;
#endif

/*
    Hyllian's CRT Shader - Sinc/Spline16 version

    Copyright (C) 2011-2022 Hyllian - sergiogdb@gmail.com

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

//#include "../../../../include/subpixel_masks.h"

#define GAMMA_IN(color)     pow(color, vec3(param.InputGamma, param.InputGamma, param.InputGamma))
#define GAMMA_OUT(color)    pow(color, vec3(1.0 / param.OutputGamma, 1.0 / param.OutputGamma, 1.0 / param.OutputGamma))

//#define scanlines_strength (2.0*param.SCANLINES_STRENGTH)
#define scanlines_strength (1.0-0.5*param.SCANLINES_STRENGTH)
#define beam_min_width     param.BEAM_MIN_WIDTH
#define beam_max_width     param.BEAM_MAX_WIDTH
#define color_boost        param.BRIGHTBOOST

#define pi    3.1415926535897932384626433832795
#define wa    (0.5*pi)
#define wb    (pi)

// Shadow mask.
vec3 Mask(vec2 pos, float shadowMask)
{
    const float maskDark  = 0.0;
    const float maskLight = 1.0;

    vec3 mask = vec3(maskDark, maskDark, maskDark);
  
    // Very compressed TV style shadow mask.
    if (shadowMask == 15.0) 
    {
        float line = maskLight;
        float odd = 0.0;
        
        if (fract(pos.x*0.166666666) < 0.5) odd = 1.0;
        if (fract((pos.y + odd) * 0.5) < 0.5) line = maskDark;  
        
        pos.x = fract(pos.x*0.333333333);

        if      (pos.x < 0.333) mask.b = maskLight;
        else if (pos.x < 0.666) mask.g = maskLight;
        else                    mask.r = maskLight;
        mask*=line;  
    } 

    // Aperture-grille. This mask is the same as mask 2.
/*    else if (shadowMask == 16.0) 
    {
        pos.x = fract(pos.x*0.333333333);

        if      (pos.x < 0.333) mask.b = maskLight;
        else if (pos.x < 0.666) mask.g = maskLight;
        else                    mask.r = maskLight;
    } 
*/
    // Stretched VGA style shadow mask (same as prior shaders).
    else if (shadowMask == 16.0) 
    {
        pos.x += pos.y*3.0;
        pos.x  = fract(pos.x*0.166666666);

        if      (pos.x < 0.333) mask.b = maskLight;
        else if (pos.x < 0.666) mask.g = maskLight;
        else                    mask.r = maskLight;
    }

    // VGA style shadow mask.
    else if (shadowMask == 17.0) 
    {
        pos.xy  = floor(pos.xy*vec2(1.0, 0.5));
        pos.x  += pos.y*3.0;
        pos.x   = fract(pos.x*0.166666666);

        if      (pos.x < 0.333) mask.b = maskLight;
        else if (pos.x < 0.666) mask.g = maskLight;
        else                    mask.r = maskLight;
    }

    return mask;
}


/* Mask code pasted from subpixel_masks.h. Masks 3 and 4 added. */
vec3 mask_weights(vec2 coord, float phosphor_layout){
   
//   if (lottes_mask > 0.5) return Mask(coord);

   if (phosphor_layout > 14) return Mask(coord, phosphor_layout);

   vec3 weights = vec3(1.,1.,1.);

   const float on  = 1.;
   const float off = 0.;

   const vec3 red     = vec3(off, off, on );
   const vec3 green   = vec3(off, on,  off);
   const vec3 blue    = vec3(on,  off, off);
   const vec3 magenta = vec3(on,  off, on );
   const vec3 yellow  = vec3(off, on,  on );
   const vec3 cyan    = vec3(on,  on,  off);
   const vec3 black   = vec3(off, off, off);
   const vec3 white   = vec3(on,  on,  on );

   int w, z = 0;
   
   // This pattern is used by a few layouts, so we'll define it here
   vec3 aperture_classic = mix(magenta, green, floor(mod(coord.x, 2.0)));
   
   if(phosphor_layout == 0.) return weights;

   else if(phosphor_layout == 1.){
      // classic aperture for RGB panels; good for 1080p, too small for 4K+
      // aka aperture_1_2_bgr
      weights  = aperture_classic;
      return weights;
   }

   else if(phosphor_layout == 2.){
      // Classic RGB layout; good for 1080p and lower
      const vec3 aperture1[3] = vec3[](red, green, blue);
//      vec3 bw3[3] = vec3[](black, yellow, blue);
      
      z = int(floor(mod(coord.x, 3.0)));
      
      weights = aperture1[z];
      return weights;
   }

   else if(phosphor_layout == 3.){
      // black and white aperture; good for weird subpixel layouts and low brightness; good for 1080p and lower
      const vec3 aperture2[3] = vec3[](black, white, black);
      
      z = int(floor(mod(coord.x, 3.0)));
      
      weights = aperture2[z];
      return weights;
   }

   else if(phosphor_layout == 4.){
      // reduced TVL aperture for RGB panels. Good for 4k.
      // aperture_2_4_rgb
      
      const vec3 aperture3[4] = vec3[](red, yellow, cyan, blue);
      
      w = int(floor(mod(coord.x, 4.0)));
      
      weights = aperture3[w];
      return weights;
   }
   

   else if(phosphor_layout == 5.){
      // black and white aperture; good for weird subpixel layouts and low brightness; good for 4k 
      const vec3 aperture4[4] = vec3[](black, black, white, white);
      
      z = int(floor(mod(coord.x, 4.0)));
      
      weights = aperture4[z];
      return weights;
   }


   else if(phosphor_layout == 6.){
      // aperture_1_4_rgb; good for simulating lower 
      const vec3 aperture5[4] = vec3[](red, green, blue, black);
      
      z = int(floor(mod(coord.x, 4.0)));
      
      weights = aperture5[z];
      return weights;
   }

   else if(phosphor_layout == 7.){
      // 2x2 shadow mask for RGB panels; good for 1080p, too small for 4K+
      // aka delta_1_2x1_bgr
      vec3 inverse_aperture = mix(green, magenta, floor(mod(coord.x, 2.0)));
      weights               = mix(aperture_classic, inverse_aperture, floor(mod(coord.y, 2.0)));
      return weights;
   }

   else if(phosphor_layout == 8.){
      // delta_2_4x1_rgb
      const vec3 delta1[2][4] = {
         {red, yellow, cyan, blue},
         {cyan, blue, red, yellow}
      };
      
      w = int(floor(mod(coord.y, 2.0)));
      z = int(floor(mod(coord.x, 4.0)));
      
      weights = delta1[w][z];
      return weights;
   }

   else if(phosphor_layout == 9.){
      // delta_1_4x1_rgb; dunno why this is called 4x1 when it's obviously 4x2 /shrug
      const vec3 delta1[2][4] = {
         {red,  green, blue, black},
         {blue, black, red,  green}
      };
      
      w = int(floor(mod(coord.y, 2.0)));
      z = int(floor(mod(coord.x, 4.0)));
      
      weights = delta1[w][z];
      return weights;
   }
   
   else if(phosphor_layout == 10.){
      // delta_2_4x2_rgb
      const vec3 delta[4][4] = {
         {red,  yellow, cyan, blue},
         {red,  yellow, cyan, blue},
         {cyan, blue,   red,  yellow},
         {cyan, blue,   red,  yellow}
      };
      
      w = int(floor(mod(coord.y, 4.0)));
      z = int(floor(mod(coord.x, 4.0)));
      
      weights = delta[w][z];
      return weights;
   }

   else if(phosphor_layout == 11.){
      // slot mask for RGB panels; looks okay at 1080p, looks better at 4K
      const vec3 slotmask[4][6] = {
         {red, green, blue,    red, green, blue,},
         {red, green, blue,  black, black, black},
         {red, green, blue,    red, green, blue,},
         {black, black, black, red, green, blue,}
      };
      
      w = int(floor(mod(coord.y, 4.0)));
      z = int(floor(mod(coord.x, 6.0)));

      // use the indexes to find which color to apply to the current pixel
      weights = slotmask[w][z];
      return weights;
   }

   else if(phosphor_layout == 12.){
      // slot mask for RGB panels; looks okay at 1080p, looks better at 4K
      const vec3 slotmask[4][6] = {
         {black,  white, black,   black,  white, black,},
         {black,  white, black,  black, black, black},
         {black,  white, black,  black,  white, black,},
         {black, black, black,  black,  white, black,}
      };
      
      w = int(floor(mod(coord.y, 4.0)));
      z = int(floor(mod(coord.x, 6.0)));

      // use the indexes to find which color to apply to the current pixel
      weights = slotmask[w][z];
      return weights;
   }

   else if(phosphor_layout == 13.){
      // based on MajorPainInTheCactus' HDR slot mask
      const vec3 slot[4][8] = {
         {red,   green, blue,  black, red,   green, blue,  black},
         {red,   green, blue,  black, black, black, black, black},
         {red,   green, blue,  black, red,   green, blue,  black},
         {black, black, black, black, red,   green, blue,  black}
      };
      
      w = int(floor(mod(coord.y, 4.0)));
      z = int(floor(mod(coord.x, 8.0)));
      
      weights = slot[w][z];
      return weights;
   }

   else if(phosphor_layout == 14.){
      // same as above but for RGB panels
      const vec3 slot2[4][10] = {
         {red,   yellow, green, blue,  blue,  red,   yellow, green, blue,  blue },
         {black, green,  green, blue,  blue,  red,   red,    black, black, black},
         {red,   yellow, green, blue,  blue,  red,   yellow, green, blue,  blue },
         {red,   red,    black, black, black, black, green,  green, blue,  blue }
      };
   
      w = int(floor(mod(coord.y, 4.0)));
      z = int(floor(mod(coord.x, 10.0)));
      
      weights = slot2[w][z];
      return weights;
   }
   
   else return weights;
}

float weight(float x)
{
	x = abs(x);

	if (x < 1.0)
	{
		return
			(
			 ((x - 9.0 / 5.0) * x - 1.0 / 5.0 ) * x + 1.0
			);
	}
	else if ((x >= 1.0) && (x < 2.0))
	{
		return
			(
			 (( -1.0 / 3.0 * (x - 1) + 4.0 / 5.0 ) * (x - 1) - 7.0 / 15.0 ) * (x - 1)
			);
	}
	else
	{
		return 0.0;
	}
}

vec4 weight4(float x)
{
	return vec4(
			weight(x - 2.0),
			weight(x - 1.0),
			weight(x),
			weight(x + 1.0)
		   );
}


vec3 resampler3(vec3 x)
{
	vec3 res;
	res.x = (x.x<=0.001) ?  wa*wb  :  sin(x.x*wa)*sin(x.x*wb)/(x.x*x.x);
	res.y = (x.y<=0.001) ?  wa*wb  :  sin(x.y*wa)*sin(x.y*wb)/(x.y*x.y);
	res.z = (x.z<=0.001) ?  wa*wb  :  sin(x.z*wa)*sin(x.z*wb)/(x.z*x.z);
	return res;
}

float wgt(float size)
{
   size = clamp(size, -1.0, 1.0);

   size = 1.0 - size * size;

   return size * size * size;
}

float vignette(vec2 uv)
{
    float vignette = uv.x * uv.y * ( 1.0 - uv.x ) * ( 1.0 - uv.y );

    return clamp( pow( param.VIG_BASE * vignette, param.VIG_EXP ), 0.0, 1.0 );
}


vec3 get_content(vec2 vTex, vec2 uv)
{
    vec2 TextureSize = mix(vec2(global.SourceSize.x * param.SHARPNESS_HACK, global.SourceSize.y), vec2(global.SourceSize.x, global.SourceSize.y * param.SHARPNESS_HACK), param.VSCANLINES);

    vec2 dx = mix(vec2(1.0/TextureSize.x, 0.0), vec2(0.0, 1.0/TextureSize.y), param.VSCANLINES);
    vec2 dy = mix(vec2(0.0, 1.0/TextureSize.y), vec2(1.0/TextureSize.x, 0.0), param.VSCANLINES);

//    vec2 pix_coord = vTexCoord*TextureSize + vec2(-0.5, 0.5);
    vec2 pix_coord = vTex*TextureSize + vec2(-0.5, 0.5);

    vec2 tc = mix((floor(pix_coord) + vec2(0.5, 0.5))/TextureSize, (floor(pix_coord) + vec2(1.5, -0.5))/TextureSize, param.VSCANLINES);

    vec2 fp = mix(fract(pix_coord), fract(pix_coord.yx), param.VSCANLINES);

    vec3 c00 = GAMMA_IN(texture(Source, tc     - dx - dy).xyz);
    vec3 c01 = GAMMA_IN(texture(Source, tc          - dy).xyz);
    vec3 c02 = GAMMA_IN(texture(Source, tc     + dx - dy).xyz);
    vec3 c03 = GAMMA_IN(texture(Source, tc + 2.0*dx - dy).xyz);
    vec3 c10 = GAMMA_IN(texture(Source, tc     - dx     ).xyz);
    vec3 c11 = GAMMA_IN(texture(Source, tc              ).xyz);
    vec3 c12 = GAMMA_IN(texture(Source, tc     + dx     ).xyz);
    vec3 c13 = GAMMA_IN(texture(Source, tc + 2.0*dx     ).xyz);

    mat4x3 color_matrix0 = mat4x3(c00, c01, c02, c03);
    mat4x3 color_matrix1 = mat4x3(c10, c11, c12, c13);

    // Get weights for spline16 horizontal filter
    vec4 weights = weight4(1.0 - fp.x);

    // Spline16 horizontal filter
    vec3 color0   = clamp((color_matrix0 * weights)/dot(weights, vec4(1.0)), 0.0, 1.0);
    vec3 color1   = clamp((color_matrix1 * weights)/dot(weights, vec4(1.0)), 0.0, 1.0);
//    vec3 color0   = (color_matrix0 * weights)/dot(weights, vec4(1.0));
//    vec3 color1   = (color_matrix1 * weights)/dot(weights, vec4(1.0));


    // Get min/max samples
    vec3 min_sample0 = min(c01,c02);
    vec3 max_sample0 = max(c01,c02);
    vec3 min_sample1 = min(c11,c12);
    vec3 max_sample1 = max(c11,c12);
    
    // Anti-ringing
    vec3 aux = color0;
    color0 = clamp(color0, min_sample0, max_sample0);
    color0 = mix(aux, color0, CRT_ANTI_RINGING * step(0.0, (c00-c01)*(c02-c03)));
    aux = color1;
    color1 = clamp(color1, min_sample1, max_sample1);
    color1 = mix(aux, color1, CRT_ANTI_RINGING * step(0.0, (c10-c11)*(c12-c13)));

    // Apply scanlines. Sinc filter vertically.
    float pos0 = fp.y;
    float pos1 = 1 - fp.y;

    vec3 lum0 = mix(vec3(beam_min_width), vec3(beam_max_width), color0);
    vec3 lum1 = mix(vec3(beam_min_width), vec3(beam_max_width), color1);

// Using sinc scanlines
/*    vec3 d0 = clamp(scanlines_strength*pos0/(lum0*lum0+0.0000001), 0.0, 1.0);
    vec3 d1 = clamp(scanlines_strength*pos1/(lum1*lum1+0.0000001), 0.0, 1.0);

    d0 = resampler3(d0);
    d1 = resampler3(d1);

    // Apply color enhancement, scanlines orientation, mask and gamma.
    vec3 color = color_boost*(color0*d0+color1*d1)/(wa*wb);
*/

// Using nobody scanlines
    float bm0 = max(lum0.r, max(lum0.g, lum0.b));
    float bm1 = max(lum1.r, max(lum1.g, lum1.b));
    vec2 ddy = vec2(pos0, pos1) / ((scanlines_strength)*vec2(bm0, bm1));
    vec2 wy = vec2(wgt(ddy.x), wgt(ddy.y));
    vec3 color = color_boost*(color0*wy.x + color1*wy.y);

    color *= ((param.VIG_TOGGLE > 0.5) ? vignette(vTex) : 1.0);

    color  = GAMMA_OUT(color);

    if (param.PHOSPHOR_LAYOUT > 0.5)
    {
        //vec2 mask_coords = mix(vTex, uv, global.h_curvature) * mask_size;
	vec2 mask_coords = uv * ub_OutputSize.xy* fr_scale * 0.5;
        mask_coords = mix(mask_coords.xy, mask_coords.yx,  param.VSCANLINES);
        vec3 mask = mask_weights(mask_coords, param.PHOSPHOR_LAYOUT);
        mask = (param.MONITOR_SUBPIXELS > 0.5) ? mask.bgr : mask;
        color = mask + (1.0 - 2.0*mask)*pow(abs(mask - color), mask*param.MASK_STRENGTH*(param.MaskGamma - 1.0) + 1.0.xxx); 
    }
    
    color  = clamp(color, 0.0, 1.0);

    return color;
}


#define ReflexSrc Source

// Yeah, an unorthodox 'include' usage.
#include "../../include/uborder_bezel_reflections_main.inc"
