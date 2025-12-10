#include "Common/Config.h"

#include "AARTFAAC/CorrelatorPipeline.h"
#include "AARTFAAC/Parset.h"
#include "AARTFAAC/SignalHandler.h"
#include "Common/Affinity.h"
#include "Common/CUDA_Support.h"

#include <list>
#include <iostream>


void printArgv(int argc, char **argv)
{
  std::clog << "command line arguments: " << argv[0];

  for (int i = 1; i < argc; i ++)
    std::clog << ' ' << argv[i];

  std::clog << std::endl;
}


void printSettings(const AARTFAAC_Parset &ps)
{
  std::clog << "#receivers = " << ps.nrStations() << std::endl;
  std::clog << "#polarizations = " << ps.nrPolarizations() << std::endl;
  std::clog << "#intermediate channels/subband = " << ps.nrChannelsPerSubband() << std::endl;
  std::clog << "#output channels/subband = " << ps.nrOutputChannelsPerSubband() << std::endl;
  std::clog << "#samples/channel = " << ps.nrSamplesPerChannel() << std::endl;
  std::clog << "#bits/sample = " << ps.nrBitsPerSample() << std::endl;
  std::clog << "correlator mode = " << ps.correlationMode() << std::endl;
  std::clog << "start time = " << ps.startTime() << std::endl;
  std::clog << "intended stop time = " << ps.stopTime() << std::endl;
  std::clog << "bandpass correction = " << ps.correctBandPass() << std::endl;
  std::clog << "delay compensation = " << ps.delayCompensation() << std::endl;
}


int main(int argc, char **argv)
{
#if !defined CREATE_BACKTRACE_ON_EXCEPTION
  try {
#endif
    cu::init();
    cu::Device device(0);
    cu::Context context(CU_CTX_SCHED_BLOCKING_SYNC, device);

#if defined __linux__
    setScheduler(SCHED_BATCH, 0);
#endif

#if 0
    std::list<SignalHandler> signalHandlers;

    for (int signo : { SIGINT, SIGTERM, SIGPIPE })
      signalHandlers.emplace_back(signo, [] (int) {
	if (write(STDERR_FILENO, "caught signal\n", 14) < 0)
	  /* ignore */;

	AARTFAAC_CorrelatorPipeline::caughtSignal();
      });
#endif

    printArgv(argc, argv);
    AARTFAAC_Parset ps(argc, argv);
    printSettings(ps);
    //BoundThread bt(ps.allowedCPUs());
    AARTFAAC_CorrelatorPipeline(ps).doWork();
#if !defined CREATE_BACKTRACE_ON_EXCEPTION
  } catch (cu::Error &error) {
    std::cerr << "caught cu::Error: " << error.what() << std::endl;
    exit(1);
  } catch (std::exception &error) {
    std::cerr << "caught std::exception: " << error.what() << std::endl;
    exit(1);
  }
#endif

  return 0;
}
