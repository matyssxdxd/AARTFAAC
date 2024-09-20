#ifndef BACKTRACE_H
#define BACKTRACE_H

#define HAVE_BACKTRACE

// \file
// Store stack frame return addresses for self-debugging.

#ifndef HAVE_BACKTRACE
# error Backtrace is not supported.
#endif

// Maximum number of stack frame return
// addresses that will be stored by the Backtrace class (default=50).
#ifndef BACKTRACE_MAX_RETURN_ADDRESSES
# define BACKTRACE_MAX_RETURN_ADDRESSES 50
#endif

//# Includes
#include <iosfwd>
#include <string>
#include <vector>

//# Forward declarations
class SymbolTable;

// \ingroup Common
// @{

// Store stack frame return addresses for self-debugging and provide a way
// to print them in a human readable form.
class Backtrace {
public:
  // Layout of a trace line
  struct TraceLine {
    TraceLine() : function("??"), file("??"), line(0) {}
    std::string function;
    std::string file;
    unsigned line;
  };
  // Constructor. Calls backtrace() to fill \c itsAddr with the return
  // addresses of the current program state.
  Backtrace();

  // Print the current backtrace in human readable form into the output
  // stream \a os.
  void print(std::ostream& os) const;

  // Indicates whether we should stop printing a backtrace when we reach
  // main(), or not.
  static bool stopAtMain;

private:
  // Maximum number of return addresses that we are willing to handle. Add
  // one to the compile time constant, because, later on, we will hide the
  // first return address (being the construction of Backtrace itself).
  static const int maxNrAddr = BACKTRACE_MAX_RETURN_ADDRESSES + 1;

  // C-array of return addresses.
  void* itsAddr[maxNrAddr];

  // Actual number of return addresses returned by backtrace().
  unsigned itsNrAddr;

  // Traceback info containing function name, filename, and line number.
  // This vector will be filled by AddressTranslator.operator().
  mutable std::vector<TraceLine> itsTrace;
};

// Print the backtrace \a st to the output stream \a os.
std::ostream& operator<<(std::ostream& os, const Backtrace& st);

// @}

#endif
