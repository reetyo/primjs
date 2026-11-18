// Copyright 2026 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_INTERPRETER_QUICKJS_SOURCE_QUICKJS_PARSER_LOOKAHEAD_H_
#define SRC_INTERPRETER_QUICKJS_SOURCE_QUICKJS_PARSER_LOOKAHEAD_H_

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "quickjs/include/primjs_monitor.h"

namespace primjs {

inline constexpr char kFastParserLookaheadSetting[] =
    "primjs_fast_parser_lookahead";
inline constexpr uint8_t kSkipWrappedExpression = 1;
inline constexpr uint8_t kUseTokenKinds = 2;
inline constexpr uint8_t kSkipAssignmentScan = 4;
inline constexpr uint8_t kAllLookaheadOptimizations =
    kSkipWrappedExpression | kUseTokenKinds | kSkipAssignmentScan;

// Called once per runtime. Each runtime keeps its own copy so creating a
// runtime with different host settings does not affect existing ones.
inline uint8_t ReadParserLookaheadFlags() {
  return GetSettingsWithKey(kFastParserLookaheadSetting)
             ? kAllLookaheadOptimizations
             : 0;
}

inline bool IsLookaheadSpace(uint8_t c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
         c == '\v';
}

// These expression starters cannot start a valid outer arrow parameter list.
// The normal expression parser still validates and compiles their contents.
inline bool StartsWithFunctionExpression(const uint8_t* open,
                                         const uint8_t* end) {
  if (open == end || *open != '(') return false;
  const uint8_t* p = open + 1;
  while (p < end && IsLookaheadSpace(*p)) ++p;
  constexpr char keyword[] = "function";
  constexpr size_t length = sizeof(keyword) - 1;
  return static_cast<size_t>(end - p) > length &&
         std::memcmp(p, keyword, length) == 0 &&
         (IsLookaheadSpace(p[length]) || p[length] == '(' || p[length] == '*');
}

inline bool StartsWithEmptyArrowExpression(const uint8_t* open,
                                           const uint8_t* end) {
  if (open == end || *open != '(') return false;
  const uint8_t* p = open + 1;
  while (p < end && IsLookaheadSpace(*p)) ++p;
  if (p == end || *p++ != '(') return false;
  while (p < end && IsLookaheadSpace(*p)) ++p;
  if (p == end || *p++ != ')') return false;
  while (p < end && (*p == ' ' || *p == '\t' || *p == '\v' || *p == '\f')) ++p;
  return end - p >= 2 && p[0] == '=' && p[1] == '>';
}

inline bool StartsWithUnambiguousArrowWrapper(const uint8_t* open,
                                              const uint8_t* end) {
  return StartsWithFunctionExpression(open, end) ||
         StartsWithEmptyArrowExpression(open, end);
}

// The regular lexer has already classified the first byte as an ASCII
// identifier start. Leave escaped, Unicode and overlong names to it.
inline const uint8_t* ScanLookaheadIdentifier(const uint8_t* begin,
                                              const uint8_t* end,
                                              size_t limit) {
  if (begin >= end) return nullptr;
  for (const uint8_t* p = begin + 1; p < end; ++p) {
    const unsigned c = *p;
    if (((c | 0x20) - 'a') <= 'z' - 'a' || c - '0' <= 9 || c == '_' ||
        c == '$') {
      if (static_cast<size_t>(p - begin) >= limit) return nullptr;
      continue;
    }
    return c >= 0x80 || c == 0 || c == '\\' ? nullptr : p;
  }
  return end;
}

inline const uint8_t* ScanLookaheadString(const uint8_t* begin,
                                          const uint8_t* end, uint8_t quote) {
  for (const uint8_t* p = begin; p < end; ++p) {
    const uint8_t c = *p;
    if (c == quote) return p + 1;
    if (c < 0x20 || c >= 0x7f || c == '\\') return nullptr;
  }
  return nullptr;
}

// The caller has consumed an opening delimiter. A true result rules out both
// '=' and '=>' after its matching delimiter. Leave all parser state untouched
// and let the full lexer handle any syntax this scan cannot classify.
inline bool CanSkipAssignmentLookahead(const uint8_t* begin,
                                       const uint8_t* end) {
  uint8_t delimiters[64];
  size_t depth = 0;
  const uint8_t* p = begin;
  while (p < end) {
    const uint8_t c = *p++;
    switch (c) {
      case '(':
      case '[':
      case '{':
        if (depth == sizeof(delimiters)) return false;
        delimiters[depth++] = c == '(' ? ')' : c == '[' ? ']' : '}';
        break;
      case ')':
      case ']':
      case '}':
        if (depth == 0 || c != delimiters[--depth]) return false;
        if (depth != 0) break;

        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' ||
                           *p == '\r' || *p == '\f' || *p == '\v')) {
          ++p;
        }
        if (p == end) return true;
        switch (*p) {
          case ')':
          case ']':
          case '}':
          case ',':
          case ';':
          case '.':
          case '(':
          case '[':
          case '?':
          case ':':
          case '+':
          case '*':
          case '%':
          case '&':
          case '|':
          case '^':
          case '!':
            return true;
          default:
            return false;
        }
      case '\'':
      case '"': {
        bool closed = false;
        while (p < end) {
          const uint8_t ch = *p++;
          if (ch == c) {
            closed = true;
            break;
          }
          if (ch == '\\') {
            if (p == end) return false;
            switch (*p++) {
              case '\'':
              case '"':
              case '\\':
              case '/':
              case 'b':
              case 'f':
              case 'n':
              case 'r':
              case 't':
              case 'v':
                break;
              default:
                return false;
            }
          } else if (ch < 0x20 || ch >= 0x7f) {
            return false;
          }
        }
        if (!closed) return false;
        break;
      }
      case '/':   // Division, RegExp, or comment.
      case '`':   // Template literal.
      case '\\':  // Identifier escape.
      case '#':
        return false;
      case '<':
        if (end - p >= 3 && p[0] == '!' && p[1] == '-' && p[2] == '-')
          return false;
        break;
      case '-':
        if (end - p >= 2 && p[0] == '-' && p[1] == '>') return false;
        break;
      case '\t':
      case '\n':
      case '\r':
      case '\f':
      case '\v':
        break;
      default:
        if (c < 0x20 || c >= 0x7f) return false;
        break;
    }
  }
  return false;
}

}  // namespace primjs

#endif  // SRC_INTERPRETER_QUICKJS_SOURCE_QUICKJS_PARSER_LOOKAHEAD_H_
