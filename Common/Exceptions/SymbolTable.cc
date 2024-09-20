#include "Common/Config.h"

#include "SymbolTable.h"
//#include "Cancellation.h"

#include <cstdlib>

#ifdef HAVE_BFD

SymbolTable::SymbolTable() :
  itsBfd(0),
  itsSymbols(0)
{
}


SymbolTable::SymbolTable(const char* filename) :
  itsBfd(0),
  itsSymbols(0)
{
  init(filename) && read();
}


SymbolTable::~SymbolTable()
{
#if 0
  ScopedDelayCancellation dc; // bfd_close might contain a cancellation point
#endif

  cleanup();
}


bool SymbolTable::init(const char* filename)
{
  bfd_init();
  if ((itsBfd = bfd_openr(filename,0)) == 0) {
    bfd_perror(filename);
    return false;
  }
  if (!bfd_check_format(itsBfd, bfd_object)) {
    bfd_perror(filename);
    return false;
  }
  return true;
}


bool SymbolTable::read()
{
  if ((bfd_get_file_flags(itsBfd) & HAS_SYMS) == 0) {
    bfd_perror(itsBfd->filename);
    return true;
  }
  unsigned int size;
  long symcount;

  // circumvent strict-aliasing rules by casting through a union
  union {
    asymbol ***src;
    void **dest;
  } symbolUnion = { &itsSymbols };

  symcount = bfd_read_minisymbols(itsBfd, false, symbolUnion.dest, &size);
  if (symcount == 0) {
    symcount = bfd_read_minisymbols(itsBfd, true, symbolUnion.dest, &size);
  }
  if (symcount < 0) {
    bfd_perror(itsBfd->filename);
    return false;
  }
  return true;
}


void SymbolTable::cleanup()
{
  if (itsSymbols) {
    free(itsSymbols);
    itsSymbols = 0;
  }
  if (itsBfd) {
    bfd_close(itsBfd);
    itsBfd = 0;
  }
}

#endif /* HAVE_BFD */
