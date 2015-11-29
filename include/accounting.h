// Thread Accounting

#ifndef __accounting_h
#define __accounting_h

#include <tsc.h>
#include <cpu.h>
#include <machine.h>
#include <chronometer.h>

__BEGIN_SYS
template<typename T> // T should be Time_Stamp, Tick or something like that.
class Accounting
{

public:
	Accounting(): _last_runtime(0), _chronometer_running(false), _created_at(Machine::cpu_id()) {
		for(unsigned int i = 0; i < Traits<Build>::CPUS; i++) {
			_total_runtime[i] = 0;
			_total_wait[i] = 0;
		}
   	}

	int created_at() { return _created_at; } 
   	
   	// Runtime related to the current CPU
	T last_runtime() { return _last_runtime; }
	void last_runtime(T ts) { _last_runtime = ts; }

	// Runtime related to all CPUs
	T runtime_at(int cpu_id) { return _total_runtime[cpu_id]; }
	void total_runtime(T ts) { _total_runtime[Machine::cpu_id()] += ts; }
	T total_runtime() {
		T runtime = 0;
		for (unsigned int i = 0; i < Traits<Build>::CPUS; i++) 
			runtime += _total_runtime[i];
		return runtime;
	}

	// Wait-time related to the current CPU
	void waiting_start() { _waittime.reset(); _waittime.start(); _chronometer_running = true; }
	void waiting_stop() { _waittime.stop(); _chronometer_running = false; };
	void chronometer_running() { return _chronometer_running; }
	void waiting_update() { total_wait(_waittime.read_ticks()); }
	T waiting() { return _waittime.read_ticks(); }

	// Wait-time related to all CPUs
	T wait_at(int cpu_id) { return _total_wait[cpu_id]; }
	void total_wait(T ts) { _total_wait[Machine::cpu_id()] += ts; }
	T total_wait() {
		T waittime = 0;
		for (unsigned int i = 0; i < Traits<Build>::CPUS; i++)
			waittime += _total_wait[i];
		return waittime;
	}

private:
	T _last_runtime, 
	  _total_runtime[Traits<Build>::CPUS],
	  _total_wait[Traits<Build>::CPUS];

	Chronometer _waittime;
	bool _chronometer_running;

	int _created_at; // At which CPU this resource was created

};

__END_SYS

#endif