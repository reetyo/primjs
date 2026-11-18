#include <cstdio>
#include <cstdlib>
#include <malloc/malloc.h>

extern "C" {
#include "quickjs/include/quickjs.h"
}

#include "gc/trace-gc.h"
#include "quickjs/include/quickjs-inner.h"
#include "quickjs/source/quickjs-parser-lookahead.h"

static int host_flags;
int GetSettingsFlag() { return host_flags; }

static void *Allocate(JSMallocState *, size_t size, int) {
  return std::malloc(size);
}
static void Free(JSMallocState *, void *ptr) { std::free(ptr); }
static void *Reallocate(JSMallocState *, void *ptr, size_t size, int) {
  return std::realloc(ptr, size);
}

int main() {
  const LEPUSMallocFunctions allocator = {
      Allocate, Free, Reallocate, malloc_size};
  host_flags = PARSER_LOOKAHEAD_ENABLE;
  LEPUSRuntime *first = LEPUS_NewRuntime();
  if (!first) return 2;
  host_flags = 0;
  LEPUSRuntime *direct = LEPUS_NewRuntime2(&allocator, nullptr, 0);
  LEPUSRuntime *next = LEPUS_NewRuntime();
  if (!direct || !next) return 2;
  std::fprintf(stdout, "mode=%s first_host_on=%u direct_host_off=%u normal_host_off=%u\n",
              LEPUS_IsGCModeRT(first) ? "GC" : "RC",
              first->parser_lookahead_flags, direct->parser_lookahead_flags,
              next->parser_lookahead_flags);
  LEPUS_FreeRuntime(next);
  LEPUS_FreeRuntime(direct);
  LEPUS_FreeRuntime(first);
}
