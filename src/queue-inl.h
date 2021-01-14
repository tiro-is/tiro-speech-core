#ifndef TIRO_SPEECH_SRC_QUEUE_INL_H_
#define TIRO_SPEECH_SRC_QUEUE_INL_H_

#include <cassert>
#include <memory>
#include <utility>

namespace tiro_speech {

template <typename T>
T ThreadSafeQueue<T>::blocking_pop() {
  std::unique_lock<std::mutex> lk{m_};
  cv_.wait(lk, [this] { return !queue_.empty(); });
  T ret = std::move(queue_.front());
  queue_.pop();
  return ret;
}

template <typename T>
void ThreadSafeQueue<T>::push(const T& t) {
  {
    std::lock_guard<std::mutex> lock{m_};
    queue_.push(t);
  }
  cv_.notify_one();
}

template <typename T>
void ThreadSafeQueue<T>::push(T&& t) {
  {
    std::lock_guard<std::mutex> lock{m_};
    queue_.push(std::forward<T>(t));
  }
  cv_.notify_one();
}

}  // end namespace tiro_speech

#endif  // TIRO_SPEECH_SRC_QUEUE_INL_H_
