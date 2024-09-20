#ifndef COMMON_SMART_PTR_H
#define COMMON_SMART_PTR_H

#include <cstdlib>


template <typename T> class SmartPtrDelete;

// T is the type of pointer (such as SmartPtr<int> to emulate int*)
// D is the deletion strategy (to choose between delete/delete[]/free)
template <typename T, class D = SmartPtrDelete<T> > class SmartPtr
{
  public:
    SmartPtr(T * = NULL);
    SmartPtr(const SmartPtr<T,D> &orig); // WARNING: move semantics; orig no longer contains pointer

    ~SmartPtr();

    operator T * () const;
    T & operator * () const;
    T * operator -> () const;

    bool operator ! () const;

    SmartPtr<T,D> & operator = (T *);
    SmartPtr<T,D> & operator = (const SmartPtr<T,D> &);

    T *get();
    T *release();

  private:
    T *ptr;
};

// Deletion strategies
template <typename T> class SmartPtrDelete {
public:
  static void free( T *ptr ) { delete ptr; }
};

template <typename T> class SmartPtrDeleteArray {
public:
  static void free( T *ptr ) { delete[] ptr; }
};

template <typename T> class SmartPtrFree {
public:
  static void free( T *ptr ) { ::free(ptr); }
};

template <typename T, void (*F)(T*) > class SmartPtrFreeFunc {
public:
  static void free( T *ptr ) { F(ptr); }
};

template <typename T, class D> inline SmartPtr<T,D>::SmartPtr(T *orig)
:
  ptr(orig)
{
}

template <typename T, class D> inline SmartPtr<T,D>::SmartPtr(const SmartPtr<T,D> &orig)
:
  ptr(orig.ptr)
{
  const_cast<T *&>(orig.ptr) = 0;
}


template <typename T, class D> inline SmartPtr<T,D>::~SmartPtr()
{
  D::free(ptr);
}


template <typename T, class D> inline SmartPtr<T,D>::operator T * () const
{
  return ptr;
}


template <typename T, class D> inline T &SmartPtr<T,D>::operator * () const
{
  return *ptr;
}


template <typename T, class D> inline T *SmartPtr<T,D>::operator -> () const
{
  return ptr;
}


template <typename T, class D> inline bool SmartPtr<T,D>::operator ! () const
{
  return ptr == 0;
}


template <typename T, class D> inline SmartPtr<T,D> &SmartPtr<T,D>::operator = (T *orig)
{
  D::free(ptr);
  ptr = orig;
  return *this;
}


template <typename T, class D> inline SmartPtr<T,D> &SmartPtr<T,D>::operator = (const SmartPtr<T,D> &orig)
{
  D::free(ptr);
  ptr = orig;
  const_cast<T *&>(orig.ptr) = 0;
  return *this;
}


template <typename T, class D> inline T *SmartPtr<T,D>::get()
{
  return ptr;
}


template <typename T, class D> inline T *SmartPtr<T,D>::release()
{
  T *tmp = ptr;
  ptr = 0;
  return tmp;
}

#endif
