#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

extern "C" {
#include "quickjs/include/quickjs.h"
}

#include "gc/trace-gc.h"
#include "quickjs/include/quickjs-inner.h"
#include "quickjs/source/quickjs-parser-lookahead.h"

int GetSettingsFlag() {
  const char* variant = std::getenv("PRIMJS_BENCH_LOOKAHEAD");
  return variant && variant[0] == '3' ? PARSER_LOOKAHEAD_ENABLE : 0;
}

int main(int argc, char** argv) {
  if (argc != 5) {
    std::fprintf(stderr, "usage: eval_bench source.js compile|eval warmups samples\n");
    return 2;
  }
  std::ifstream file(argv[1], std::ios::binary);
  std::string source((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
  const std::string mode(argv[2]);
  const int warmups = std::atoi(argv[3]);
  const int samples = std::atoi(argv[4]);
  if (source.empty() || (mode != "compile" && mode != "eval") ||
      warmups < 0 || samples < 1)
    return 2;

  const char* variant = std::getenv("PRIMJS_BENCH_LOOKAHEAD");
  if (!variant || variant[0] < '0' || variant[0] > '3' || variant[1]) return 2;
  const int flags = LEPUS_EVAL_TYPE_GLOBAL |
                    (mode == "compile" ? LEPUS_EVAL_FLAG_COMPILE_ONLY : 0);
  for (int i = -warmups; i < samples; ++i) {
    LEPUSRuntime* runtime = LEPUS_NewRuntime();
    if (!runtime) return 2;
    const uint8_t expected =
        variant[0] == '3' ? primjs::kAllLookaheadOptimizations : 0;
    if (runtime->parser_lookahead_flags != expected) return 2;
    LEPUSContext* context = LEPUS_NewContext(runtime);
    if (!context) return 2;

    const auto start = std::chrono::steady_clock::now();
    LEPUSValue result = LEPUS_Eval(context, source.c_str(), source.size(),
                                   "workload.js", flags);
    const auto stop = std::chrono::steady_clock::now();
    if (LEPUS_IsException(result)) {
      std::fputs("LEPUS_Eval threw\n", stderr);
      return 1;
    }
    if (mode == "eval") {
      int32_t value = 0;
      if (LEPUS_ToInt32(context, &value, result) != 0 || value != 42) return 1;
    }
    const bool gc = LEPUS_IsGCMode(context);
    if (!gc) LEPUS_FreeValue(context, result);
    LEPUS_FreeContext(context);
    LEPUS_FreeRuntime(runtime);
    if (i >= 0) {
      std::fprintf(stdout, "%.6f,%d\n",
                  std::chrono::duration<double, std::milli>(stop - start).count(),
                  gc);
    }
  }
}
