// EPOS IA32 Time-Stamp Counter Mediator Declarations

#ifndef __ia32_tsc_h
#define __ia32_tsc_h

#include <cpu.h>
#include <tsc.h>

__BEGIN_SYS

class IA32_TSC: private TSC_Common
{
public:
    using TSC_Common::Hertz;
    using TSC_Common::Time_Stamp;

public:
    IA32_TSC() {}

    static Hertz frequency() { return CPU::clock(); }

    static Time_Stamp time_stamp() {
        Time_Stamp ts;
        ASM("rdtsc" : "=A" (ts) : ); // must be volatile!
        return ts;
    }

    static Time_Stamp timestamp_to_seconds(Time_Stamp ts) {
    	return (ts / CPU::clock());
    }
};

__END_SYS

#endif
