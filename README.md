# General
`git clone git@git.astron.nl:RD/AARTFAAC.git`

## Pre-requisites
On DAS-5 requires module files (`load module <>`): `gcc/8.3.0`, `cuda110/toolkit/11.0.1`, `boost/1.73-gcc-8.3.0` and `fftw/3.3.8-gcc-8.3.0`

For usage with Nvidia GPUs, use the CUDA OpenCL library instead of rocm, current as work around: `export OPENCL_LIB=$CUDA_LIB`

# Correlator

## Installation
```
cd AARTFAAC
make -j
```

## Usage
A test program for the Correlator pipeline `./Correlator/Correlator`

Unit tests in `/Correlator/Tests` :
* `CorrelatorTest`;
Uses 288 stations by default and therefore does not run subtest `CorrelateRectangleTest` (only used when nr of stations is not a multiple of 32)
* `DelayAndBandPassTest`
* `DeviceInstanceTest`
* `Filter_FFT_Test`
* `FIR_FilterTest`

# AARTFAAC

## Installation
```
cd AARTFAAC
mkdir AARTFAAC/installed
make install
```

## Usage
A test program `./AARTFAAC/installed/AARTFAAC`, currently fails (not intended for single node?)