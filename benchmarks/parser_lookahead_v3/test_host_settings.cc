#include <cstdlib>
#include <cstring>

bool GetSettingsWithKey(const char* key) {
  const char* enabled = std::getenv("PRIMJS_REVIEW_LOOKAHEAD");
  return enabled && enabled[0] == '1' &&
         std::strcmp(key, "primjs_fast_parser_lookahead") == 0;
}
