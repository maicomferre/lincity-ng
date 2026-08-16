/* headless_env.hpp - shared bootstrap for headless engine tests.
 *
 * Initializes Config, SDL (dummy drivers), the module/factory registries,
 * the RNG with a fixed seed and a muted Sound instance, mirroring the
 * startup sequence of the real game binary without opening a display.
 */

#ifndef LINCITYNG_TESTS_HEADLESS_ENV_HPP
#define LINCITYNG_TESTS_HEADLESS_ENV_HPP

#include <cstdint>
#include <filesystem>

namespace headless {

/* create config, SDL, registries, RNG seed and muted sound */
void init_env(const std::filesystem::path& appData,
  const std::filesystem::path& userData, uint64_t seed);

void shutdown_env();

const std::filesystem::path& app_data();
const std::filesystem::path& user_data();

} // namespace headless

#endif // LINCITYNG_TESTS_HEADLESS_ENV_HPP
