#ifndef TIRO_SPEECH_SRC_UTILS_INL_H_
#define TIRO_SPEECH_SRC_UTILS_INL_H_

#include <cassert>
#include <chrono>
#include <ctime>

namespace tiro_speech {

template <typename Clock>
std::tm to_tm(const std::chrono::time_point<Clock>& time_point) {
  using namespace std::chrono;
  std::time_t seconds_since_epoch =
      duration_cast<seconds>(time_point.time_since_epoch()).count();
  std::tm* t = std::gmtime(&seconds_since_epoch);
  assert(t != nullptr);  // gmtime should never fail in these conditions.
  return {*t};
}

}  // end namespace tiro_speech

#endif  // TIRO_SPEECH_SRC_UTILS_INL_H_
