/*! 
 * \file WRATHTextureFontFreeType_Mix.hpp
 * \brief file WRATHTextureFontFreeType_Mix.hpp
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




#ifndef WRATH_HEADER_TEXTURE_FONT_FreeType_MIX_HPP_
#define WRATH_HEADER_TEXTURE_FONT_FreeType_MIX_HPP_

#include "WRATHConfig.hpp"
#include <typeinfo>
#include "WRATHTextureFontFreeType_MixTypes.tcc"
#include "WRATHTextureFontFreeType_Coverage.hpp"
#include "WRATHTextureFontFreeType_DetailedCoverage.hpp"
#include "WRATHTextureFontFreeType_Analytic.hpp"
#include "WRATHTextureFontFreeType_CurveAnalytic.hpp"
#include "WRATHTextureFontFreeType_Distance.hpp"

/*! \addtogroup Text
 * @{
 */

/*!\class WRATHTextureFontFreeType_TMix

  A WRATHTextureFontFreeType_TMix uses two fonts:
  a T font for it's  native resolution 
  and a S for it's minified resolution. 
  The requirements on the types T and S are:
  - publically derived from WRATHTextureFont

  The texture handles returned by texture_binder(int)
  is an array of first the elements from
  T::texture_binder() followed by the
  elements from S::texture_binder().

  The fragment source code returned by
  \ref glyph_glsl() defines the additional 
  functions and symbols:
  - native_wrath_glyph_compute_coverage() is wrath_glyph_compute_coverage() function taken from the
    fragment source code of the native sized font (T) [fragment shader only]
  - native_wrath_glyph_is_covered() is wrath_glyph_is_covered() function taken from the
    fragment source code of the native sized font (T) [fragment shader only]
  - minified_wrath_glyph_compute_coverage() is wrath_glyph_compute_coverage() function taken from the
    fragment source code of the minified font (S) [fragment shader only]
  - minified_wrath_glyph_is_covered() is wrath_glyph_is_covered() function taken from the
    fragment source code of the minified font (S) [fragment shader only]
  - MIX_FONT_SHADER is the ratio of the pixel sizes of the
    native font (T) to the minified font (S). The value is used to determine
    in wrath_glyph_is_covered() and wrath_glyph_compute_coverage() to decide which of the fonts to
    use. A WRATHFontShaderSpecifier can define this symbol in their
    pre-vertex-shader (\ref WRATHFontShaderSpecifier::append_pre_vertex_shader_source()) 
    and pre-fragment-shader (\ref WRATHFontShaderSpecifier::append_pre_fragment_shader_source()) 
    to customize this value.

  \tparam T WRATHTextureFont derived type used for the
            native_value data of the mix font.

  \tparam S WRATHTextureFont derived type used for the
            minified_values data of the mix font.
 */
template<typename T, typename S=WRATHTextureFontFreeType_Coverage>
class WRATHTextureFontFreeType_TMix:
  public WRATHTextureFontFreeTypeT<WRATHTextureFontFreeType_TMix<T,S> >
{
public:
  using WRATHTextureFont::glyph_data;
  using WRATHTextureFontFreeTypeT<WRATHTextureFontFreeType_TMix>::fetch_font;

  enum
    {
      /*!
        Enumeration to indicate that the font 
        is scalable.
       */
      font_scalability_value=T::font_scalability_value
    };

  /*!\fn WRATHTextureFontFreeType_TMix(WRATHFreeTypeSupport::LockableFace::handle,
                                       const WRATHTextureFontKey &presource_name)
    Ctor for WRATHTextureFontFreeType_TMix, it is HIGHLY advised
    to use fetch_font() to create/get fonts from files.
    This routine will fetch both the S and T fonts,
    use the type's fetch_font() method.
    \param pttfFace passed to the ctor of the underlying
                    T and S fonts.
    \param presource_name specifies the WRATHFontDatabase::Font source
                          and the pixel size, becomes the key for the font.
   */
  WRATHTextureFontFreeType_TMix(WRATHFreeTypeSupport::LockableFace::handle pttfFace,  
                                const WRATHTextureFontKey &presource_name):
    WRATHTextureFontFreeTypeT<WRATHTextureFontFreeType_TMix<T,S> >(pttfFace, presource_name),
    m_size_ratio(default_size_divider())
  {
    m_minified_src=create_minified_font();
    m_native_src=create_native_font();
    common_init();
  }

  
  /*!\fn WRATHTextureFontFreeType_TMix(WRATHFreeTypeSupport::LockableFace::handle,
                                       T*, S*, const WRATHTextureFontKey &)
    Ctor.
    Create a WRATHTextureFontFreeType_TMix using a given
    T and S font. The two font objects provided MUST
    source from the exact same WRATHFontDatabase::Font object.
    \param dist_font font used as the "native" resolution data.
    \param minified_font font used as the "minified" resolution data
    \param pttfFace passed to the ctor of the underlying
                    T and S fonts.
    \param rkey specifies the resource name of the font
  */
  WRATHTextureFontFreeType_TMix(WRATHFreeTypeSupport::LockableFace::handle pttfFace,
                                T *dist_font,
                                S *minified_font,
                                const WRATHTextureFontKey &rkey):
    WRATHTextureFontFreeTypeT<WRATHTextureFontFreeType_TMix<T,S> >(pttfFace, rkey),
    m_minified_src(minified_font),
    m_native_src(dist_font),
    m_size_ratio(static_cast<float>(dist_font->pixel_size())
                 /static_cast<float>(minified_font->pixel_size()))
  {
    WRATHassert(dist_font->source_font()==minified_font->source_font());
    common_init();
  }

  ~WRATHTextureFontFreeType_TMix()
  {}

  virtual
  const_c_array<WRATHTextureChoice::texture_base::handle>
  texture_binder(int texture_page)
  {
    return m_page_tracker.texture_binder(texture_page);
  }
  
  virtual
  int
  glyph_custom_float_data_size(void) const
  {
    return m_glyph_custom_float_data_size;
  }

  virtual
  int
  texture_page_data_size(void) const
  {
    return m_texture_page_data_size;
  }

  virtual
  float
  texture_page_data(int texture_page, int idx) const
  {
    return (0<=idx and idx<m_texture_page_data_size)?
      m_page_tracker.custom_data(texture_page)[idx]:
      0.0f;
  }

  virtual
  int
  number_texture_pages(void)
  {
    return m_page_tracker.number_texture_pages();
  }
  
  virtual
  const WRATHTextureFont::GlyphGLSL*
  glyph_glsl(void)
  {
    return m_glyph_glsl;
  }

  /*!\fn S* minified_font_src
    Returns the font used as the source
    of the minified font data, i.e
    the returned font's native_value
    are used as this's minified_values.
   */
  S*
  minified_font_src(void)
  {
    return m_minified_src;
  }

  /*!\fn T* native_font_src
    Returns the font used by this
    font as the native values, i.e.
    the returned font's native_value
    are this font's native_value.
   */
  T*
  native_font_src(void)
  {
    return m_native_src;
  }

  /*!\fn WRATHTextureFont* fetch_font(int, int, const WRATHFontDatabase::Font::const_handle &hnd)
    Checks if a WRATHTextureFontFreeType_Mix from the WRATHFontDatabase::Font
    source and point size has been created, if so returns it, 
    otherwise creates a new WRATHTextureFontFreeType_Mix of those 
    parameters and returns it. The font's resource name will be: 
    WRATHTextureFontKey(hnd, native_psize, X) where X is
 
    typeid(WRATHTextureFontFreeType_TMix).name(), minified_psize
    \n\n

    This routine requires that both S and T have the static function:\n
    \code
    WRATHTextureFont*
    fetch_font(int psize, const WRATHFontDatabase::Font::const_handle &hnd);
    \endcode \n

    where Type is T and S repsectively.    

    \param native_psize pointsize to for native font use.
    \param minified_psize pointsize to use for minified font.
    \param hnd WRATHFontDatabase::Font from which to source
   */
  static
  WRATHTextureFont*
  fetch_font(int native_psize, int minified_psize,
             const WRATHFontDatabase::Font::const_handle &hnd);

  /*!\fn float default_size_divider(void)
    Returns the the pixel of the
    minified font (type S) divided the 
    pixel size of the non-minified font
    (type T) when a font object is requested
    via fetch_font(int, const std::string&, int)
    [since that function only specifies the
    pixel size of the non-minified font, T].
   */
  static
  float
  default_size_divider(void) 
  {
    return datum().default_size_divider();    
  }

  /*!\fn void default_size_divider(float)  
    Sets the the pixel of the
    minified font (type S) divided the 
    pixel size of the non-minified font
    (type T) when a font object is requested
    via fetch_font(int, const std::string&, int)
    [since that function only specifies the
    pixel size of the non-minified font, T].
    \param v value to use
   */
  static
  void
  default_size_divider(float v) 
  {
    datum().default_size_divider(v);
  }
  
  /*!\fn void minified_font_inflate_factor(float)
    Set the value C where C is used in the 
    default mix font shader as follows:
    Let D=minification factor at which a
    glyph is displayed, let F=ratio of
    the pixel size of the non-minified
    font divided by the pixel size of
    the minified font. The non-minified font is
    used if D < F/C. Note that a value of
    C=1.0 simply means that the minified font
    is displayed whenever the glyph is displayed
    at the size of the minified font or smaller.
    In general a value of C means that the
    glyph is displayed using the minified font
    whenever the glyph is displayed at the size
    C*pixel-size-of-minified-font or smaller.
    Default value is 1.0. For a given mix font 
    type this value should be set _before_ any
    such mix fonts are created and not changed
    once any font of that mixed font type is created.
    \param v value to use
   */
  static
  void
  minified_font_inflate_factor(float v)
  {
    datum().minified_font_inflate_factor(v);
  }

  /*!\fn float minified_font_inflate_factor(void)
    Returns the value as set by
    minified_font_inflate_factor(float)
   */
  static
  float
  minified_font_inflate_factor(void)
  {
    return datum().minified_font_inflate_factor();
  }

protected:
  
  virtual
  void
  on_increment_use_count(void)
  {
    m_minified_src->increment_use_count();
    m_native_src->increment_use_count();
    WRATHTextureFontFreeType::on_increment_use_count();
  }
  
  virtual
  void
  on_decrement_use_count(void)
  {
    m_minified_src->decrement_use_count();
    m_native_src->decrement_use_count();
    WRATHTextureFontFreeType::on_decrement_use_count();
  }

private:

  static
  WRATHTextureFontFreeType_TMixSupport::PerMixClass&
  datum(void);

  virtual
  WRATHTextureFont::glyph_data_type*
  generate_character(WRATHTextureFont::glyph_index_type G);
  
  S*
  create_minified_font(void);
    
  T*
  create_native_font(void);

  void
  on_create_texture_page(void);

  void
  common_init(void);

  WRATHFreeTypeSupport::LockableFace::handle m_ttf_face;

  S *m_minified_src;
  T *m_native_src;
  float m_size_ratio;
  const WRATHTextureFont::GlyphGLSL *m_glyph_glsl;
  int m_texture_page_data_size, m_glyph_custom_float_data_size;
  int m_glyph_custom_native_start, m_glyph_custom_minified_start;
  
  WRATHMutex m_mutex;
  bool m_new_page;

  WRATHTextureFontUtil::TexturePageTracker m_page_tracker;
};


/*!\class WRATHMixFontTypes

  Typedef machine defining various 
  mix font types.
  \tparam T "base" font type
 */
template<typename T>
class WRATHMixFontTypes
{
public:

  /*!\typedef base

    Base font type, i.e. the font type
    used for the non-minified font.
   */
  typedef T base;

  /*!\typedef mix
    
    Mixed font type where minification font
    is a \ref WRATHTextureFontFreeType_Coverage
    and the non-minified font type is \ref base.
   */
  typedef WRATHTextureFontFreeType_TMix<base> mix;

  /*!\typedef hq_mix
    
    Mixed font type where minification font
    is a \ref WRATHTextureFontFreeType_DetailedCoverage
    and the non-minified font type is \ref base.
   */
  typedef WRATHTextureFontFreeType_TMix<base, WRATHTextureFontFreeType_DetailedCoverage> hq_mix;

  /*!\typedef self_mix
    
    Mixed font type where both the minification font
    and the non-minified font type are \ref base.
   */
  typedef WRATHTextureFontFreeType_TMix<base, base> self_mix;
};


namespace WRATHFontFetch
{
  /*!\fn void font_fetcher(type_tag<T>, type_tag<S>)
    Provided as a readability conveniance, equivalent
    to
    \code
    font_fetcher(type_tag<WRATHTextureFontFreeType_TMix<T,S> >());
    \endcode
    \tparam T font type for magnification
    \tparam S font type for minification
   */
  template<typename T, typename S>
  void
  font_fetcher(type_tag<T>, type_tag<S>)
  {
    font_fetcher(type_tag<WRATHTextureFontFreeType_TMix<T,S> >());
  }

  
  /*!\fn void font_fetcher(type_tag<T>, type_tag<S>, float)
    Provided as a readability conveniance, equivalent
    to
    \code
    font_fetcher(type_tag<WRATHTextureFontFreeType_TMix<T,S> >());
    WRATHTextureFontFreeType_TMix<T,S>::default_size_divider(pdefault_size_divider);
    \endcode
    \tparam T font type for magnification
    \tparam S font type for minification
    \tparam pdefault_size_divider value to which to set the default size divider,
                                  see \ref WRATHTextureFontFreeType_TMix<T,S>::default_size_divider(float)
   */
  template<typename T, typename S>
  void
  font_fetcher(type_tag<T>, type_tag<S>, float pdefault_size_divider)
  {
    font_fetcher(type_tag<WRATHTextureFontFreeType_TMix<T,S> >());
    WRATHTextureFontFreeType_TMix<T,S>::default_size_divider(pdefault_size_divider);
  }
}

#include "WRATHTextureFontFreeType_MixImplement.tcc"




/*! @} */

#endif
