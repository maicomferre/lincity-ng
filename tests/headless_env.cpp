#include "headless_env.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <libxml/xmlversion.h>

#include <clocale>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

#include "gui/ComponentFactory.hpp"          // for initFactories, component_factories
#include "gui/FontManager.hpp"              // for fontManager
#include "gui/Painter.hpp"                  // for Painter (complete type for delete)
#include "gui/TextureManager.hpp"            // for texture_manager
#include "lincity-ng/Config.hpp"             // for Config, getConfig
#include "lincity-ng/Sound.hpp"              // for Sound
#include "lincity-ng/main.hpp"               // for painter, window
#include "lincity/modules/all_modules.hpp"   // for initializeModules
#include "lincity/resources.hpp"             // for ResourceGroup, ResourceGroup::resMap
#include "util/randutil.hpp"                 // for BasicUrbg

extern Config* configPtr;
// window_renderer is a file-scope global in Video.cpp with no extern
// header; we need it here to destroy it in shutdown_env.
extern SDL_Renderer* window_renderer;

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

  // BUG-10: ResourceGroup instances are singletons allocated with `new`
  // in initializeModules/all_modules and registered in the static
  // ResourceGroup::resMap. Nothing in the engine deletes them — they
  // live for the whole process. In the real game the OS reclaims them at
  // exit, but under ASan that reads as a leak and masks real bugs. Tear
  // them down explicitly here so the headless test exit is clean.
  // ResourceGroup::~ResourceGroup erases itself from resMap, so swap the
  // map into a local first (same pattern as World::~World for
  // vehicleList) and then delete. The ~ResourceGroup also SDL_DestroySurface's
  // any unconverted image and deletes textures created via
  // texture_manager->create() (those are NOT in texture_manager's own
  // cache, which only tracks load() — see TextureManager.cpp:80).
  std::map<std::string, ResourceGroup*> groups;
  groups.swap(ResourceGroup::resMap);
  for(auto& [_, grp] : groups)
    delete grp;

  // texture_manager is created in initVideo() and owns the textures
  // registered via load() (TextureManager.cpp:80). In the real game it is
  // deleted in main.cpp:127; the headless harness never did that, so
  // those textures and the manager itself leaked. Delete it AFTER the
  // ResourceGroups so the textures created via create() (owned by the
  // groups) are gone first.
  delete texture_manager;
  texture_manager = nullptr;

  // initFactories() new's 16 Factory singletons into the global
  // component_factories map, but IMPLEMENT_COMPONENT_FACTORY (used by
  // PBar.cpp, EconomyGraph.cpp, ...) also inserts STATIC factory objects
  // into the same map. The two kinds cannot be told apart at cleanup time,
  // so deleting the map entries would double-free the static ones. Leave
  // them alone — they are 15 x 8-byte process-lifetime singletons and are
  // suppressed via tests/lsan_suppressions.txt (leak:initFactories).

  // initVideo() (called by some tests) new's painter/fontManager and
  // creates SDL_Window/SDL_Renderer. In the real game these live for the
  // whole process; in the headless test they leaked. Tear them down here,
  // before SDL_Quit.
  delete painter;
  painter = nullptr;
  delete fontManager;
  fontManager = nullptr;
  if(window_renderer) {
    SDL_DestroyRenderer(window_renderer);
    window_renderer = nullptr;
  }
  if(window) {
    SDL_DestroyWindow(window);
    window = nullptr;
  }

  delete configPtr;
  configPtr = nullptr;

  TTF_Quit();
  SDL_Quit();
}

const std::filesystem::path& app_data() { return g_appData; }
const std::filesystem::path& user_data() { return g_userData; }

} // namespace headless
