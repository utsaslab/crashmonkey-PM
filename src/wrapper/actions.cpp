#include "actions.h"


namespace wrapper {

int Checkpoint() {
  // Issue an unusual instruction so that writetracker can pick it up and record a checkpoint in wt.out
  // f2xm1 is 2^x-1 floating point, likely to be unused by file systems
  asm volatile("f2xm1");
  return 0;
}

} // wrapper
