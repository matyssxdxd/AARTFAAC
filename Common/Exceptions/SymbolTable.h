#ifndef SYMBOLTABLE_H
#define SYMBOLTABLE_H

#include "Common/Config.h"

//# Includes
#ifndef HAVE_BFD
# warning Binary file descriptor (bfd) package is missing, \
please install the GNU binutils development package.
#else
# include <bfd.h>

// \addtogroup Common
// @{

// Class holding the symbol table of an object file. 
//
// \note The code is based on the utility addr2line.c in GNU binutils 2.16.
class SymbolTable {

public:
  // Default constructor. Doesn't read any symbol table into memory.
  SymbolTable();

  // Read the symbol table from \a filename into memory.
  SymbolTable(const char* filename);

  // Destructor.
  ~SymbolTable();

  // Return a pointer to the symbol table.
  asymbol** getSyms() const
  { return itsSymbols; }

  // Return a pointer to the bfd.
  bfd* getBfd() const
  { return itsBfd; }

private:
  // Disallow copy constructor.
  SymbolTable(const SymbolTable&);

  // Disallow assignment operator.
  SymbolTable& operator=(const SymbolTable&);

  // Open the object file \c filename and check its format.
  // @return True on success.
  bool init(const char* filename);

  // Read the symbol table from the object file.
  // @return True when read was successful.
  bool read();

  // Free dynamically allocated memory; close object file.
  void cleanup();

  // Pointer to the binary file descriptor.
  bfd* itsBfd;

  // Pointer to the internal representation of the symbol table.
  asymbol** itsSymbols;
};

#endif /* HAVE_BFD */

#endif
