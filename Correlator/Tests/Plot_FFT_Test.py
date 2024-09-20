#!/usr/bin/env python3
# Script to plot and compare input and output for the FFT Test
# Copyright (C) 2021 ASTRON (Netherlands Institute for Radio Astronomy)
# SPDX-License-Identifier: GPL-3.0-or-later

import time
import argparse
import numpy as np
import matplotlib.pyplot as plt
import sys
import os
from csv import reader

# Creates and returns the ArgumentParser object
def create_arg_parser():
    parser = argparse.ArgumentParser(description='Plot and compare input and output for the FFT Test')
    parser.add_argument('dataDirectory',
                   help='The directory with the (csv) data files, both input and output, expecting 2 input files and 2 output files')
    parser.add_argument('--savePlot',
                    help='If specified the plot is saved to the specified file path instead of showing the plot to screen')
    parser.add_argument('--quiet', action='store_true',
                    help='Suppress verbose output')
    return parser

def get_complex (filePath):
    # output might contain 'real'+'negative imaginary', replace '+' in that case
    return np.loadtxt(filePath, dtype=complex, converters={0: lambda s: complex(s.decode().replace('+-', '-'))})

if __name__ == "__main__":

    # Get application start times and format them
    starttime=time.localtime()
    totaltime = time.time()
    print('Starting application at {}'.format(time.strftime("%a %b %d %H:%M:%S %Y",starttime)))

    # Get input parameters
    argParser = create_arg_parser()
    parsedArgs = argParser.parse_args(sys.argv[1:])
    verbose = not(parsedArgs.quiet)

    # Set main directory
    if os.path.exists(parsedArgs.dataDirectory):
        dataDirectory = parsedArgs.dataDirectory
        # Count files in te directory
        fileListDir = os.listdir(dataDirectory)
        fileList = [i for i in fileListDir if i.endswith('.csv')]
        nfiles = len(fileList)
        if(verbose): print(fileList)
        if nfiles != 4: raise RuntimeError(f'Expecting 4 files, found {nfiles}')
    else: # raise exception
        raise RuntimeError("Directory" + parsedArgs.dataDirectory + "does not exist")

    # expecting a file list with: ['sinusFFT_FilterTest_input_x.csv', 'sinusFFT_FilterTest_input_y.csv', 'sinusFFT_FilterTest_output_x.csv', 'sinusFFT_FilterTest_output_y.csv']

    # ToDo: make more generic (nr polarizations, nr files, impacts plot grid!)
    # Input
    # x polarization
    filePath = os.path.join(dataDirectory, "sinusFFT_FilterTest_input_x.csv")
    in_x = get_complex(filePath)
    # y polarization
    filePath = os.path.join(dataDirectory, "sinusFFT_FilterTest_input_y.csv")
    in_y = get_complex(filePath)
    # Output
    # x polarization
    filePath = os.path.join(dataDirectory, "sinusFFT_FilterTest_output_x.csv")
    out_x = get_complex(filePath)
    # y polarization
    filePath = os.path.join(dataDirectory, "sinusFFT_FilterTest_output_y.csv")
    out_y = get_complex(filePath)

    t = np.linspace(1,len(in_x),num=len(in_x))

    fig, ((ax11, ax12), (ax21, ax22)) = plt.subplots(2, 2, figsize=(20, 10))

    ax11.set_title("Input POL_X")
    ax11.plot(t, in_x.real, label='real')
    ax11.plot(t, in_x.imag, label='imaginary')
    ax11.legend(loc='upper left')
    ax11.set_xlabel('Channels')

    ax12.set_title("Input POL_Y")
    ax12.plot(t, in_y.real)
    ax12.plot(t, in_y.imag)
    ax12.set_xlabel('Channels')

    ax21.set_title("Output POL_X")
    ax21.plot(t, out_x.real)
    ax21.plot(t, out_x.imag)
    ax21.set_xlabel('Frequency')

    ax22.set_title("Output POL_Y")
    ax22.plot(t, out_y.real)
    ax22.plot(t, out_y.imag)
    ax22.set_xlabel('Frequency')

    if parsedArgs.savePlot:
        plt.savefig(parsedArgs.savePlot)
        print("Saved plot to " + parsedArgs.savePlot)
    else:
        plt.show()

    #Wrap up
    totaltime = time.time()-totaltime
    print('### Ending application at {} processing took {:.2f} seconds'.format(time.strftime("%a %b %d %H:%M:%S %Y",time.localtime()),totaltime))
