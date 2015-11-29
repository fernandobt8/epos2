// Thread Accounting

#ifndef __accounting_h
#define __accounting_h

#include <tsc.h>
#include <cpu.h>
#include <machine.h>

__BEGIN_SYS
template<typename T> // T should be Time_Stamp, Tick or something like that.
class Accounting
{

public:
	Accounting(): _last_runtime(0), _waiting_since(0), _created_at(Machine::cpu_id()) {
		for(unsigned int i = 0; i < Traits<Build>::CPUS; i++) {
			_total_runtime[i] = 0;
			_total_waittime[i] = 0;
		}
   	}

	T last_runtime() { return _last_runtime; }
	T last_waittime() { return waiting_since(); } // deprecated
	T waiting_since() { return _waiting_since; } 
	T runtime_at(int cpu_id) { return _total_runtime[cpu_id]; }
	T waittime_at(int cpu_id) { return _total_waittime[cpu_id]; }
	int created_at() { return _created_at; } 

	T total_runtime() {
		T runtime = 0;
		for (unsigned int i = 0; i < Traits<Build>::CPUS; i++) 
			runtime += _total_runtime[i];
		return runtime;
	}

	T total_waittime() {
		T waittime = 0;
		for (unsigned int i = 0; i < Traits<Build>::CPUS; i++)
			waittime += _total_waittime[i];
		return waittime;
	}

	void last_runtime(T ts) { _last_runtime = ts; }
	void last_waittime(T ts) { waiting_since(ts); } // deprecated
	void waiting_since(T ts) { _waiting_since = ts; }
	void total_runtime(T ts) { _total_runtime[Machine::cpu_id()] += ts; }
	void total_waittime(T ts) { _total_waittime[Machine::cpu_id()] += ts; }

private:
	T _last_runtime, 
	  _waiting_since, 
	  _total_runtime[Traits<Build>::CPUS],
	  _total_waittime[Traits<Build>::CPUS];

	// At which CPU this resource was created
	int _created_at;

};

__END_SYS

#endif