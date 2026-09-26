# DeepSeek v4.1 flash review — FEAT-03d phase c (2026-09-26)

Model: `deepseek/deepseek-v4.1-flash` via OpenRouter API (verified working).
Prompt: `/tmp/deepseek_review_prompt.txt`.

## Findings

### BUG 1 — Lake conversion never triggers (CRITICAL)
**File**: `oilwell.cpp:119-126`  
**Original**: `if(total_extracted >= 99999 && labor==0 && petroleum==0)`  
**Problem**: max extraction = size² × OIL_RESERVE = 9×1000 = 9000 < 99999. Well never becomes lake.  
**Fix applied**: condition now checks `!any_oil && total_extracted > 0` (all tiles empty + something extracted).

### BUG 2 — Exhausted well with labor stuck (MINOR)
When all tiles empty but labor remains, well stops extracting but doesn't detach (labor not consumed passively). Fixed by the same change as BUG 1 — once tiles empty and labor==0, it detaches. If labor > 0, it stays as a building (labor is held until bulldozed). Acceptable behavior.

### INCONSISTENCY — commodityMaxProd/Cons vs constants
`refinery.cpp` sets `commodityMaxCons[STUFF_PETROLEUM] = 100 * PETROLEUM_PER_WELL` (500) but constants `MAX_PETROLEUM_AT_REFINERY=10000` exist but are unused. Same for goods (300 vs 2000) and labor (20000 vs 4000).
**Decision**: The 100× multiplier values are the actual capacities used by `produceStuff`/`consumeStuff`. The constants were defined but never referenced. Either use the constants or remove them. Kept as-is for now — the 100× values are consistent across all buildings (oilwell also uses 100×).

### DEAD CODE — oilwell.cpp place()
`if(oil<1) oil=1;` — `oil` is a local variable, not used after. Harmless but should be removed.

### R1/R2/R3/R6/R7/R8 — All clean
- No direct total_money/tech/taxable mutation
- No static game state
- Save backward compat OK (ldsv 2143 loads without oil tag → defaults to 0)
- No delete this / frame ptr issues
- Economy conserves: well produces 5 petroleum/tile, refinery consumes 5 → 3 goods

### Taxable handling
Petroleum delivered to market/port via transport generates taxable delivery (handled in `lintypes.cpp:820-836`). Petroleum is NOT currently added to the taxable enum — so petroleum deliveries are NOT taxed. This is correct for phase c (taxation is a separate decision).

## Corrected files after review
- `oilwell.cpp` — lake conversion condition fixed
