#include <cstdlib>

int GetSettingsFlag() {
  const char* enabled = std::getenv("PRIMJS_REVIEW_LOOKAHEAD");
  return enabled && enabled[0] == '1' ? (1 << 22) : 0;
}
