# BUG-31: este caminho está morto e não compila. Verificado por
# tentativa, não por leitura:
#   - PainterGL não implementa os puro-virtuais do Painter atual
#     (drawTexture, clear, createTargetTexture, pushRenderTarget...), então
#     Video.cpp:144 faz `new PainterGL(window)` numa classe abstrata;
#   - TextureManagerGL.cpp:73 trata convert->format como ponteiro, mas em
#     SDL3 SDL_PixelFormat é valor.
# Ele nunca acompanhou nem a reescrita da interface Painter nem a mudança
# de formato de pixel do SDL3.
#
# O padrão já é desligado, e Config.cpp avisa no stderr e força
# useOpenGL=false quando GL é pedido num build sem GL — então não há
# armadilha para o jogador. Esta mensagem é para quem for reativar a
# opção por curiosidade: o build vai falhar, e a remoção do backend (e
# da option) é a correção de verdade, registrada no inventário.
set(LINCITYNG_DISABLE_GL_MODE ON CACHE BOOL "Disable direct OpenGL renderer (unsupported: the GL backend does not compile).")

if(LINCITYNG_DISABLE_GL_MODE)
  message(STATUS "lincity-ng: direct OpenGL renderer disabled "
                 "(PainterGL does not compile against the current Painter "
                 "interface nor SDL3; see BUG-31)")
endif()

add_library(lincity_gui  STATIC
  Button.cpp
  CheckButton.cpp
  Child.cpp
  Color.cpp
  Component.cpp
  ComponentFactory.cpp
  ComponentLoader.cpp
  Desktop.cpp
  DialogBuilder.cpp
  Document.cpp
  DocumentImage.cpp
  Event.cpp
  FilledRectangle.cpp
  Filter.cpp
  FontManager.cpp
  Gradient.cpp
  Image.cpp
  Panel.cpp
  Paragraph.cpp
  RadioButtonGroup.cpp
  Rect2D.cpp
  ScrollBar.cpp
  ScrollView.cpp
  Style.cpp
  SwitchComponent.cpp
  TableLayout.cpp
  TextureManager.cpp
  TooltipManager.cpp
  Vector2.cpp
  Window.cpp
  WindowManager.cpp

  Button.hpp
  CheckButton.hpp
  Child.hpp
  Color.hpp
  Component.hpp
  ComponentFactory.hpp
  ComponentLoader.hpp
  Desktop.hpp
  DialogBuilder.hpp
  Document.hpp
  DocumentElement.hpp
  DocumentImage.hpp
  Event.hpp
  FilledRectangle.hpp
  Filter.hpp
  FontManager.hpp
  Gradient.hpp
  Image.hpp
  Painter.hpp
  Panel.hpp
  Paragraph.hpp
  RadioButtonGroup.hpp
  Rect2D.hpp
  ScrollBar.hpp
  ScrollView.hpp
  Signal.hpp
  Style.hpp
  SwitchComponent.hpp
  TableLayout.hpp
  Texture.hpp
  TextureManager.hpp
  TooltipManager.hpp
  Vector2.hpp
  Window.hpp
  WindowManager.hpp

  # PainterGL/PainterGL.cpp
  # PainterGL/PainterGL.hpp
  # PainterGL/TextureGL.cpp
  # PainterGL/TextureGL.hpp
  # PainterGL/TextureManagerGL.cpp
  # PainterGL/TextureManagerGL.hpp

  PainterSDL/PainterSDL.cpp
  PainterSDL/PainterSDL.hpp
  PainterSDL/TextureSDL.cpp
  PainterSDL/TextureSDL.hpp
  PainterSDL/TextureManagerSDL.cpp
  PainterSDL/TextureManagerSDL.hpp
)

if(NOT LINCITYNG_DISABLE_GL_MODE)
  target_sources(lincity_gui PRIVATE
    PainterGL/PainterGL.cpp
    PainterGL/PainterGL.hpp
    PainterGL/TextureGL.cpp
    PainterGL/TextureGL.hpp
    PainterGL/TextureManagerGL.cpp
    PainterGL/TextureManagerGL.hpp
  )
endif()

if(LINCITYNG_DISABLE_GL_MODE)
	target_compile_definitions(lincity_gui PUBLIC DISABLE_GL_MODE)
endif()

add_library(LinCity::gui ALIAS lincity_gui)

target_link_libraries(lincity_gui
  PUBLIC
  $<IF:$<TARGET_EXISTS:SDL3::SDL3>,SDL3::SDL3,SDL3::SDL3-static>
  $<IF:$<TARGET_EXISTS:SDL3_ttf::SDL3_ttf>,SDL3_ttf::SDL3_ttf,SDL3_ttf::SDL3_ttf-static>
  xmlutil
  LibXml++-5.0::LibXml++-5.0
  PRIVATE
  fmt::fmt
  gettext
  ptrutil
)
if(NOT LINCITYNG_DISABLE_GL_MODE)
  target_link_libraries(lincity_gui PUBLIC OpenGL::GL)
endif()

target_include_directories(lincity_gui
  PUBLIC
  ${LIBXML2_INCLUDE_DIR}
)

include_directories(.)
