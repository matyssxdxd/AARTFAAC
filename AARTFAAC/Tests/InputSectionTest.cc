#include "AARTFAAC/InputSection.h"

int main(int argc, char **argv)
{
  AARTFAAC_Parset parset(argc, argv);
  InputSection inputSection(parset);
  parset.stopTime().wait();
  return 0;
}
