#include "tiny_test.hpp"

int main(int argc, char** argv) {
  for(int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if(arg == "--filter") {
      if(i + 1 >= argc) {
        std::fprintf(stderr, "--filter needs a parameter\n");
        return 1;
      }
      tiny_test::set_filter(argv[++i]);
    } else if(arg == "--ts") {
      tiny_test::set_timestamps(true);
    } else if(arg == "--list-tests") {
      return tiny_test::list_tests();
    } else {
      std::fprintf(stderr, "unrecognized argument: %s\n", arg.c_str());
      return 1;
    }
  }
  return tiny_test::run_all_tests();
}
