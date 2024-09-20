#ifndef COMMON_THREADS_QUEUE_H
#define COMMON_THREADS_QUEUE_H

#include <condition_variable>
#include <list>
#include <mutex>


template <typename T> class Queue
{
  public:
    Queue() {}
    Queue(const Queue &) = delete;
    Queue& operator = (const Queue &) = delete;

    void     append(T &);
    T	     remove();

    unsigned size() const;
    bool     empty() const;

  private:
    mutable std::mutex	    mutex;
    std::condition_variable newElementAppended;
    std::list<T>	    queue;
};


template <typename T> inline void Queue<T>::append(T &element)
{
  std::lock_guard<std::mutex> lock(mutex);
  queue.push_back(std::move(element));
  newElementAppended.notify_one();
}


template <typename T> inline T Queue<T>::remove()
{
  std::unique_lock<std::mutex> lock(mutex);

  while (queue.empty())
    newElementAppended.wait(lock);

  T element(std::move(queue.front()));
  queue.pop_front();

  return element;
}


template <typename T> inline unsigned Queue<T>::size() const
{
  std::lock_guard<std::mutex> lock(mutex);
  return queue.size();
}


template <typename T> inline bool Queue<T>::empty() const
{
  return size() == 0;
}

#endif
