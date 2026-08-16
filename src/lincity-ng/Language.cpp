/* ---------------------------------------------------------------------- *
 * src/lincity-ng/Language.cpp
 * This file is part of Lincity-NG.
 *
 * Copyright (C) 2005      Matthias Braun <matze@braunis.de>
 * Copyright (C) 2024-2025 David Bears <dbear4q@gmail.com>
 * Copyright (C) 2026      Marc Young <myoung008@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
** ---------------------------------------------------------------------- */

#include "main.hpp"

#include <cassert>                               // for assert
#include <clocale>                               // for NULL, setlocale, LC_ALL
#include <cstdlib>                               // for getenv, setenv, unsetenv
#include <cstring>                               // for strcmp
#include <optional>                              // for optional, nullopt

#include "config.h"                              // for HAVE_NL_MSG_CAT_CNTR, ENABLE_NLS

std::optional<std::string> oldLanguage = std::nullopt;

#ifdef HAVE_NL_MSG_CAT_CNTR
// glibc gettext magic
extern "C" int _nl_msg_cat_cntr;
#endif
void
setLang(const std::string& lang) {
  if(lang != "autodetect") {
#ifdef WIN32
    _putenv_s("LANGUAGE", lang.c_str());
#else
    setenv("LANGUAGE", lang.c_str(), 1);
#endif
  }
  else if(oldLanguage) {
#ifdef WIN32
    _putenv_s("LANGUAGE", oldLanguage->c_str());
#else
    setenv("LANGUAGE", oldLanguage->c_str(), 1);
#endif
  }
  else {
#ifdef WIN32
    _putenv_s("LANGUAGE", "");
#else
    unsetenv("LANGUAGE");
#endif
  }
#define GETTEXT_HAS_

#ifdef HAVE_NL_MSG_CAT_CNTR
  // glibc gettext magic
  ++_nl_msg_cat_cntr;
#endif
}

// This tries to get the same language that as gettext.
std::string
getLang() {
#if ENABLE_NLS
  const char *locale = setlocale(LC_MESSAGES, NULL);
#else
  const char *locale = "C.UTF-8";
#endif
  assert(locale);
  if(locale && !strcmp(locale, "C")) return "C";
  const char *language = getenv("LANGUAGE");
  if(language && *language) return language;
  if(locale) return locale;
  return "";
}


/** @file lincity-ng/Language.cpp */
