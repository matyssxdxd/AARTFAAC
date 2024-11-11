#include "Common/Config.h"

#include "ISBI/CorrelatorPipeline.h"
#include "ISBI/Parset.h"
#include "ISBI/InputSection.h"
//#include "ISBI/SignalHandler.h"
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


void printSettings(const ISBI_Parset &ps){
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


int main(int argc, char **argv){
    
    #if !defined CREATE_BACKTRACE_ON_EXCEPTION
    try {
    #endif
    cu::init();
    cu::Device device(0);
    cu::Context context(CU_CTX_SCHED_BLOCKING_SYNC, device);

    #if defined __linux__
    setScheduler(SCHED_BATCH, 0);
    #endif
    
    printArgv(argc, argv);
    ISBI_Parset ps(argc, argv);
    printSettings(ps);
   
   ISBI_CorrelatorPipeline correlatorPipeline (ps);
   //InputSection inputSection(ps);
   //inputSection.startReadTransaction(ps.startTime());
   
    //correlatorPipeline.startReadTransaction(ps.startTime());
  correlatorPipeline.doWork();
    ///std::clog<<"size is " << sizeof (ISBI_CorrelatorPipeline) << std::endl;
      
    #if !defined CREATE_BACKTRACE_ON_EXCEPTION
  } catch (cu::Error &error) {
    std::cerr << "caught cu::Error: " << error.what() << std::endl;
    exit(1);
  } catch (std::exception &error) {
    std::cerr << "caught std::exception: " << error.what() << std::endl;
    exit(1);
  }
#endif
    
    
    
    //AARTFAAC/AARTFAAC -p1 -n288 -t768 -c256 -C17 -b16 -s8 -m15 -D '2024-03-17 03:36:46' -r4 -g0 -q1 -R0 -i /mnt/VLBI/data/b004/b004_ib_no0001.m5a,/mnt/VLBI/data/b004/b004_ir_no0001.m5a -o null:
    
    //inputSection;    
   //ISBI/ISBI -p1 -n288 -t768 -c256 -C17 -b16 -s8 -m15 -D '2024-03-17 03:36:46' -r4 -g0 -q1 -R0 -i /mnt/VLBI/data/b004/b004_ib_no0001.m5a,/mnt/VLBI/data/b004/b004_ir_no0001.m5a -o null:


//    TZ=UTC ISBI/ISBI -p1 -n2 -t768 -c256 -C16 -b16 -s8 -m15 -D '2024-02-20 04:35:19' -r180 -g0 -q1 -R0 -i /var/scratch/jsteinbe/data/aa1_ib_no0002.m5a,/var/scratch/jsteinbe/data/aa1_ir_no0002.m5a -o /var/scratch/mpurvins/a1.out,/var/scratch/mpurvins/a2.out,/var/scratch/mpurvins/a3.out,/var/scratch/mpurvins/a4.out,/var/scratch/mpurvins/a5.out,/var/scratch/mpurvins/a6.out,/var/scratch/mpurvins/a7.out,/var/scratch/mpurvins/a8.out
  return 0;
}

