#if !defined FUNCTION_H
#define FUNCTION_H

#include "Common/Module.h"

#include <string>


class Function : public cu::Function
{
  public:
    Function(const Parset &, const cu::Module &, const std::string &name);

  protected:
    const Parset &ps;
};

#endif
