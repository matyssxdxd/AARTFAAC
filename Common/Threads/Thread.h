#ifndef THREAD_H
#define THREAD_H

#include <cxxabi.h>
#include <pthread.h>
#include <signal.h>

#include "Common/SystemCallException.h"
#include "Common/Threads/Semaphore.h"
#include "Common/Threads/Mutex.h"
//#include "Common/Threads/Cancellation.h"


class Thread
{
  public:
    // Spawn a method by creating a Thread object as follows:
    //
    // class C {
    //   public:
    //     C() : thread(this, &C::method) {}
    //   private:
    //     void method() { std::cout << "runs asynchronously" << std::endl; }
    //     Thread thread;
    // };
    //
    // The thread is joined in the destructor of the Thread object
      
    template <typename T> Thread(T *object, void (T::*method)(), size_t stackSize = 0);
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    // ~Thread() is NOT virtual, because Thread should NOT be inherited. An
    // DerivedThread class would partially destruct itself before reaching
    // the pthread_join in ~Thread(), which in turn would result in a running
    // thread controlled by a partially deconstructed Thread object.
			  ~Thread() noexcept(false); // join a thread

    void		  cancel();

    void		  wait();

  private:
    template <typename T> struct Args {
      Args(T *object, void (T::*method)(), Thread *thread) : object(object), method(method), thread(thread) {}

      T	     *object;
      void   (T::*method)();
      Thread *thread;
    };

    template <typename T> void	      stub(Args<T> *);
    template <typename T> static void *stub(void *);

    Semaphore	       finished;
    pthread_t	       thread;
    std::exception_ptr exception_ptr;
};


template <typename T> inline Thread::Thread(T *object, void (T::*method)(), size_t stackSize)
{
  int retval;

  if (stackSize != 0) {
    pthread_attr_t attr;

    if ((retval = pthread_attr_init(&attr)) != 0)
      throw SystemCallException("pthread_attr_init", retval);

    if ((retval = pthread_attr_setstacksize(&attr, stackSize)) != 0)
      throw SystemCallException("pthread_attr_setstacksize", retval);

    if ((retval = pthread_create(&thread, &attr, &Thread::stub<T>, new Args<T>(object, method, this))) != 0)
      throw SystemCallException("pthread_create", retval);

    if ((retval = pthread_attr_destroy(&attr)) != 0)
      throw SystemCallException("pthread_attr_destroy", retval);
  } else {
    if ((retval = pthread_create(&thread, 0, &Thread::stub<T>, new Args<T>(object, method, this))) != 0)
      throw SystemCallException("pthread_create", retval);
  }
}


inline Thread::~Thread() noexcept(false)
{
  //ScopedDelayCancellation dc; // pthread_join is a cancellation point

  int retval;

  if ((retval = pthread_join(thread, 0)) != 0 && !std::uncaught_exception())
    throw SystemCallException("pthread_join", retval);

  if (exception_ptr != 0 && !std::uncaught_exception())
    std::rethrow_exception(exception_ptr);
}


inline void Thread::cancel()
{
  (void) pthread_cancel(thread); // could return ESRCH ==> ignore
}


inline void Thread::wait()
{
  finished.down();
  finished.up(); // allow multiple waits
}


template <typename T> inline void Thread::stub(Args<T> *args)
{
  // (un)register WITHIN the thread, since the thread id
  // can be reused once the thread finishes.
  //Cancellation::ScopedRegisterThread rt;

  try {
    (args->object->*args->method)();
  } catch (abi::__forced_unwind &) {
    finished.up();
    throw;
  } catch (...) {
    args->thread->exception_ptr = std::current_exception(); // let parent thread handle the exception
  }

  finished.up();
}

template <typename T> inline void *Thread::stub(void *arg)
{
  std::auto_ptr<Args<T> > args(static_cast<Args<T> *>(arg));
  args->thread->stub(args.get());
  return 0;
}

#endif
