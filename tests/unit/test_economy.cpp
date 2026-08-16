/* Layer 1 tests for the pure economic formulas (economy.hpp).
 * Expected values are computed from the canonical formulas documented in
 * .devdocs/09-mapa-economia-taxas.md. */

#include "../tiny_test.hpp"

#include "lincity/economy.hpp"

TTEST(compute_cost_scales_with_tech) {
  // cost * (1 + cost_mul * tech / 1e6)
  TCHECK_EQ(100, compute_cost(100, 25, 0));
  TCHECK_EQ(2600, compute_cost(100, 25, 1000000));
  TCHECK_EQ(50000, compute_cost(50000, 2, 0));
  TCHECK_EQ(150000, compute_cost(50000, 2, 1000000));
  TCHECK_EQ(1, compute_cost(1, 25, 0));
  TCHECK_EQ(26, compute_cost(1, 25, 1000000));
  TCHECK_EQ(500, compute_cost(500, 10, 0));
  TCHECK_EQ(5500, compute_cost(500, 10, 1000000));
}

TTEST(compute_maintenance_cost_scales_with_tech) {
  TCHECK_EQ(100, compute_maintenance_cost(100, 1, 0));
  TCHECK_EQ(200, compute_maintenance_cost(100, 1, 1000000));
  TCHECK_EQ(2, compute_maintenance_cost(2, 1, 0));
  TCHECK_EQ(4, compute_maintenance_cost(2, 1, 1000000));
}

TTEST(compute_income_tax) {
  TCHECK_EQ(640, compute_income_tax(8000, 8));
  TCHECK_EQ(9, compute_income_tax(123, 8));  // integer truncation
  TCHECK_EQ(0, compute_income_tax(0, 8));
}

TTEST(compute_coal_and_ore_tax) {
  TCHECK_EQ(150, compute_coal_tax(100, 15));
  TCHECK_EQ(1, compute_coal_tax(1, 15));     // 15/10 truncates to 1
  TCHECK_EQ(150, compute_ore_tax(100, 15));
  TCHECK_EQ(1, compute_ore_tax(1, 15));
}

TTEST(compute_goods_tax_with_tech_bonus) {
  TCHECK_EQ(10, compute_goods_tax(1000, 1, 0));
  // tech bonus: tax * rate * tech / 2e6 = 10 * 1 * 1e6 / 2e6 = 5
  TCHECK_EQ(15, compute_goods_tax(1000, 1, 1000000));
}

TTEST(compute_export_discount_brackets) {
  TCHECK_EQ(0, compute_export_discount(0));
  TCHECK_EQ(0, compute_export_discount(25000));
  TCHECK_EQ(500, compute_export_discount(30000));
  // 900000 - triggers: 875000+850000+800000+700000+500000+100000 = 3825000/10
  TCHECK_EQ(382500, compute_export_discount(900000));
}

TTEST(compute_interest_capped) {
  TCHECK_EQ(0, compute_interest(0));
  TCHECK_EQ(0, compute_interest(1000));
  TCHECK_EQ(15000, compute_interest(-1000000)); // 1000 * 15
  TCHECK_EQ(1000000, compute_interest(-1000000000)); // capped
}

TTEST(compute_clamp_money) {
  TCHECK_EQ(0, compute_clamp_money(0));
  TCHECK_EQ(2000000000, compute_clamp_money(2000000000));
  TCHECK_EQ(2000000000, compute_clamp_money(2100000000));
  TCHECK_EQ(-2000000000, compute_clamp_money(-2000000000));
  TCHECK_EQ(-2000000000, compute_clamp_money(-2100000000));
}

TTEST(compute_tax_burden_labor_defaults_inert) {
  // at default rates the elasticity adds nothing
  TCHECK_EQ(0, compute_tax_burden_labor(8, 15, 15, 1));
  TCHECK_EQ(0, compute_tax_burden_labor(4, 10, 10, 0));
}

TTEST(compute_tax_burden_labor_raised_rates) {
  // income 16 (8 over), coal 30 (15 over), ore 30 (15 over),
  // goods 2 (1 over) = 39 extra labor
  TCHECK_EQ(39, compute_tax_burden_labor(16, 30, 30, 2));
  TCHECK_EQ(8, compute_tax_burden_labor(16, 15, 15, 1));
  TCHECK_EQ(1, compute_tax_burden_labor(8, 15, 15, 2)); // goods 2-1
}

TTEST(compute_tax_discomfort_defaults_inert) {
  TCHECK_EQ(0, compute_tax_discomfort(8, 15, 1));
}

TTEST(compute_tax_discomfort_raised_rates) {
  // income 16 -> 2*8=16; coal 30 -> 15; goods 2 -> 1 => 32
  TCHECK_EQ(32, compute_tax_discomfort(16, 30, 2));
  TCHECK_EQ(16, compute_tax_discomfort(16, 15, 1));
  TCHECK_EQ(15, compute_tax_discomfort(8, 30, 1));
}

TTEST(compute_school_cost_regressive_by_occupancy) {
  // SCHOOL_RUNNING_COST=2: 0% -> 4, 25% -> 3, 50% -> 2, >=75% -> 1
  TCHECK_EQ(4, compute_school_cost(0, 2));
  TCHECK_EQ(3, compute_school_cost(25, 2));
  TCHECK_EQ(2, compute_school_cost(50, 2));
  TCHECK_EQ(1, compute_school_cost(75, 2));
  TCHECK_EQ(1, compute_school_cost(100, 2));
}
