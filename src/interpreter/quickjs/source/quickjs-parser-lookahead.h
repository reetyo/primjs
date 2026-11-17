// Copyright 2026 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_INTERPRETER_QUICKJS_SOURCE_QUICKJS_PARSER_LOOKAHEAD_H_
#define SRC_INTERPRETER_QUICKJS_SOURCE_QUICKJS_PARSER_LOOKAHEAD_H_

#include <cstddef>
#include <cstdint>

namespace primjs {

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
