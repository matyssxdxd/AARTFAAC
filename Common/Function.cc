#include "Common/Function.h"


Function::Function(const Parset &ps, const cu::Module &module, const std::string &name)
:
  cu::Function(module, name.c_str()),
  ps(ps)
{
}
