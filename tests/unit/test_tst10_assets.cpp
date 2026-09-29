/* TST-10: asset reference and contract validation.
 *
 * Scans active XML files for asset references and verifies
 * every referenced file exists on disk. Detects Sea Farm
 * assets as present (BUG-23) without accusing commented-out
 * bridge entries.
 *
 * Any new XML sprite/help reference must have a matching
 * file, or this test fails.
 */

#include "../tiny_test.hpp"

#include <filesystem>
#include <fstream>
#include <set>
#include <string>

#include "lincity/init_game.hpp"
#include "lincity/lintypes.hpp"
#include "lincity/modules/all_modules.hpp"
#include "lincity/modules/sea_farm.hpp"

namespace {

std::string read_file(const std::filesystem::path& p) {
  std::ifstream f(p);
  if(!f) return "";
  return std::string((std::istreambuf_iterator<char>(f)), {});
}

/* Strip XML comments <!-- ... --> */
std::string strip_comments(const std::string& s) {
  std::string out;
  std::size_t i = 0;
  while(i < s.size()) {
    if(i + 3 < s.size() && s[i] == '<'
      && s[i+1] == '!' && s[i+2] == '-'
      && s[i+3] == '-') {
      i += 4;
      while(i + 2 < s.size()) {
        if(s[i] == '-' && s[i+1] == '-'
          && s[i+2] == '>') { i += 3; break; }
        ++i;
      }
    } else {
      out += s[i++];
    }
  }
  return out;
}

/* Collect <image file="..."/> references from images.xml. */
std::set<std::string> images_xml_refs(
  const std::string& content) {
  std::set<std::string> refs;
  std::string clean = strip_comments(content);
  std::size_t pos = 0;
  while(true) {
    auto start = clean.find("<image ", pos);
    if(start == std::string::npos) break;
    auto filePos = clean.find("file=\"", start);
    if(filePos == std::string::npos) break;
    filePos += 6;
    auto endPos = clean.find('"', filePos);
    if(endPos == std::string::npos) break;
    refs.insert(clean.substr(filePos, endPos - filePos));
    pos = endPos;
  }
  return refs;
}

/* Collect img src="..." references from help XML. */
std::set<std::string> help_xml_refs(
  const std::string& content) {
  std::set<std::string> refs;
  std::string clean = strip_comments(content);
  std::size_t pos = 0;
  while(true) {
    auto start = clean.find("<img ", pos);
    if(start == std::string::npos) break;
    auto srcPos = clean.find("src=\"", start);
    if(srcPos == std::string::npos) break;
    srcPos += 5;
    auto endPos = clean.find('"', srcPos);
    if(endPos == std::string::npos) break;
    refs.insert(clean.substr(srcPos, endPos - srcPos));
    pos = endPos;
  }
  return refs;
}

TTEST(tst10_images_xml_refs_exist) {
  const auto base = std::filesystem::path(
    LINCITYNG_TEST_DATA_DIR);
  const auto imagesXml = base / "images" / "tiles" / "images.xml";

  const std::string content = read_file(imagesXml);
  TCHECK(!content.empty());
  if(content.empty()) return;

  const auto refs = images_xml_refs(content);
  TCHECK(!refs.empty());

  int missing = 0;
  for(const auto& ref : refs) {
    const auto full = base / "images" / "tiles" / ref;
    if(!std::filesystem::exists(full)) {
      std::fprintf(stderr,
        "  images.xml missing: %s\n", ref.c_str());
      ++missing;
    }
  }
  TCHECK_EQ(missing, 0);
}

TTEST(tst10_help_xml_refs_exist) {
  const auto base = std::filesystem::path(
    LINCITYNG_TEST_DATA_DIR);
  const auto helpDir = base / "help" / "en";

  std::ifstream f(helpDir / "farm.xml");
  TCHECK(f.good());
  if(!f.good()) return;

  int missing = 0;
  for(const auto& entry :
      std::filesystem::directory_iterator(helpDir)) {
    if(entry.path().extension() != ".xml") continue;
    const std::string content = read_file(entry.path());
    if(content.empty()) continue;
    const auto refs = help_xml_refs(content);
    for(const auto& ref : refs) {
      const auto full = base / ref;
      if(!std::filesystem::exists(full)) {
        std::fprintf(stderr,
          "  help missing: %s (from %s)\n",
          ref.c_str(),
          entry.path().filename().string().c_str());
        ++missing;
      }
    }
  }
  TCHECK_EQ(missing, 0);
}

TTEST(tst10_sea_farm_assets_present) {
  const auto base = std::filesystem::path(
    LINCITYNG_TEST_DATA_DIR);

  const char* tiles[] = {
    "seafarm0.png", "seafarm3.png", "seafarm7.png",
    "seafarm11.png", "seafarm15.png"
  };
  for(const char* t : tiles) {
    TCHECK(std::filesystem::exists(
      base / "images" / "tiles" / t));
  }
  TCHECK(std::filesystem::exists(
    base / "images" / "gui" / "buttonpanel" / "parks"
    / "seafarm.png"));

  initializeModules();
  const auto it = ResourceGroup::resMap.find("SeaFarm");
  TCHECK(it != ResourceGroup::resMap.end());
}

TTEST(tst10_sea_farm_can_build_on_water) {
  initializeModules();
  city_settings city;
  city.with_village = true;
  std::unique_ptr<World> world = new_city(&city, 100,
    SimulationProtocol::DETERMINISTIC_V1, 0);
  TCHECK(world != nullptr);

  MapPoint p(-1, -1);
  for(MapPoint c(2, 2); c.y < world->map.len() - 6;
      ++c.y) {
    for(c.x = 2; c.x < world->map.len() - 6; ++c.x) {
      bool ok = true;
      for(int dy = 0; dy < GROUP_SEA_FARM_SIZE
            && ok; ++dy)
        for(int dx = 0; dx < GROUP_SEA_FARM_SIZE;
            ++dx)
          if(!world->map(MapPoint(c.x + dx,
              c.y + dy))->is_bare()) {
            ok = false; break;
          }
      if(ok) { p = c; break; }
    }
    if(p.x >= 0) break;
  }
  TCHECK(p.x >= 0);
  if(p.x < 0) return;
  for(int dy = 0; dy < GROUP_SEA_FARM_SIZE; ++dy)
    for(int dx = 0; dx < GROUP_SEA_FARM_SIZE; ++dx)
      world->map(MapPoint(p.x + dx, p.y + dy))
        ->group = GROUP_WATER;

  Message::ptr msg;
  auto* cg = dynamic_cast<SeaFarmConstructionGroup*>(
    ConstructionGroup::getConstructionGroup(
      GROUP_SEA_FARM));
  TCHECK(cg != nullptr);
  if(!cg) return;
  TCHECK(cg->can_build_here(*world, p, msg));
}

} // namespace
