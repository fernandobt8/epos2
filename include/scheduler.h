// EPOS Scheduler Abstraction Declarations

#ifndef __scheduler_h
#define __scheduler_h

#include <utility/list.h>
#include <cpu.h>
#include <machine.h>

__BEGIN_SYS

// All scheduling criteria, or disciplines, must define operator int() with
// the semantics of returning the desired order of a given object within the
// scheduling list
namespace Scheduling_Criteria
{
    // Priority (static and dynamic)
    class Priority
    {
    public:
        enum {
            MAIN   = 0,
            HIGH   = 1,
            NORMAL = (unsigned(1) << (sizeof(int) * 8 - 1)) - 3,
            LOW    = (unsigned(1) << (sizeof(int) * 8 - 1)) - 2,
            IDLE   = (unsigned(1) << (sizeof(int) * 8 - 1)) - 1
        };

        static const bool timed = false;
        static const bool dynamic = false;
        static const bool preemptive = true;

    public:
        Priority(int p = NORMAL): _priority(p) {}

        operator const volatile int() const volatile { return _priority; }

        void update() {}

    protected:
        volatile int _priority;
    };

    // Round-Robin
    class RR: public Priority
    {
    public:
        enum {
            MAIN   = 0,
            NORMAL = 1,
            IDLE   = (unsigned(1) << (sizeof(int) * 8 - 1)) - 1
        };

        static const bool timed = true;
        static const bool dynamic = false;
        static const bool preemptive = true;
        static const unsigned int HEADS = Traits<Machine>::CPUS;

    public:
        RR(int p = NORMAL): Priority(p) {}

        static unsigned int current_head() { return Machine::cpu_id(); }
    };

    // First-Come, First-Served (FIFO)
    class FCFS: public Priority
    {
    public:
        enum {
            MAIN   = 0,
            NORMAL = 1,
            IDLE   = (unsigned(1) << (sizeof(int) * 8 - 1)) - 1
        };

        static const bool timed = false;
        static const bool dynamic = false;
        static const bool preemptive = false;

    public:
        FCFS(int p = NORMAL); // Defined at Alarm
    };

    template<typename T>
    class CpuAffinity: public Priority
	{
	public:
		enum {
			MAIN   = 0,
			NORMAL = 1,
			IDLE   = (unsigned(1) << (sizeof(int) * 8 - 1)) - 1
		};

		static const bool timed = true;
		static const bool dynamic = false;
		static const bool preemptive = true;
		static const unsigned int QUEUES = Traits<Machine>::CPUS;
		unsigned int _queue;

	public:
		CpuAffinity(int p = NORMAL): Priority(p) {
			if(_priority == IDLE || _priority == MAIN)
				_queue = Machine::cpu_id();
			else
				_queue = T::schedule_queue();
		}

		CpuAffinity(int p, unsigned int queue): Priority(p) {
			_queue = queue;
		}
		static unsigned int current_queue() { return Machine::cpu_id(); }

		const unsigned int queue() const { return _queue; }
	};

    template<typename T>
    class CFSAffinity: public Priority
	{
	public:
		enum {
			MAIN   = 0,
			NORMAL = 1,
			IDLE   = (unsigned(1) << (sizeof(int) * 8 - 1)) - 1
		};

		static const bool timed = true;
		static const bool dynamic = false;
		static const bool preemptive = true;
		static const unsigned int QUEUES = Traits<Machine>::CPUS;
		unsigned int _queue;

	public:
		CFSAffinity(int p = NORMAL): Priority(p) {
			if(_priority == IDLE || _priority == MAIN)
				_queue = Machine::cpu_id();
			else
				_queue = T::schedule_queue();
		}

		static unsigned int current_queue() { return Machine::cpu_id(); }

		const unsigned int queue() const { return _queue; }
	};
}


// Scheduling_Queue
template<typename T, typename R = typename T::Criterion>
class Scheduling_Queue: public Scheduling_Multilist<T> {};

// Scheduler
// Objects subject to scheduling by Scheduler must declare a type "Criterion"
// that will be used as the scheduling queue sorting criterion (viz, through
// operators <, >, and ==) and must also define a method "link" to export the
// list element pointing to the object being handled.
template<typename T>
class Scheduler: public Scheduling_Queue<T>
{
private:
    typedef Scheduling_Queue<T> Base;
    double _waiting_time[T::Criterion::QUEUES];

public:
    typedef typename T::Criterion Criterion;
    typedef Scheduling_List<T, Criterion> Queue;
    typedef typename Queue::Element Element;

public:
    Scheduler() {}

    unsigned int schedulables() { return Base::size(); }

    T * volatile chosen() {
    	// If called before insert(), chosen will dereference a null pointer!
    	// For threads, we this won't happen (see Thread::init()).
    	// But if you are unsure about your new use of the scheduler,
    	// please, pay the price of the extra "if" bellow.
//    	return const_cast<T * volatile>((Base::chosen()) ? Base::chosen()->object() : 0);
    	return const_cast<T * volatile>(Base::chosen()->object());
    }

    void insert(T * obj) {
        db<Scheduler>(TRC) << "Scheduler[chosen=" << chosen() << "]::insert(" << obj << ")" << endl;

        update_waiting_time(obj);
        Base::insert(obj->link());
    }

    T * remove(T * obj) {
        db<Scheduler>(TRC) << "Scheduler[chosen=" << chosen() << "]::remove(" << obj << ")" << endl;

        if(Base::remove(obj->link())){
        	update_waiting_time(obj, false);
        	return obj;
        } else {
        	return 0;
        }
    }

    void suspend(T * obj) {
        db<Scheduler>(TRC) << "Scheduler[chosen=" << chosen() << "]::suspend(" << obj << ")" << endl;

        Base::remove(obj->link());
        update_waiting_time(obj, false);
    }

    void resume(T * obj) {
        db<Scheduler>(TRC) << "Scheduler[chosen=" << chosen() << "]::resume(" << obj << ")" << endl;

        Base::insert(obj->link());
        update_waiting_time(obj);
    }

    T * choose() {
        db<Scheduler>(TRC) << "Scheduler[chosen=" << chosen() << "]::choose() => ";

        T * obj = Base::choose()->object();

        db<Scheduler>(TRC) << obj << endl;

        return obj;
    }

    T * choose_another() {
        db<Scheduler>(TRC) << "Scheduler[chosen=" << chosen() << "]::choose_another() => ";

        T * obj = Base::choose_another()->object();

        db<Scheduler>(TRC) << obj << endl;

        return obj;
    }

    T * choose(T * obj) {
        db<Scheduler>(TRC) << "Scheduler[chosen=" << chosen() << "]::choose(" << obj;

        if(!Base::choose(obj->link()))
            obj = 0;

        db<Scheduler>(TRC) << obj << endl;

        return obj;
    }

    void update_waiting_time(T* obj, bool update_object = true){
    	if(obj->link()->rank() == Criterion::IDLE || obj->link()->rank() == Criterion::MAIN){
			unsigned int queue = obj->link()->rank().queue();
			double size = size_without_idle(queue);
			if(size > 0){
				_waiting_time[queue] = ((double)100 / size) - (double)100;
				if(update_object){
					obj->update_waiting_time(_waiting_time[queue]);
				}
			}
    	}
    }

    double size_without_idle(unsigned int queue) {
    	if(Base::_list[queue].empty()){
    		return 0;
    	} else{
    		return Base::_list[queue].size() - 1;
    	}
    }

    unsigned int queue_min_size() const {
		double min = 1;
		unsigned int queue = 0;

		for(unsigned int i = 0; i < T::Criterion::QUEUES; i++) {
			if(min > _waiting_time[i]){
				min = _waiting_time[i];
				queue = i;
			}
		}

		return queue;
	}

    T* chosen_from_list(unsigned int list){
    	return Base::_list[list].chosen()->object();
    }
};

__END_SYS

#endif
