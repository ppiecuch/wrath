/*! 
 * \file WRATHFontShaderSpecifier.cpp
 * \brief file WRATHFontShaderSpecifier.cpp
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



#include "WRATHConfig.hpp"
#include "WRATHFontShaderSpecifier.hpp"
#include "WRATHStaticInit.hpp"
#include "WRATHShaderBrushSourceHoard.hpp"

namespace
{
  class DefaultShaders:boost::noncopyable
  {
  public:    
    WRATHGLShader::shader_source m_vertex_shader;
    WRATHGLShader::shader_source m_aa_fragment_shader;
    WRATHGLShader::shader_source m_non_aa_fragment_shader;
    
    static
    const DefaultShaders&
    instance(void)
    {
      WRATHStaticInit();
      static DefaultShaders R;
      return R;
    }

  private:
    DefaultShaders(void)
    {
      m_vertex_shader.add_source("simple_ui_font.vert.wrath-shader.glsl", WRATHGLShader::from_resource);
      m_aa_fragment_shader.add_source("font_generic_aa.frag.wrath-shader.glsl", WRATHGLShader::from_resource);
      m_non_aa_fragment_shader.add_source("font_generic.frag.wrath-shader.glsl", WRATHGLShader::from_resource);
    }

  };

  
  class ReadyShaderSrcMap:public std::map<GLenum, WRATHGLShader::shader_source>
  {
  public:
    ReadyShaderSrcMap(const std::string &brush_macro,
                      const std::string &fragment_src)
    {
      operator[](GL_VERTEX_SHADER)
        .add_macro(brush_macro)
        .add_source("simple_ui_font.vert.wrath-shader.glsl", WRATHGLShader::from_resource);

      operator[](GL_FRAGMENT_SHADER)
        .add_macro(brush_macro)
        .add_source(fragment_src, WRATHGLShader::from_resource); 
    }
  };

  class FontBrushHoard:
    public ReadyShaderSrcMap,
    public WRATHShaderBrushSourceHoard
  {
  public:
    FontBrushHoard(const std::string &macro,
                   const std::string &fragment_src):
      ReadyShaderSrcMap(macro, fragment_src),
      WRATHShaderBrushSourceHoard(static_cast<const ReadyShaderSrcMap&>(*this))
    {}
  };
}


WRATH_RESOURCE_MANAGER_IMPLEMENT(WRATHFontShaderSpecifier, 
                                 WRATHFontShaderSpecifier::ResourceKey);

WRATHFontShaderSpecifier::
WRATHFontShaderSpecifier(const ResourceKey &pname,
                         const WRATHGLShader::shader_source &vs,
                         const WRATHGLShader::shader_source &fs,
                         const WRATHGLProgramInitializerArray &initers,
                         const WRATHGLProgramOnBindActionArray &on_bind_actions):
  m_resource_name(pname),
  m_remove_from_manager(true),
  m_initializers(initers),
  m_bind_actions(on_bind_actions),
  m_modifiable(true),
  m_font_discard_thresh(0.9f),
  m_linear_glyph_position(true)
{
  append_vertex_shader_source()=vs;
  append_fragment_shader_source()=fs;
  resource_manager().add_resource(m_resource_name, this);
}

WRATHFontShaderSpecifier::
WRATHFontShaderSpecifier(const WRATHGLShader::shader_source &vs,
                         const WRATHGLShader::shader_source &fs,
                         const WRATHGLProgramInitializerArray &initers,
                         const WRATHGLProgramOnBindActionArray &on_bind_actions):
  m_resource_name(),
  m_remove_from_manager(false),
  m_initializers(initers),
  m_bind_actions(on_bind_actions),
  m_modifiable(true),
  m_font_discard_thresh(0.9f),
  m_linear_glyph_position(true)
{
  append_vertex_shader_source()=vs;
  append_fragment_shader_source()=fs;
}




WRATHFontShaderSpecifier::
~WRATHFontShaderSpecifier()
{
  for(map_type::iterator iter=m_actual_creators.begin(),
        end=m_actual_creators.end(); iter!=end; ++iter)
    {
      WRATHDelete(iter->second);
    }

  if(m_remove_from_manager)
    {
      resource_manager().remove_resource(this);
    }
}

void
WRATHFontShaderSpecifier::
add_shader_source_code(const WRATHBaseSource *src,
                       enum WRATHBaseSource::precision_t prec,
                       const std::string &suffix)
{
  src->add_shader_source_code(append_all_shader_sources(),
                              prec, suffix);
                              
}

void
WRATHFontShaderSpecifier::
add_pre_shader_source_code(const WRATHBaseSource *src,
                       enum WRATHBaseSource::precision_t prec,
                       const std::string &suffix)
{
  src->add_shader_source_code(append_all_pre_shader_sources(),
                              prec, suffix);
                              
}



WRATHTextureFontDrawer*
WRATHFontShaderSpecifier::
fetch_texture_font_drawer(const WRATHTextureFont::GlyphGLSL *fs_source,
                          const WRATHItemDrawerFactory &factory,
                          const WRATHTextAttributePacker *text_packer,
                          int sub_drawer_id) const
{
  WRATHAutoLockMutex(m_mutex);

  const WRATHAttributePacker *attribute_packer;
  int number_custom_to_use;
  
  number_custom_to_use=fs_source->m_custom_data_use.size();
  attribute_packer=text_packer->fetch_attribute_packer(number_custom_to_use);

  map_type::iterator iter;
  m_modifiable=false;

  iter=m_actual_creators.find(fs_source);
  if(iter!=m_actual_creators.end())
    {
      WRATHShaderSpecifier *sp(iter->second);

      return sp->fetch_two_pass_drawer<WRATHTextureFontDrawer>(factory, attribute_packer, 
                                                               sub_drawer_id, true);
    }

  WRATHShaderSpecifier *new_specifier;

  /*
    "private" shader specifier, not resource managed.
   */
  new_specifier=WRATHNew WRATHShaderSpecifier();

  /*
    basic idea:
     1. add fragment sources from fs_source and also use the 
        compute scaling factor mode to add vertex shader
        and fragment shader source code
     2. add initializers coming from fs_source listing of samplers
     3. add this's initializes and binder actions.
   */

  new_specifier->translucent_threshold(font_discard_thresh());
  new_specifier->append_bind_actions()=bind_actions();

  new_specifier->append_initializers()=initializers();

  for(int i=0, endi=fs_source->m_sampler_names.size(); i<endi; ++i)
    {
      new_specifier->append_initializers()
        .add_sampler_initializer(fs_source->m_sampler_names[i], i);

      new_specifier->append_bindings()
        .add_texture_binding(GL_TEXTURE0+i);
    }

  for(std::map<unsigned int, std::string>::const_iterator iter=m_additional_textures.begin(),
        end=m_additional_textures.end(); iter!=end; ++iter)
    {
      unsigned int S;
      
      S=iter->first + fs_source->m_sampler_names.size();

      new_specifier->append_initializers()
        .add_sampler_initializer(iter->second, S);

      new_specifier->append_bindings()
        .add_texture_binding(GL_TEXTURE0+S);
    }

  /*
    ISSUE: The font shading system does not support shading
    stages beyond vertex and fragment shading; we should 
    likely make it support it, the main issue it looks like
    is WRATHTextureFont::GlyphGLSL interface, perhaps
    use an std::map for its way to specify shader code?
   */

  enum WRATHTextureFont::GlyphGLSL::glyph_position_linearity v;
  const char *linearity_macro[]=
    {
      /*[linear_glyph_position]=   */ "WRATH_TEXTURE_FONT_LINEAR",
      /*[nonlinear_glyph_position]=*/ "WRATH_TEXTURE_FONT_NONLINEAR"
    };

  v=(m_linear_glyph_position)?
    WRATHTextureFont::GlyphGLSL::linear_glyph_position:
    WRATHTextureFont::GlyphGLSL::nonlinear_glyph_position;
  
  WRATHGLShader::shader_source cst;
  if(number_custom_to_use!=0)
    {
      const char *struct_def=
        "\nstruct wrath_font_custom_data_t"
        "\n{"
        "\n\thighp float values[WRATH_FONT_CUSTOM_DATA];"
        "\n};\n";

      cst
        .add_macro("WRATH_FONT_CUSTOM_DATA", number_custom_to_use)
        .add_source(struct_def, WRATHGLShader::from_string);
      
      text_packer->generate_custom_data_glsl(cst, number_custom_to_use);
    }
  else
    {
      cst.add_macro("WRATH_FONT_NO_CUSTOM_DATA");
    }

  
  /*
    pre-shader source codes.
   */
  new_specifier->append_pre_vertex_shader_source()
    .add_macro(linearity_macro[v])
    .absorb(vertex_pre_shader_source());
  
  new_specifier->append_pre_fragment_shader_source()
    .add_macro(linearity_macro[v])
    .absorb(fragment_pre_shader_source());
    

  if(fs_source->m_texture_page_data_size==0)
    {
      new_specifier->append_vertex_shader_source()
        .add_macro("WRATH_FONT_TEXTURE_PAGE_DATA_EMPTY");

      new_specifier->append_fragment_shader_source()
        .add_macro("WRATH_FONT_TEXTURE_PAGE_DATA_EMPTY");
    }
  
  /*
    append shader codes
   */
  new_specifier->append_vertex_shader_source()
    .add_macro("WRATH_FONT_TEXTURE_PAGE_DATA_SIZE", 
               fs_source->m_texture_page_data_size)
    .add_source("font_shader_texture_page_data.wrath-shader.glsl", 
                WRATHGLShader::from_resource)
    .absorb(fs_source->m_vertex_processor[v])
    .absorb(cst)
    .add_source("font_shader_wrath_prepare_glyph_vs.vert.wrath-shader.glsl",
                WRATHGLShader::from_resource)
    .absorb(vertex_shader_source());
  
  new_specifier->append_fragment_shader_source()
    .add_macro("WRATH_FONT_TEXTURE_PAGE_DATA_SIZE", 
               fs_source->m_texture_page_data_size)
    .add_source("font_shader_texture_page_data.wrath-shader.glsl", 
                WRATHGLShader::from_resource)
    .absorb(fs_source->m_fragment_processor[v])
    .absorb(fragment_shader_source());
    
  m_actual_creators[fs_source]=new_specifier;

  
  return new_specifier->fetch_two_pass_drawer<WRATHTextureFontDrawer>(factory, 
                                                                      attribute_packer, 
                                                                      sub_drawer_id, true);
}

const WRATHGLShader::shader_source&
WRATHFontShaderSpecifier::
default_vertex_shader(void)
{
  return DefaultShaders::instance().m_vertex_shader;
}


const WRATHGLShader::shader_source&
WRATHFontShaderSpecifier::
default_aa_fragment_shader(void)
{
  return DefaultShaders::instance().m_aa_fragment_shader;
}

const WRATHGLShader::shader_source&
WRATHFontShaderSpecifier::
default_non_aa_fragment_shader(void)
{
  return DefaultShaders::instance().m_non_aa_fragment_shader;
}


const WRATHFontShaderSpecifier&
WRATHFontShaderSpecifier::
default_aa(void)
{
  WRATHStaticInit();
  static WRATHFontShaderSpecifier R(default_vertex_shader(),
                                    default_aa_fragment_shader());
  return R;                                    
}

const WRATHFontShaderSpecifier&
WRATHFontShaderSpecifier::
default_non_aa(void)
{
  WRATHStaticInit();
  static WRATHFontShaderSpecifier R(default_vertex_shader(),
                                    default_non_aa_fragment_shader());
  return R;                                    
}

const WRATHFontShaderSpecifier&
WRATHFontShaderSpecifier::
default_brush_item_aa(const WRATHShaderBrush &brush)
{
  WRATHStaticInit();
  static FontBrushHoard H("WRATH_APPLY_BRUSH_RELATIVE_TO_ITEM", 
                          "font_generic_aa.frag.wrath-shader.glsl");
  return H.fetch_font_shader(brush, WRATHBaseSource::mediump_precision);                                    
}

const WRATHFontShaderSpecifier&
WRATHFontShaderSpecifier::
default_brush_letter_aa(const WRATHShaderBrush &brush)
{
  WRATHStaticInit();
  static FontBrushHoard H("WRATH_APPLY_BRUSH_RELATIVE_TO_LETTER", 
                          "font_generic_aa.frag.wrath-shader.glsl");
  return H.fetch_font_shader(brush, WRATHBaseSource::mediump_precision);                                    
}


const WRATHFontShaderSpecifier&
WRATHFontShaderSpecifier::
default_brush_item_non_aa(const WRATHShaderBrush &brush)
{
  WRATHStaticInit();
  static FontBrushHoard H("WRATH_APPLY_BRUSH_RELATIVE_TO_ITEM", 
                          "font_generic.frag.wrath-shader.glsl");
  return H.fetch_font_shader(brush, WRATHBaseSource::mediump_precision);                                    
}

const WRATHFontShaderSpecifier&
WRATHFontShaderSpecifier::
default_brush_letter_non_aa(const WRATHShaderBrush &brush)
{
  WRATHStaticInit();
  static FontBrushHoard H("WRATH_APPLY_BRUSH_RELATIVE_TO_LETTER", 
                          "font_generic.frag.wrath-shader.glsl");
  return H.fetch_font_shader(brush, WRATHBaseSource::mediump_precision);                                    
}
