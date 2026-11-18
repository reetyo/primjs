// Copyright 2026 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include <string>

#include "gtest/gtest.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "quickjs/include/quickjs-libc.h"
#include "quickjs/include/quickjs.h"
#ifdef __cplusplus
}
#endif
#include "gc/trace-gc.h"
#include "quickjs/include/quickjs-inner.h"
#include "quickjs/source/quickjs-parser-lookahead.h"

namespace parser_lookahead_test {

// Evaluates `src` (expected to throw) and returns the exception's stack.
static std::string EvalAndGetStack(LEPUSContext* ctx, const char* src) {
  LEPUSValue ret =
      LEPUS_Eval(ctx, src, strlen(src), "test.js", LEPUS_EVAL_TYPE_GLOBAL);
  EXPECT_TRUE(LEPUS_IsException(ret));
  if (!LEPUS_IsGCMode(ctx)) LEPUS_FreeValue(ctx, ret);

  std::string result;
  LEPUSValue exception_val = LEPUS_GetException(ctx);
  HandleScope func_scope(ctx, &exception_val, HANDLE_TYPE_LEPUS_VALUE);
  LEPUSValue stack_val = LEPUS_GetPropertyStr(ctx, exception_val, "stack");
  if (!LEPUS_IsUndefined(stack_val)) {
    const char* stack = LEPUS_ToCString(ctx, stack_val);
    if (stack) result = stack;
    if (!LEPUS_IsGCMode(ctx)) LEPUS_FreeCString(ctx, stack);
  }
  if (!LEPUS_IsGCMode(ctx)) {
    LEPUS_FreeValue(ctx, stack_val);
    LEPUS_FreeValue(ctx, exception_val);
  }
  return result;
}

static const char* kWrappedIdentifier =
    "function f(){\n  return (((missingValue))) + (5);\n}\nf();";
static const char* kNestedLiteral =
    "function f(){\n  return ({x: [1, 2, innermost]});\n}\nf();";

class ParserLookaheadTest : public ::testing::Test {
 protected:
  struct Engine {
    LEPUSRuntime* rt = nullptr;
    LEPUSContext* ctx = nullptr;
  };

  static Engine Create(uint8_t flags) {
    Engine e;
    e.rt = LEPUS_NewRuntime();
    // Override whatever the host setting returned so the test is
    // deterministic.
    e.rt->parser_lookahead_flags = flags;
    e.ctx = LEPUS_NewContext(e.rt);
    return e;
  }

  static void Destroy(Engine& e) {
    LEPUS_FreeContext(e.ctx);
    LEPUS_FreeRuntime(e.rt);
  }
};

// With the setting off, identifier columns keep the historical values
// (pointing at the closing delimiter of the scanned group).
TEST_F(ParserLookaheadTest, ColumnsWithSettingOff) {
  Engine off = Create(0);
  EXPECT_NE(EvalAndGetStack(off.ctx, kWrappedIdentifier).find(":2:29"),
            std::string::npos);
  EXPECT_NE(EvalAndGetStack(off.ctx, kNestedLiteral).find(":2:34"),
            std::string::npos);
  Destroy(off);
}

// With the setting on, identifier columns point at the identifier itself.
TEST_F(ParserLookaheadTest, ColumnsWithSettingOn) {
  Engine on = Create(primjs::kAllLookaheadOptimizations);
  EXPECT_NE(EvalAndGetStack(on.ctx, kWrappedIdentifier).find(":2:13"),
            std::string::npos);
  EXPECT_NE(EvalAndGetStack(on.ctx, kNestedLiteral).find(":2:22"),
            std::string::npos);
  Destroy(on);
}

// Creating a runtime with a different setting must not change the behavior
// of a runtime that already exists.
TEST_F(ParserLookaheadTest, SettingIsPerRuntime) {
  Engine a = Create(0);
  EXPECT_NE(EvalAndGetStack(a.ctx, kWrappedIdentifier).find(":2:29"),
            std::string::npos);

  Engine b = Create(primjs::kAllLookaheadOptimizations);
  EXPECT_NE(EvalAndGetStack(b.ctx, kWrappedIdentifier).find(":2:13"),
            std::string::npos);

  EXPECT_NE(EvalAndGetStack(a.ctx, kWrappedIdentifier).find(":2:29"),
            std::string::npos);

  Destroy(b);
  Destroy(a);
}

}  // namespace parser_lookahead_test
