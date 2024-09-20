#include "Common/Config.h"

#include "Common/Exceptions/AddressTranslator.h"
#include "Common/Exceptions/Backtrace.h"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <execinfo.h>


using namespace std;

// Initialize to true, so that backtrace printing stop at main() function.
bool Backtrace::stopAtMain = true;

Backtrace::Backtrace() :
  itsNrAddr(0)
{
  memset(itsAddr, 0, maxNrAddr*sizeof(void*));
  itsNrAddr = backtrace(itsAddr, maxNrAddr);
}

void Backtrace::print(ostream& os) const
{
  if (itsTrace.empty()) {
    try {
      AddressTranslator()(itsTrace, itsAddr, itsNrAddr);
    } catch (std::bad_alloc&) {}
  }
    
  // Save the current fmtflags
  ios::fmtflags flags(os.flags());

  os.setf(ios::left);
  for(unsigned i = 1; i < itsNrAddr; ++i) {
    if (i > 1) os << endl;
    os << "#" << setw(2) << i-1
       << " " << itsAddr[i];
    if (i < itsTrace.size()) {
      os << " in " << itsTrace[i].function
	 << " at " << itsTrace[i].file
	 << ":"    << itsTrace[i].line;
      if (stopAtMain && itsTrace[i].function == "main") break;
    }
  }

  // Restore the fmtflags
  os.flags(flags);
}


ostream& operator<<(ostream& os, const Backtrace& st)
{
  st.print(os);
  return os;
}
