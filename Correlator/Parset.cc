#include "Correlator/Parset.h"

#include <boost/program_options.hpp>

#include <fstream>

CorrelatorParset::CorrelatorParset(int argc, char **argv, bool throwExceptionOnUnmatchedParameter)
:
  Parset(argc, argv, false)
{
  using namespace boost::program_options;

  std::string configFile;

  options_description allowed_options;

  allowed_options.add_options()
    ("nrOutputChannelsPerSubband,C", value<unsigned>(&_nrOutputChannelsPerSubband)->default_value(0))
    ("correlationMode,m", value<unsigned>(&_correlationMode)->default_value(0xF))
    ("configFile,configFile", value<std::string>()->notifier([&configFile] (const std::string &arg) { configFile = arg; } ))
  ;

  variables_map vm;
  parsed_options parsed = command_line_parser(toPassFurther).options(allowed_options).allow_unregistered().run();
  toPassFurther = collect_unrecognized(parsed.options, include_positional);
  store(parsed, vm);
  notify(vm);
  
  if (configFile.empty()) {
    throw Error("Configuration file is required but not provided");
  }
  
  std::ifstream config(configFile, std::ios::binary);
  if (!config.is_open()) {
    throw Error("Could not open configuration file: " + configFile);
  }

  for (int station = 0; station < nrStations(); ++station) {
    uint32_t n;
    config.read(reinterpret_cast<char*>(&n), sizeof(uint32_t));

    std::map<int64_t, double> stationDelays;

    for (uint32_t i = 0; i < n; ++i) {
      int64_t ts;
      double delay;

      config.read(reinterpret_cast<char*>(&ts), sizeof(int64_t));
      config.read(reinterpret_cast<char*>(&delay), sizeof(double));

      stationDelays[ts] = delay;
    }
  
    _delays.push_back(std::move(stationDelays));
  }
 
 
  uint32_t num_frequencies;

  config.read(reinterpret_cast<char*>(&num_frequencies), sizeof(uint32_t));
  if (config.gcount() != sizeof(uint32_t)) {
    throw Error("Failed to read num_frequencies from configuration file");
  }

  _centerFrequencies = std::vector<double>(num_frequencies, 0);
  config.read(reinterpret_cast<char*>(_centerFrequencies.data()), num_frequencies * sizeof(double));
  if (config.gcount() != num_frequencies * sizeof(double)) {
    throw Error("Failed to read center frequencies data from configuration file");
  }

  uint32_t mapping_len;
  
  config.read(reinterpret_cast<char*>(&mapping_len), sizeof(uint32_t));
  if (config.gcount() != sizeof(uint32_t)) {
    throw Error("Failed to read mapping len from config file");
  }

  _channelMapping = std::vector<uint32_t>(mapping_len, 0);
  config.read(reinterpret_cast<char*>(_channelMapping.data()), mapping_len * sizeof(uint32_t));
  if (config.gcount() != mapping_len * sizeof(uint32_t)) {
    throw Error("Failed to read channel mapping data from configuration file");
  }

  config.close();

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
