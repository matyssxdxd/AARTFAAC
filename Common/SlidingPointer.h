#ifndef COMMON_SLIDING_POINTER_H
#define COMMON_SLIDING_POINTER_H

#include <condition_variable>
#include <mutex>


template <typename T> class SlidingPointer
{
  public:
    SlidingPointer();
    SlidingPointer(const T &);
    SlidingPointer(const SlidingPointer<T> &);

    void advanceTo(const T &);
    void waitFor(const T &);

  //private:
    T			    currentValue;
    std::mutex		    mutex;
    std::condition_variable valueUpdated;
};


template <typename T> inline SlidingPointer<T>::SlidingPointer()
{
}


template <typename T> inline SlidingPointer<T>::SlidingPointer(const T &value)
:
  currentValue(value)
{
}


template <typename T> inline SlidingPointer<T>::SlidingPointer(const SlidingPointer<T> &other)
:
  currentValue(other.currentValue)
{
}


template <typename T> inline void SlidingPointer<T>::advanceTo(const T &value)
{
  std::lock_guard<std::mutex> lock(mutex);

  if (currentValue < value) {
    currentValue = value;
    valueUpdated.notify_all();
  }
}


template <typename T> inline void SlidingPointer<T>::waitFor(const T &value)
{
  std::unique_lock<std::mutex> lock(mutex);
  valueUpdated.wait(lock, [this, value] { return currentValue >= value; });
}

#endif
