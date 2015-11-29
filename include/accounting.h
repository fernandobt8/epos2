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
	// MUDAR ISSO DEPOIS PARA CRITERION
	// Accounting(int relative_max_size = 3) {
   	typedef Simple_List<double> List;

	Accounting() {
		_last_runtime = 0;
		// _relative_total_runtime = 0;
		// _chronometer_running = false;
		// _relative_max_size = relative_max_size;
		// _relative_size = 0;
		_created_at = Machine::cpu_id();
		// _history[Traits<Build>::CPUS];

		for(unsigned int i = 0; i < Traits<Build>::CPUS; i++) {
			_total_runtime[i] = 0;
			// _total_wait[i] = 0;
		}
   	}

	int created_at() { return _created_at; }
   	
   	// Runtime related to the current CPU
	T last_runtime() { return _last_runtime; }
	void last_runtime(T ts) { 
		_last_runtime = ts;

		// Saving to the history of running time.
		// if (_relative_size < _relative_max_size) {
		// 	_relative_size++;
		// 	_relative_total_runtime += ts;
		// } else {
		// 	_relative_size = 0;
		// 	_relative_total_runtime = 0;
		// }
	}

	// Runtime related to all CPUs
	T runtime_at(int cpu_id) { return _total_runtime[cpu_id]; }
	void total_runtime(T ts) { _total_runtime[Machine::cpu_id()] += ts; }
	T total_runtime() {
		T runtime = 0;
		for (unsigned int i = 0; i < Traits<Build>::CPUS; i++) 
			runtime += _total_runtime[i];
		return runtime;
	}

	List history() { return _history; }
	double history_head() { return _history.head()->object(); }
	void add_history(double value) {
		_history.insert(new (SYSTEM) List::Element(&value)); 
	}
	double remove_history() {
		return _history.remove()->object();
	}
	// T relative_total_runtime() { return _relative_total_runtime; } 
	// void relative_total_runtime(T ts) { _relative_total_runtime = ts; }
	// int relative_size() { return _relative_size; }

	// Wait-time related to the current CPU
	// void waiting_start() { _waittime.reset(); _waittime.start(); _chronometer_running = true; }
	// void waiting_stop() { _waittime.stop(); _chronometer_running = false; };
	// void chronometer_running() { return _chronometer_running; }
	// void waiting_update() { total_wait(_waittime.read_ticks()); }
	// T waiting() { return _waittime.read_ticks(); }

	// Wait-time related to all CPUs
	// T wait_at(int cpu_id) { return _total_wait[cpu_id]; }
	// void total_wait(T ts) { _total_wait[Machine::cpu_id()] += ts; }
	// T total_wait() {
	// 	T waittime = 0;
	// 	for (unsigned int i = 0; i < Traits<Build>::CPUS; i++)
	// 		waittime += _total_wait[i];
	// 	return waittime;
	// }

private:
	T _last_runtime,
	  _total_runtime[Traits<Build>::CPUS];
	  // _relative_total_runtime,
	  // _total_wait[Traits<Build>::CPUS];

	// Chronometer _waittime;
	// bool _chronometer_running;
	List _history; // mudar depois
	int _created_at; // At which CPU this resource was created
	// int _relative_max_size, // For how many context switches "_relative_total_runtime" will be stored
	// int _relative_size; // How many context switches are saved on "_relative_total_runtime" right now

};

__END_SYS

#endif