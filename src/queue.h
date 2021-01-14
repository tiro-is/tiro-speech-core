#ifndef TIRO_SPEECH_SRC_QUEUE_H_
#define TIRO_SPEECH_SRC_QUEUE_H_

#include <condition_variable>
#include <mutex>
#include <queue>

namespace tiro_speech {

/** ThreadSafeQueue
 * \brief  Simple thread safe queue with a blocking pop.
 *
 * \detail  This is useful for when we hava seperate Reader thread supplying a
 *          Processor thread with data.
 */
template <typename T>
class ThreadSafeQueue {
 public:
  ThreadSafeQueue() = default;
  ~ThreadSafeQueue() = default;
  T blocking_pop();
  void push(const T& t);
  void push(T&& t);

 private:
  std::queue<T> queue_;
  std::mutex m_;
  std::condition_variable cv_;
};

}  // namespace tiro_speech

#include "src/queue-inl.h"

#endif  // TIRO_SPEECH_SRC_QUEUE_H_
