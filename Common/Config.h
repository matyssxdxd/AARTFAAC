#if !defined CONFIG_H
#define CONFIG_H

#if defined __x86_64__
//#define HAVE_BFD
#endif
#undef TEXAS_INSTRUMENTS
#define BOOST_DISABLE_ASSERTS
#undef USE_FUSED_KERNEL
#define USE_NEW_CORRELATOR
//#define USE_PHI_CORRELATOR
#define USE_2X2
#undef MEASURE_POWER

#define NR_TAPS			16
#define NR_STATION_FILTER_TAPS	16
#define HAVE_FFTW3
#undef CREATE_BACKTRACE_ON_EXCEPTION

#define USE_FFT_LIBRARY		FFT_LIBRARY_APPLE
#define FFT_LIBRARY_APPLE	1
#define FFT_LIBRARY_AMD		2


// Constants
#define COMPLEX			2
#define REAL			0
#define IMAG			1
#define POL_X			0
#define POL_Y			1

#endif
