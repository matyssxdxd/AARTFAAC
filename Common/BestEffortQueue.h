#ifndef COMMON_BEST_EFFORT_QUEUE_H
#define COMMON_BEST_EFFORT_QUEUE_H

#include <Common/Threads/Queue.h>
#include <Common/Threads/Semaphore.h>

/*
 * Implements a best-effort queue. The queue has a maximum size,
 * at which point further append()s are blocked until an item
 * is removed. If `dropIfFull` is set, append() will not block,
 * but discard the item instead.
 *
 * The noMore() function signals the end-of-stream, at which point
 * an 0 or NULL element is inserted into the stream. The reader
 * thus has to consider the value 0 as end-of-stream.
 */

template<typename T> class BestEffortQueue : public Queue<T>
{
  public:
    // Create a best-effort queue with room for `maxSize' elements.
    // If `dropIfFull' is true, appends are dropped if the queue
    // has reached `maxSize'.
    BestEffortQueue(size_t maxSize, bool dropIfFull);

    // Add an element. Returns true if append succeeded, false if element
    // was dropped.
    bool append(T);

    // Remove an element -- 0 or NULL signals end-of-stream.
    T remove();

    // Signal end-of-stream.
    void noMore();

  private:
    const size_t maxSize;
    const bool dropIfFull;

    // contains the amount of free space in the queue
    Semaphore freeSpace;

    // true if the queue is being flushed
    bool flushing;
};


template <typename T> inline BestEffortQueue<T>::BestEffortQueue(size_t maxSize, bool dropIfFull)
:
  maxSize(maxSize),
  dropIfFull(dropIfFull),
  freeSpace(maxSize),
  flushing(false)
{
}


template <typename T> inline bool BestEffortQueue<T>::append(T element)
{
  bool canAppend;

  // can't append if we're emptying the queue
  if (flushing)
    return false;

  // determine whether we can append
  if (dropIfFull) {
    canAppend = freeSpace.tryDown();
  } else {
    canAppend = freeSpace.down();
  }

  // append if possible
  if (canAppend) {
    Queue<T>::append(element);
  }

  return canAppend;
}


template <typename T> inline T BestEffortQueue<T>::remove()
{
  T element = Queue<T>::remove();

  // freed up one spot
  freeSpace.up();

  return element;
}


template <typename T> inline void BestEffortQueue<T>::noMore()
{
  if (flushing)
    return;

  // mark queue as flushing
  flushing = true;

  // prevent writer from blocking
  freeSpace.noMore();

  // signal end-of-stream to reader
  Queue<T>::append(0);
}

#endif

