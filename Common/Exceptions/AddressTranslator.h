#ifndef ADDRESSTRANSLATOR_H
#define ADDRESSTRANSLATOR_H

// \file
// Translate return addresses to function, file, line.

//# Includes
#include "Common/Config.h"
#include "Backtrace.h"

#ifndef HAVE_BFD
# warning Binary file descriptor (bfd) package is missing, \
please install the GNU binutils development package.
#else
# include <bfd.h>
# include <link.h>
#endif

#include <mutex>

// \ingroup Common
// @{

// Functor class for translating return addresses to function name,
// filename, and line number. 
//
// \note The code is based on the utility addr2line.c in the GNU binutils
// 2.16.
// \note For details on handling of shared libraries, please refer to
// - the Linux man page \c dl_iterate_phdr(3)
// - \c util/backtrace-symbols.c in cairo (http://cairographics.org/)
// - \c src/dwarf/Gfind_proc_info-lsb.c:callback() in libunwind
//   (http://www.nongnu.org/libunwind/)

class AddressTranslator {
public:
  AddressTranslator();
  ~AddressTranslator();

  // Translate the \a size addresses specified in \a addr to \a size
  // trace lines, containing function name, filename, and line number, iff
  // that information can be retrieved from the program's symbol table.
  // \param[out] trace vector containing the trace lines
  // \param[in]  addr C-array of return addresses
  // \param[in]  size number of return addresses in \a addr
  void operator()(std::vector<Backtrace::TraceLine>& trace, 
		  void* const* addr, int size);

private:
#ifdef HAVE_BFD
  // Used to guard access to the BFD routines, which are not thread-safe.
  static std::mutex mutex;

  // Helper function to pass the member function #do_find_matching_file() as
  // argument to dl_iterate_phdr().
  // \see The Linux man page <tt>dl_iterate_phdr(3)</tt>.
  static int find_matching_file(dl_phdr_info*, size_t, void*);

  // Look for the address #pc in the application's shared objects. If found,
  // set the member variables #bfdFile and #base_addr.
  int do_find_matching_file(dl_phdr_info*);

  // Helper function to pass the member function
  // #do_find_address_in_section() as argument to bfd_map_over_sections().
  // \see BFD documentation for details (<tt>info bfd</tt>).
  static void find_address_in_section(bfd*, asection*, void*);

  // Look for the address #pc in the section \a section of the symbol table
  // associated with the BFD \a abfd. If found, set the member variables
  // #filename, #line, and #functionname.
  void do_find_address_in_section(bfd* abfd, asection* section);

  // @name Local variables set by operator()
  // @{
  asymbol** syms;  ///< BFD symbol table information
  bfd_vma pc;      ///< Virtual memory address
  // @}

  // @name Local variables set by do_find_matching_file()
  // @{
  const char* bfdFile;  ///< Filename of the matching shared object
  bfd_vma base_addr;    ///< Base address of the shared object
  // @}

  // @name Local variables set by do_find_address_in_section()
  // @{
  const char* filename;      ///< Name of matching source file
  const char* functionname;  ///< Name of matching function
  unsigned int line;         ///< Line number in matching source file
  bool found;                ///< Indicates whether a match was found
  // @}
#endif /* HAVE_BFD */
};

#endif
