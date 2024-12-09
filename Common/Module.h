#if !defined MODULE_H
#define MODULE_H

#include "Common/Parset.h"

#include <cudawrappers/cu.hpp>
#include <cudawrappers/nvrtc.hpp>

#include <string>
#include <vector>


class Module : public cu::Module
{
  public:
    Module(const cu::Device &, nvrtc::Program &, const Parset &); // throw (cu::Error, nvrtc::Error)

  private:
    std::string findNVRTCincludePath() const;
    cu::Module  compileModule(const cu::Device &, nvrtc::Program &, const Parset &) const;
};

#endif
