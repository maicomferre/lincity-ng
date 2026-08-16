#include "headless_env.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <libxml/xmlversion.h>

#include <clocale>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

#include "gui/ComponentFactory.hpp"          // for initFactories
#include "lincity-ng/Config.hpp"             // for Config, getConfig
#include "lincity-ng/Sound.hpp"              // for Sound
#include "lincity/modules/all_modules.hpp"   // for initializeModules
#include "util/randutil.hpp"                 // for BasicUrbg

extern Config* configPtr;

namespace headless {

namespace {
std::filesystem::path g_appData;
std::filesystem::path g_userData;
std::unique_ptr<Sound> g_sound;
} // namespace

void init_env(const std::filesystem::path& appData,
    const std::filesystem::path& userData, uint64_t seed) {
  g_appData = appData;
  g_userData = userData;

  LIBXML_TEST_VERSION;

  std::error_code ec;
  std::filesystem::create_directories(g_userData, ec);
  if(ec)
    throw std::runtime_error("cannot create user data dir: " +
      ec.message());

  setlocale(LC_ALL, "");
  setenv("SDL_VIDEODRIVER", "dummy", 1);
  setenv("SDL_AUDIODRIVER", "dummy", 1);

  configPtr = new Config();
  const std::string appDataStr = g_appData.string();
  const std::string userDataStr = g_userData.string();
  char prog[] = "lincity-test";
  char appDataArg[] = "--app-data";
  char userDataArg[] = "--user-data";
  char muteArg[] = "-m";
  char sdlArg[] = "-s";
  char* argv[] = {prog, appDataArg, const_cast<char*>(appDataStr.data()),
    userDataArg, const_cast<char*>(userDataStr.data()), muteArg, sdlArg};
  getConfig()->init(7, argv);

  if(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS))
    throw std::runtime_error(std::string("SDL_Init failed: ")
      + SDL_GetError());
  if(!TTF_Init())
    throw std::runtime_error(std::string("TTF_Init failed: ")
      + SDL_GetError());

  BasicUrbg::get().reseed(seed);
  srand(seed);

  initFactories();
  initializeModules();
  g_sound.reset(new Sound());
}

void shutdown_env() {
  g_sound.reset();
  TTF_Quit();
  SDL_Quit();
}

const std::filesystem::path& app_data() { return g_appData; }
const std::filesystem::path& user_data() { return g_userData; }

} // namespace headless
