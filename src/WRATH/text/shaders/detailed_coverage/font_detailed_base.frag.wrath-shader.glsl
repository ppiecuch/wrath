/*! 
 * \file font_detailed_base.frag.wrath-shader.glsl
 * \brief file font_detailed_base.frag.wrath-shader.glsl
 * 
 * Copyright 2013 by Nomovok Ltd.
 * 
 * Contact: info@nomovok.com
 * 
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 * 
 * \author Kevin Rogovin <kevin.rogovin@nomovok.com>
 * 
 */




uniform mediump sampler2D wrath_DetailedCoverageTexture;
uniform mediump sampler2D wrath_DetailedIndexTexture; 

mediump float
wrath_detailed_wrath_glyph_compute_coverage(in mediump vec2 GlyphCoordinate,
                                            in mediump vec2 GlyphNormalizedCoordinate,
                                            in mediump float GlyphIndex)
{
  mediump vec4 loc_sz;
  mediump vec2 idx_tex;
  mediump vec2 ptex;

  #if defined(WRATH_DERIVATIVES_SUPPORTED)
  {
    mediump float sc;
    mediump vec2 dx, dy;

    /*
      dx and dy give derivatives
      of GlyphCoordinate with repsect
      to the screen. Thus, value less
      than 1.0 means GlyphCoordnate changes 
      slower than the screen pixels,
      whic means it is zoom _in_.
      This is why there is inversesqrt.
     */
    dx=dFdx(GlyphCoordinate);
    dy=dFdy(GlyphCoordinate);
    sc=clamp(inversesqrt( dot(dx,dx) + dot(dy,dy) ), 0.0, 1.0);
    idx_tex=vec2(sc, GlyphIndex);
  }
  #else
  {
    idx_tex=vec2(1.0, GlyphIndex);
  }
  #endif

  
  loc_sz=texture2D(wrath_DetailedIndexTexture, idx_tex);
  ptex=loc_sz.xy + GlyphNormalizedCoordinate*loc_sz.zw;

  return texture2D(wrath_DetailedCoverageTexture, ptex).r;
                   
}

mediump float 
wrath_detailed_wrath_glyph_is_covered(in mediump vec2 GlyphCoordinate,
                                      in mediump vec2 GlyphNormalizedCoordinate,
                                      in mediump float GlyphIndex)
{
  return step(0.5, wrath_detailed_wrath_glyph_compute_coverage(GlyphCoordinate,
                                                               GlyphNormalizedCoordinate,
                                                               GlyphIndex));
}
