#include "Correlator/Parset.h"

#include <boost/program_options.hpp>

#include <fstream>

CorrelatorParset::CorrelatorParset(int argc, char **argv, bool throwExceptionOnUnmatchedParameter)
:
  Parset(argc, argv, false)
{
  using namespace boost::program_options;

  std::string delayPath;

  options_description allowed_options;

  allowed_options.add_options()
    ("nrOutputChannelsPerSubband,C", value<unsigned>(&_nrOutputChannelsPerSubband)->default_value(0))
    ("correlationMode,m", value<unsigned>(&_correlationMode)->default_value(0xF))
    ("delayPath,K", value<std::string>()->notifier([&delayPath] (const std::string &arg) { delayPath = arg; } ))
  ;

  variables_map vm;
  parsed_options parsed = command_line_parser(toPassFurther).options(allowed_options).allow_unregistered().run();
  toPassFurther = collect_unrecognized(parsed.options, include_positional);
  store(parsed, vm);
  notify(vm);
  
  std::ifstream delayFile(delayPath, std::ios::binary);
 
  uint32_t num_rows, num_cols;
  delayFile.read(reinterpret_cast<char*>(&num_rows), sizeof(uint32_t));
  delayFile.read(reinterpret_cast<char*>(&num_cols), sizeof(uint32_t));
 
  _trueDelays = std::vector<int>(num_rows * num_cols);
  _fracDelays = std::vector<double>(num_rows * num_cols);
 
  delayFile.read(reinterpret_cast<char*>(_trueDelays.data()), num_cols * num_rows * sizeof(int));
 
  delayFile.read(reinterpret_cast<char*>(_fracDelays.data()), num_cols * num_rows * sizeof(double));
 
 
  uint32_t num_frequencies;
  delayFile.read(reinterpret_cast<char*>(&num_frequencies), sizeof(uint32_t));
 
  _centerFrequencies = std::vector<double>(num_frequencies, 0);
 
  delayFile.read(reinterpret_cast<char*>(_centerFrequencies.data()), num_frequencies * sizeof(double));

  if (throwExceptionOnUnmatchedParameter && toPassFurther.size() > 0)
    throw Error(std::string("unrecognized argument \'") + toPassFurther[0] + '\'');


  if (_nrOutputChannelsPerSubband == 0)
    _nrOutputChannelsPerSubband = std::max(_nrChannelsPerSubband - 1, 1U);

  if (_nrOutputChannelsPerSubband > _nrChannelsPerSubband)
    throw Error("#output channels cannot exceed #internal channels");

  switch (_nrPolarizations) {
    case 1: _nrVisibilityPolarizations = 1;
	    break;

    case 2: switch (_correlationMode) {
	      case 0x1:
	      case 0x8: _nrVisibilityPolarizations = 1;
			break;

	      case 0x9: _nrVisibilityPolarizations = 2;
			break;

	      case 0xF: _nrVisibilityPolarizations = 4;
			break;

	      default:  throw Error("unsupported correlation mode");
	    }

	    break;

    default: throw Error("unsupported #polarizations");
  }
}


std::vector<std::string> CorrelatorParset::compileOptions() const
{
  std::vector<std::string> options =
  {
    "-DNR_OUTPUT_CHANNELS_PER_SUBBAND=" + std::to_string(nrOutputChannelsPerSubband()),
  };

  std::vector<std::string> parentOptions = Parset::compileOptions();
  options.insert(options.end(), parentOptions.begin(), parentOptions.end());
  return options;
}
