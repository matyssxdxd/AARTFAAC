#include "Common/Config.h"

#include "Common/Exceptions/Exception.h"

#include <cstdlib>    // for abort()
#include <iostream>

#ifndef __GNUG__
# include <cstdio>    // for fputs()
# include <typeinfo>  // for typeid()
#endif

using namespace std;

//## ----   P u b l i c   m e t h o d s   ---- ##//

void Exception::terminate()
{
  // We need to safe-guard against recursive calls. E.g., we were called
  // twice, because a rethrow was attempted without an active exception.
  static bool terminating = false;
  if (terminating) {
#ifdef __GNUG__
    __gnu_cxx::__verbose_terminate_handler();
#else
    // fputs() is the only safe way to print to stderr when low on memory
    fputs("terminate called recursively\n", stderr);
    abort();
#endif
  }
  terminating = true;
  
  // Make sure there was an exception by trying to rethrow it. If that fails,
  // terminate will be called again.
  try {
    throw;
  } 
  catch (...) {
    cerr << "\n**** uncaught exception ****\n" << endl;
    // Rethrow, so we can separate LOFAR::Exception from other exceptions.
    try {
      throw;
    }
    // LOFAR::Exception already carries backtrace info, if available.
    catch (Exception& e) {
      try {
	cerr << e << endl;
      } catch (...) {}
    }
    // For all other exceptions, generate a backtrace first, if we can.
    // Use the GNU's verbose terminate handler, if available. Otherwise,
    // try to do the dirty work ourselves.
    catch (...) {
#ifdef HAVE_BACKTRACE
      cerr << "Backtrace follows:" << endl;
      try {
	cerr << Backtrace() << endl;
      } catch (...) {}
#endif
#ifdef __GNUG__
      __gnu_cxx::__verbose_terminate_handler();
#else
      // Rethrow once more to separate std::exception from other exceptions.
      try {
	throw;
      } 
      // Print the (mangled) type of the exception and its content.
      catch (exception& e) {
	try {
	  cerr << typeid(e).name() << ": " << e.what() << endl;
	} catch (...) {}
      }
#endif
    }
  }
  abort();
}


void Exception::print(ostream& os) const
{
  os << "[" << type() << ": " << itsText << "]" << endl
     << "in function " << (itsFunction.empty() ? "??" : itsFunction) << endl
     << "(" << (itsFile.empty() ? "??" : itsFile) << ":" << itsLine << ")";
#ifdef HAVE_BACKTRACE
  if (itsBacktrace) {
    os << "\nBacktrace follows:\n" << *itsBacktrace;
  }
#endif
}


const string Exception::message() const
{
  ostringstream oss;
  print(oss);
  return oss.str();
}


//## ----   G l o b a l   m e t h o d s   ---- ##//

ostream& operator<<(ostream& os, const Exception& ex)
{
  ex.print(os);
  return os;
}

