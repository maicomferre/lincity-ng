/*
Copyright (C) 2005 Matthias Braun <matze@braunis.de>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/**
 * @author Matthias Braun
 * @file FontManager.cpp
 */

#include "FontManager.hpp"

#include <SDL3/SDL.h>             // for SDL_GetError
#include <SDL3_ttf/SDL_ttf.h>     // for TTF_CloseFont, TTF_Font, TTF_OpenFont
#include <filesystem>             // for path, operator/, operator<<
#include <iostream>               // for cerr, ostream
#include <sstream>                // for basic_stringstream, basic_ostream
#include <stdexcept>              // for runtime_error
#include <utility>                // for pair, make_pair

#include "Style.hpp"              // for Style
#include "lincity-ng/Config.hpp"  // for getConfig, Config

FontManager* fontManager = 0;

namespace {

/* BUG-34: data/fonts/sans.ttf cobre Latim, Grego e Cirílico, e nada
 * além disso — nenhum kanji, hiragana, katakana ou pontuação CJK (a
 * cobertura se confirma por cmap). Mas o jogo envia ja.po e zh_CN.po, e
 * MainMenu.cpp:327 monta a lista de idiomas a partir dos .mo
 * instalados, então os dois aparecem e são selecionáveis: escolhê-los
 * renderiza a interface inteira em tofu.
 *
 * Empacotar uma fonte CJK resolveria, mas são 15 MB e é decisão de
 * distribuição, não de correção. O que fazemos é procurar uma fonte CJK
 * já instalada no sistema e emendá-la como fallback — o SDL3_ttf
 * 3.2.2 tem TTF_AddFallbackFont, e ela precisa ter o mesmo corpo e
 * estilo da fonte principal. Se o sistema não tiver nenhuma, o aviso
 * abaixo diz isso uma vez, em vez de o jogador descobrir no meio da
 * tela.
 *
 * A busca é por caminho conhecido, não por fontconfig: ligar o
 * fontconfig seria uma dependência a mais por causa de uma lista de
 * diretórios que cabe numa constante. */
const char* const kCjkFallbackCandidates[] = {
  // Linux (Debian/Fedora/Arch/Arch-derived)
  "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
  "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
  "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc",
  "/usr/share/fonts/opentype/ipafont-gothic/ipag.ttf",
  "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
  "/usr/share/fonts/adobe-source-han-sans/SourceHanSans-Regular.ttc",
  // macOS
  "/System/Library/Fonts/PingFang.ttc",
  "/System/Library/Fonts/Hiragino Sans GB.ttc",
  // Windows
  "C:/Windows/Fonts/msyh.ttc",   // Microsoft YaHei (zh)
  "C:/Windows/Fonts/msgothic.ttc" // MS Gothic (ja)
};

/* Abre a primeira fonte CJK do sistema, ou nullptr se não houver
 * nenhuma. Silencioso por design: quem chama decide se avisa. */
TTF_Font* openCjkFallback(int fontsize, int fontstyle) {
  for(const char* path : kCjkFallbackCandidates) {
    if(!std::filesystem::exists(path))
      continue;
    TTF_Font* font = TTF_OpenFont(path, fontsize);
    if(!font)
      continue;   // existe mas não abre (formato, permissão) — tenta o próximo
    if(fontstyle != 0)
      TTF_SetFontStyle(font, fontstyle);
    return font;
  }
  return nullptr;
}

/* Uma vez por processo: emendar a fallback custa abrir uma fonte, e
 * o aviso sobre CJK ausente também não precisa de repetição. */
bool warnedMissingCjk = false;

} // namespace

FontManager::FontManager()
{
}

FontManager::~FontManager()
{
    /* A fallback tem de sair antes da principal: TTF_ClearFallbackFonts
     * tira a referência, e só depois faz sentido fechar as duas. */
    for(Fonts::iterator i = fonts.begin(); i != fonts.end(); ++i)
        TTF_ClearFallbackFonts(i->second);
    for(Fonts::iterator i = fonts.begin(); i != fonts.end(); ++i)
        TTF_CloseFont(i->second);
    for(TTF_Font* fallback : cjkFallbacks)
        TTF_CloseFont(fallback);
}

bool
FontManager::attachCjkFallback(TTF_Font* font, int fontsize, int fontstyle)
{
    TTF_Font* cjk = openCjkFallback(fontsize, fontstyle);
    if(cjk) {
        /* TTF_AddFallbackFont NÃO fica com referência: fechar a fallback
         * aqui devolve a fonte principal ao estado sem cobertura (foi o
         * que aconteceu na primeira versão deste conserto — ver o teste
         * em .devdocs/06). O dono é o FontManager. */
        if(TTF_AddFallbackFont(font, cjk))
            cjkFallbacks.push_back(cjk);
        else
            TTF_CloseFont(cjk);
        return true;
    }
    if(!warnedMissingCjk) {
        warnedMissingCjk = true;
        std::cerr << "warning: no CJK font found on this system; Japanese and "
                     "Simplified Chinese (data/locale/ja.po, zh_CN.po) will "
                     "render as tofu.\n";
    }
    return false;
}

TTF_Font*
FontManager::getFont(Style style)
{
    FontInfo info;
    info.name = style.font_family;
    info.fontsize = (int) style.font_size;
    info.fontstyle = 0;
    if(style.italic)
        info.fontstyle |= TTF_STYLE_ITALIC;
    if(style.bold)
        info.fontstyle |= TTF_STYLE_BOLD;

    Fonts::iterator i = fonts.find(info);
    if(i != fonts.end())
        return i->second;

    std::filesystem::path fontsDir = getConfig()->appDataDir.get() / "fonts";
    std::filesystem::path fontfile = fontsDir / (info.name + ".ttf");
    TTF_Font* font = TTF_OpenFont(fontfile.string().c_str(), info.fontsize);
    if(!font) {
      std::stringstream msg;
      msg << "Error opening font " << fontfile
        << ": " << SDL_GetError();
      throw std::runtime_error(msg.str());
    }
    if(info.fontstyle != 0)
        TTF_SetFontStyle(font, info.fontstyle);
    attachCjkFallback(font, info.fontsize, info.fontstyle);

    fonts.insert(std::make_pair(info, font));
    return font;
}


/** @file gui/FontManager.cpp */
