#if !defined MODULE_H
#define MODULE_H

#include "Common/Parset.h"

#include </var/scratch/jsteinbe/tensor-core-correlator/build/_deps/cudawrappers-src/include/cudawrappers/cu.hpp>
#include </var/scratch/jsteinbe/tensor-core-correlator/build/_deps/cudawrappers-src/include/cudawrappers/nvrtc.hpp>

#include <string>
#include <vector>


class Module : public cu::Module
{
  public:
    Module(nvrtc::Program &, const Parset &); // throw (cu::Error, nvrtc::Error)

  private:
    std::string findNVRTCincludePath() const;
    cu::Module  compileModule(nvrtc::Program &, const Parset &) const;
};

#endif
