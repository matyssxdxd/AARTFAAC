#ifndef PRINT_VECTOR_H
#define PRINT_VECTOR_H

#include <iostream>
#include <vector>

template<typename T> inline std::ostream &operator << (std::ostream &str, const std::vector<T> &v)
{
  str << '[';

  for (typename std::vector<T>::const_iterator it = v.begin(); it != v.end(); it ++) {
    if (it != v.begin())
      str << ',';

    str << *it;
  }
  
  return str << ']';
}


template<typename T, typename U> inline std::ostream &operator << (std::ostream &str, const std::pair<T,U> &p)
{
  return str << '(' << p.first << ',' << p.second << ')';
}

#endif
