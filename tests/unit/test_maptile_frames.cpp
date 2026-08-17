/* Layer 1 tests for MapTile ExtraFrame ownership (REF-03b).
 * Invariants (see .devdocs/13-framesptr-ciclo-de-vida.md §6):
 *  1. hasFrames() == false exactly when the tile owns no frames
 *  2. an iterator returned by addFrame() stays valid until its own
 *     removeFrame(it) (other elements may come and go)
 *  3. removeFrame(it) with an iterator of another tile aborts in debug
 *  4. moveFrameTo() does not invalidate the iterator (same node, new tile)
 *  5. after any op sequence, total live frames == adds - removes
 *
 * Map/MapTile are standalone (no globals touched), so this runs as a pure
 * unit test. The random-op test uses a fixed LCG seed so failures reproduce.
 */

#include "../tiny_test.hpp"

#include <cstdint>
#include <list>
#include <vector>

#include "lincity/resources.hpp"  // for ExtraFrame
#include "lincity/world.hpp"       // for Map, MapTile

namespace {

struct LiveFrame {
  MapTile* tile;
  std::list<ExtraFrame>::iterator it;
  int marker;
};

uint64_t test_rng = 0x12345678abcdefULL;
uint32_t rng_next() {
  test_rng = test_rng * 6364136223846793005ULL + 1442695040888963407ULL;
  return static_cast<uint32_t>(test_rng >> 33);
}

} // namespace

TTEST(maptile_add_remove_lifecycle) {
  Map map(4);
  MapTile* tile = map(MapPoint(1, 1));
  TCHECK(!tile->hasFrames());

  std::list<ExtraFrame>::iterator it = tile->addFrame();
  TCHECK(tile->hasFrames());
  TCHECK_EQ(it->frame, 0);      // ExtraFrame default state
  TCHECK_EQ(it->move_x, 0);
  it->frame = 42;
  it->move_x = -3;
  TCHECK_EQ(it->frame, 42);
  TCHECK_EQ(it->move_x, -3);
  TCHECK_EQ(tile->frames().size(), 1u);

  tile->removeFrame(it);
  TCHECK(!tile->hasFrames());   // list deleted when it emptied
}

TTEST(maptile_iterator_stability_across_removals) {
  Map map(4);
  MapTile* tile = map(MapPoint(2, 2));

  std::list<ExtraFrame>::iterator a = tile->addFrame();
  std::list<ExtraFrame>::iterator b = tile->addFrame();
  std::list<ExtraFrame>::iterator c = tile->addFrame();
  a->frame = 1;
  b->frame = 2;
  c->frame = 3;
  TCHECK_EQ(tile->frames().size(), 3u);

  tile->removeFrame(b);         // remove from the middle
  TCHECK_EQ(tile->frames().size(), 2u);
  TCHECK_EQ(a->frame, 1);       // survivors stay valid (invariant 2)
  TCHECK_EQ(c->frame, 3);

  tile->removeFrame(a);
  TCHECK_EQ(c->frame, 3);
  tile->removeFrame(c);
  TCHECK(!tile->hasFrames());
}

TTEST(maptile_moveframe_keeps_iterator_valid) {
  Map map(4);
  MapTile* src = map(MapPoint(1, 2));
  MapTile* dst = map(MapPoint(3, 2));

  std::list<ExtraFrame>::iterator it = src->addFrame();
  it->frame = 77;

  src->moveFrameTo(map, MapPoint(3, 2), it);
  TCHECK(!src->hasFrames());    // source list emptied and deleted
  TCHECK(dst->hasFrames());
  TCHECK_EQ(dst->frames().size(), 1u);
  TCHECK_EQ(it->frame, 77);     // same node, now owned by dst (invariant 4)

  dst->removeFrame(it);         // the moved iterator is killable by the new owner
  TCHECK(!dst->hasFrames());
}

TTEST(maptile_random_ops_invariants) {
  Map map(4);
  MapTile* tiles[4] = {
    map(MapPoint(0, 0)), map(MapPoint(1, 1)),
    map(MapPoint(2, 2)), map(MapPoint(3, 3)),
  };
  std::vector<LiveFrame> live;
  int next_marker = 1;

  for(int op = 0; op < 2000; ++op) {
    const uint32_t roll = rng_next() % 100;
    if(roll < 45 || live.empty()) {
      // addFrame on a random tile
      MapTile* t = tiles[rng_next() % 4];
      auto it = t->addFrame();
      it->frame = next_marker;
      live.push_back({t, it, next_marker});
      ++next_marker;
    } else if(roll < 75) {
      // removeFrame of a random live frame through its OWNING tile
      size_t idx = rng_next() % live.size();
      live[idx].tile->removeFrame(live[idx].it);
      live.erase(live.begin() + idx);
    } else {
      // moveFrameTo of a random live frame to a different tile
      size_t idx = rng_next() % live.size();
      MapTile* src = live[idx].tile;
      size_t si = 0;
      while(tiles[si] != src) {
        ++si;
      }
      size_t di = rng_next() % 4;
      if(di == si) {
        di = (di + 1) % 4;
      }
      MapTile* dst = tiles[di];
      src->moveFrameTo(map, dst->point, live[idx].it);
      live[idx].tile = dst;
    }

    // Invariant 1 + 5 after every op: per-tile live count matches the
    // hasFrames()/frames().size() view of the world.
    for(MapTile* t : tiles) {
      size_t count = 0;
      for(const LiveFrame& lf : live) {
        if(lf.tile == t) {
          ++count;
        }
      }
      TCHECK_EQ(t->hasFrames(), count > 0);
      if(count > 0) {
        TCHECK_EQ(t->frames().size(), count);
      }
    }
    // Invariant 2 + 4: every live iterator still derefs to its marker.
    for(const LiveFrame& lf : live) {
      TCHECK_EQ(lf.it->frame, lf.marker);
    }
  }

  // Drain: removing every live frame empties all lists (invariant 5).
  for(LiveFrame& lf : live) {
    lf.tile->removeFrame(lf.it);
  }
  for(MapTile* t : tiles) {
    TCHECK(!t->hasFrames());
  }
}

#ifndef NDEBUG
// REF-03d: the whole-map consistency check must hold after mixed ops.
TTEST(maptile_consistency_check_passes) {
  Map map(4);
  std::list<ExtraFrame>::iterator a = map(MapPoint(0, 0))->addFrame();
  std::list<ExtraFrame>::iterator b = map(MapPoint(1, 1))->addFrame();
  map(MapPoint(2, 2))->addFrame();
  map.assertFramesConsistent();  // 3 registered frames on 3 tiles

  map(MapPoint(0, 0))->moveFrameTo(map, MapPoint(3, 3), a);
  map.assertFramesConsistent();  // splice keeps addresses: registry untouched

  map(MapPoint(1, 1))->removeFrame(b);
  map.assertFramesConsistent();  // kill unregisters

  TCHECK(map(MapPoint(3, 3))->hasFrames());
}

// REF-03d: destroying a map with frames still alive (World teardown, test
// scopes) must leave the debug registry clean for the next map in the same
// process — every test here runs its own Map(4) in one binary.
TTEST(maptile_teardown_with_live_frames_keeps_registry_clean) {
  {
    Map map(4);
    map(MapPoint(1, 1))->addFrame();
    std::list<ExtraFrame>::iterator it = map(MapPoint(2, 2))->addFrame();
    map(MapPoint(2, 2))->moveFrameTo(map, MapPoint(0, 0), it);
    // scope ends with 2 live frames owned by the map
  }
  Map next(4);
  next(MapPoint(3, 3))->addFrame();
  next.assertFramesConsistent();  // must not trip on stale addresses
  TCHECK(next(MapPoint(3, 3))->hasFrames());
}

#if defined(__linux__)
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

TTEST(maptile_cross_tile_remove_aborts_in_debug) {
  // Invariant 3: prove the ownership assert actually fires. A child process
  // kills tile B's frame through tile A; the parent expects SIGABRT. Runs
  // only in debug builds on Linux (CI is ubuntu; release builds skip it).
  pid_t pid = fork();
  if(pid == 0) {
    Map map(4);
    std::list<ExtraFrame>::iterator foreign = map(MapPoint(1, 1))->addFrame();
    map(MapPoint(2, 2))->addFrame();
    map(MapPoint(2, 2))->removeFrame(foreign);  // cross-tile kill
    _exit(0);  // reached only if the assert failed to fire
  }
  int status = 0;
  waitpid(pid, &status, 0);
  TCHECK(WIFSIGNALED(status));
  TCHECK_EQ(WTERMSIG(status), SIGABRT);
}

TTEST(maptile_double_kill_aborts_in_debug) {
  // REF-03d: prove the registry assert catches a double kill. The tile's
  // list is NOT empty when the second kill happens (another frame remains),
  // so the abort can only come from the registry membership assert (or, in
  // sanitizer builds, from the UAF read of the dead iterator — ASan exits
  // nonzero instead of raising SIGABRT, so accept both outcomes).
  pid_t pid = fork();
  if(pid == 0) {
    Map map(4);
    MapTile* tile = map(MapPoint(1, 1));
    std::list<ExtraFrame>::iterator victim = tile->addFrame();
    tile->addFrame();  // keep the list alive after the first kill
    tile->removeFrame(victim);
    tile->removeFrame(victim);  // double kill
    _exit(0);  // reached only if detection failed
  }
  int status = 0;
  waitpid(pid, &status, 0);
  const bool clean_exit = WIFEXITED(status) && WEXITSTATUS(status) == 0;
  TCHECK(!clean_exit);
}
#endif // __linux__
#endif // !NDEBUG
