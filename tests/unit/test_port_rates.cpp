/* Layer 1 test: PORT_*_RATE constants match the FEAT-03e benchmark
 * (Rocket_98.scn.gz, seed 1234567, 1500 days, port configured
 * import food/coal/ore, export goods/steel).
 *
 * Benchmark changes: ORE 1→50, STEEL 100→30, PETROLEUM new=40.
 * FOOD_RATE=1 was not changed by the benchmark (doc 13 says 500 —
 * discrepancy flagged, not a code change).
 */

#include "../tiny_test.hpp"

#include "lincity/modules/port.hpp"

TTEST(port_rates_benchmark) {
  TCHECK_EQ(PORT_ORE_RATE, 50);       // was 1 ($0,01) → 50
  TCHECK_EQ(PORT_STEEL_RATE, 30);     // was 100 → 30
  TCHECK_EQ(PORT_PETROLEUM_RATE, 40); // between COAL=50, STEEL=30
  TCHECK_EQ(PORT_FOOD_RATE, 1);       // unchanged (doc 13: 500 — flag)
  TCHECK_EQ(PORT_COAL_RATE, 50);      // unchanged
  TCHECK_EQ(PORT_GOODS_RATE, 3);      // unchanged
}
