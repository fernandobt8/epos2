// Thread Accounting

#ifndef __accounting_h
#define __accounting_h

#include <tsc.h>
#include <cpu.h>
#include <machine.h>
#include <chronometer.h>
#include <utility/list.h>

__BEGIN_SYS
template<typename T> // T should be Time_Stamp, Tick or something like that.
class Accounting
{

public:
   	typedef Simple_List<double> List;

	Accounting() {
		_last_runtime = 0;
		_created_at = Machine::cpu_id();

		for(unsigned int i = 0; i < Traits<Build>::CPUS; i++) {
			_total_runtime[i] = 0;
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

	// History
	List history() { return _history; }
	double history_head() { return _history.head()->object(); }
	void add_history(double value) {
		_history.insert(new (SYSTEM) List::Element(&value)); 
	}
	double remove_history() {
		return _history.remove()->object();
	}

private:
	T _last_runtime,
	  _total_runtime[Traits<Build>::CPUS];
	
	List _history;
	int _created_at; // At which CPU this resource was created

};

__END_SYS

#endif