#include "Common/Config.h"
#include "Common/Module.h"

#include <iostream>

#define GNU_SOURCE
#include <link.h>


Module::Module(const cu::Device &device, nvrtc::Program &program, const Parset &ps)
:
  cu::Module(compileModule(device, program, ps))
{
}


std::string Module::findNVRTCincludePath() const
{
  std::string path;

  if (dl_iterate_phdr([] (struct dl_phdr_info *info, size_t, void *arg) -> int
                      {
                        std::string &path = *static_cast<std::string *>(arg);
                        path = info->dlpi_name;
                        return path.find("libnvrtc.so") != std::string::npos;
                      }, &path))
  {
    path.erase(path.find_last_of("/")); // remove library name
    path.erase(path.find_last_of("/")); // remove /lib64
    path += "/include";
  }

  return path;
}


cu::Module Module::compileModule(const cu::Device &device, nvrtc::Program &program, const Parset &ps) const
{
  int capability = 10 * device.getAttribute<CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR>() + device.getAttribute<CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR>();

  std::vector<std::string> options =
  {
    "-I" + findNVRTCincludePath(),
    "-Invidia-mathdx-22.11.0-Linux/nvidia/mathdx/22.11/include",
    "-I/usr/include/aarch64-linux-gnu",
    "-default-device", // necessary to compile cuFFTDx through NVRTC
    "-std=c++17", // required by cuFFTDx
    "-arch=compute_" + std::to_string(capability),
    "-lineinfo",
  };

  if (device.getAttribute(CU_DEVICE_ATTRIBUTE_INTEGRATED))
    options.push_back("-DHAS_INTEGRATED_MEMORY");

  std::vector<std::string> parsetOptions = ps.compileOptions();

  options.insert(options.end(), parsetOptions.begin(), parsetOptions.end());

  //std::for_each(options.begin(), options.end(), [] (const std::string &e) { std::cout << e << ' '; }); std::cout << std::endl;

  try {
    program.compile(options);
  } catch (nvrtc::Error &error) {
    std::cerr << program.getLog();
    throw;
  }

#if 0
  std::fstream ptx("out.ptx", std::ios::out);
  ptx << program.getPTX();
#endif

  return cu::Module((void *) program.getPTX().data());
}
