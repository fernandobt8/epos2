// EPOS Thread Abstraction Implementation

#include <machine.h>
#include <system.h>
#include <thread.h>
#include <alarm.h> // for FCFS

// This_Thread class attributes
__BEGIN_UTIL
bool This_Thread::_not_booting;
__END_UTIL

__BEGIN_SYS

// Class attributes
volatile unsigned int Thread::_thread_count;
Scheduler_Timer * Thread::_timer;
Rebalancer_Timer * Thread::_rebalancer_timer;
Scheduler<Thread> Thread::_scheduler;
Spin Thread::_lock;
Thread::List Thread::toSuspend[Thread::Criterion::QUEUES];

// Methods
void Thread::constructor_prolog(unsigned int stack_size)
{
    lock();

    _thread_count++;
    _scheduler.insert(this);

    _stack = new (SYSTEM) char[stack_size];
}


void Thread::constructor_epilog(const Log_Addr & entry, unsigned int stack_size)
{
    db<Thread>(TRC) << "Thread(entry=" << entry
                    << ",state=" << _state
                    << ",priority=" << _link.rank()
                    << ",stack={b=" << reinterpret_cast<void *>(_stack)
                    << ",s=" << stack_size
                    << "},context={b=" << _context
                    << "," << *_context << "}) => " << this << endl;

    if((_state != READY) && (_state != RUNNING))
        _scheduler.suspend(this);

    if(preemptive && (_state == READY) && (_link.rank() != IDLE)) {
        cutucao(this);
    } else
        if((_state == RUNNING) || (_link.rank() == IDLE)) // Keep interrupts disabled during init_first()
            unlock(false);
        else
            unlock();
}


Thread::~Thread()
{
    lock();

    db<Thread>(TRC) << "~Thread(this=" << this
                    << ",state=" << _state
                    << ",priority=" << _link.rank()
                    << ",stack={b=" << reinterpret_cast<void *>(_stack)
                    << ",context={b=" << _context
                    << "," << *_context << "})" << endl;

    // The running thread cannot delete itself!
    assert(_state != RUNNING);

    switch(_state) {
    case RUNNING:  // For switch completion only: the running thread would have deleted itself! Stack wouldn't have been released!
        exit(-1);
        break;
    case READY:
        _scheduler.remove(this);
        _thread_count--;
        break;
    case SUSPENDED:
        _scheduler.resume(this);
        _scheduler.remove(this);
        _thread_count--;
        break;
    case WAITING:
        _waiting->remove(this);
        _scheduler.resume(this);
        _scheduler.remove(this);
        _thread_count--;
        break;
    case FINISHING: // Already called exit()
        break;
    }

    if(_joining)
        _joining->resume();

    unlock();

    delete _stack;
}


void Thread::priority(const Priority & c)
{
    lock();

    db<Thread>(TRC) << "Thread::priority(this=" << this << ",prio=" << c << ")" << endl;

    _link.rank(Criterion(c));

    if(_state != RUNNING) {
        _scheduler.remove(this);
        _scheduler.insert(this);
    }

    if(preemptive) {
        cutucao(this);
    }
}


int Thread::join()
{
    lock();

    db<Thread>(TRC) << "Thread::join(this=" << this << ",state=" << _state << ")" << endl;

    // Precondition: no Thread::self()->join()
    assert(running() != this);

    // Precondition: a single joiner
    assert(!_joining);

    if(_state != FINISHING) {
        _joining = running();
        _joining->suspend(true);
    } else
        unlock();

    return *reinterpret_cast<int *>(_stack);
}


void Thread::pass()
{
    lock();

    db<Thread>(TRC) << "Thread::pass(this=" << this << ")" << endl;

    Thread * prev = running();
    if(prev->queue() == this->queue()){
		Thread * next = _scheduler.choose(this);

		if(next)
			dispatch(prev, next, false);
		else {
			db<Thread>(WRN) << "Thread::pass => thread (" << this << ") not ready!" << endl;
			unlock();
		}
    } else {
    	unlock();
    }
}

void Thread::suspend(bool locked)
{
    if(!locked)
        lock();

    db<Thread>(TRC) << "Thread::suspend(this=" << this << ")" << endl;

    Thread * prev = running();
    if(prev->queue() == this->queue()){
		//fudeu se this é de cpu diferente de running.
		_scheduler.suspend(this);
		_state = SUSPENDED;

		Thread * next = running();

		dispatch(prev, next);
    } else {
    	toSuspend[this->queue()].insert(new (SYSTEM) List::Element(this));
    	IC::ipi_send(this->queue(), IC::INT_SUSPEND);
    	unlock();
    }
}

void Thread::suspend_handler(const IC::Interrupt_Id & i)
{
	lock();

	List::Element* e = toSuspend[Machine::cpu_id()].remove_head();
	Thread* suspend = e->object();
	delete e;
	suspend->suspend(true);
}


void Thread::resume()
{
    lock();

    db<Thread>(TRC) << "Thread::resume(this=" << this << ")" << endl;

    if(_state == SUSPENDED) {
        _state = READY;
        _scheduler.resume(this);

        if(preemptive)
            cutucao(this);
    } else {
        db<Thread>(WRN) << "Resume called for unsuspended object!" << endl;

        unlock();
    }
}


// Class methods
void Thread::yield()
{
    lock();

    db<Thread>(TRC) << "Thread::yield(running=" << running() << ")" << endl;

    Thread * prev = running();
    Thread * next = _scheduler.choose_another();

    dispatch(prev, next);
}


void Thread::exit(int status)
{
    lock();

    db<Thread>(TRC) << "Thread::exit(status=" << status << ") [running=" << running() << "]" << endl;

    Thread * prev = running();
    _scheduler.remove(prev);
    *reinterpret_cast<int *>(prev->_stack) = status;
    prev->_state = FINISHING;

    _thread_count--;

    if(prev->_joining) {
        prev->_joining->_state = READY;
        _scheduler.resume(prev->_joining);
        prev->_joining = 0;
    }

    dispatch(prev, _scheduler.choose());
}


void Thread::sleep(Queue * q)
{
    db<Thread>(TRC) << "Thread::sleep(running=" << running() << ",q=" << q << ")" << endl;

    // lock() must be called before entering this method
    assert(locked());

    Thread * prev = running();
    _scheduler.suspend(prev);
    prev->_state = WAITING;
    q->insert(&prev->_link);
    prev->_waiting = q;

    dispatch(prev, _scheduler.chosen());
}


void Thread::wakeup(Queue * q)
{
    db<Thread>(TRC) << "Thread::wakeup(running=" << running() << ",q=" << q << ")" << endl;

    // lock() must be called before entering this method
    assert(locked());

    if(!q->empty()) {
        Thread * t = q->remove()->object();
        t->_state = READY;
        t->_waiting = 0;
        _scheduler.resume(t);

        if(preemptive)
            cutucao(t);
    } else
        unlock();
}


void Thread::wakeup_all(Queue * q)
{
    db<Thread>(TRC) << "Thread::wakeup_all(running=" << running() << ",q=" << q << ")" << endl;

    // lock() must be called before entering this method
    assert(locked());

    if(!q->empty())
        while(!q->empty()) {
            Thread * t = q->remove()->object();
            t->_state = READY;
            t->_waiting = 0;
            _scheduler.resume(t);

            if(preemptive) {
                cutucao(t);
                lock();
            }
         }
    else
        unlock();
}


void Thread::reschedule()
{
    db<Scheduler<Thread> >(TRC) << "Thread::reschedule()" << endl;

    // lock() must be called before entering this method
    assert(locked());

    Thread * prev = running();
    Thread * next = _scheduler.choose();

    dispatch(prev, next);
}


void Thread::time_slicer(const IC::Interrupt_Id & i)
{
    lock();

    reschedule();
}

void Thread::rebalance_handler(const IC::Interrupt_Id & i)
{
//	lock();
//	Thread* prev = _scheduler.head()->object();
//	int chosen_list = -1;
//	unsigned int min = 0;
//	for(unsigned int i = 0; i < Criterion::QUEUES; i++){
//		if(prev->queue() != i){
//			Thread* other = _scheduler.chosen_from_list(i);
//			if(min > other->stats.total_runtime()){
//				chosen_list = i;
//				min = other->stats.total_runtime();
//			}
//		}
//	}
//	if(prev->stats.total_runtime() < min - QUANTUM){
//		//fazer calculo de prioridade na proxima fila
//		//prev->_link->rank(Criterion(NORMAL, chosen_list));
//		//_scheduler.insert(prev);
//	}
//	db<void>(TRC) << "re running: " << running() << " prev: " << prev << " q: " << prev->queue() << " nq: " << chosen_list << endl;
//	unlock();
}

void Thread::reschedule_handler(const IC::Interrupt_Id & i)
{
	lock();

 	reschedule();
}

//Novembro Azul
void Thread::cutucao(Thread * needy)
{
	IC::ipi_send(needy->queue(), IC::INT_RESCHEDULER);
	unlock();
}

/*

Aula 25/11/2015.

Quantum = tempo para acontecer uma reavaliação de prioridade. Valor fixo.
Vamos utilizar prioridades dinâmicas.

mediaHistórica = (mediaHistórica + corrente)/2  
[ Não faz sentido armazenar a série histórica inteira, armazenar uma quantidade de samples. ] 
[ Ir para o mais elaborado caso isso não nos satisfaça (não vai nos satisfazer). ]

Afinidade no algoritmo global pode. Mas é muito pior.

Jantar dos Filósofos não é um bom teste p/ testar afinidade de cache 
(por causa do for que não faz nada e não precisa de nada na cache).

"Bin packing". Resolver como first-fit, worst-fit, best-fit.
https://en.wikipedia.org/wiki/Bin_packing_problem

| P2         |
| P0  P1  P3 |  <-- como fazer justiça nesse caso? Tem que ter arrumar, mas alguém vai
|------------|      ter um pouco de injustiça.
| C0  C1  C2 |

Fazer justiça por fila de CPU tbm.

C* = CPU
P* = PROCESS

O erro do TSC é em relação ao timestamp que em um ambiente emulado (QEMU) funciona, pois a instrução
assembly realizada no QEMU que retorna o TS é mapeada para uma mesma CPU direto do hardware 
(ou seja, cada CPU lógica do EPOS tem um timestamp compatível com as outras CPUs lógicas 
[sequencial e posterior]. Não podemos assumir isso, pois em um ambiente real isso não 
funcionaria). 

CFS é TEMPO DE ESPERA das threads, não tempo que rodou. Se uma thred não aumentou o tempo 
de espera das demais threads (exemplo: outras threads esperando IO), essa thread não é 
prejudicada na próxima execução. 

*/
void Thread::dispatch(Thread * prev, Thread * next, bool charge)
{

    Count count = 0;
    if(charge) {
        if(Criterion::timed)
            count = _timer->reset_and_count();
    }

    // Accounting the runtime.
    // Don't care if prev != next, because at exit(), prev==next and we still need to account that.
    if ((prev->_state == RUNNING || prev->_state == FINISHING) && prev->_link.rank() != IDLE){
        prev->stats.last_runtime(count);
        prev->stats.total_runtime(count);
    }

    if(prev != next) {

        if(prev->_state == RUNNING)
            prev->_state = READY;
            
        next->_state = RUNNING;

        db<Thread>(TRC) << "Thread::dispatch(prev=" << prev << ",next=" << next << ")" << endl;
        db<Thread>(INF) << "prev={" << prev << ",ctx=" << *prev->_context << "}" << endl;
        db<Thread>(INF) << "next={" << next << ",ctx=" << *next->_context << "}" << endl;

        if(smp)
            _lock.release(); // Note that releasing the lock here, even with interrupts disabled, allows for another CPU to select "prev".
                             // The analysis of whether it could get scheduled by another CPU while its context is being saved by CPU::switch_context()
                             // must focus on the time it takes to save a context and to reschedule a thread. If this gets stringent for a given architecture,
                             // then unlocking must be moved into the mediator. For x86 and ARM it doesn't seam to be the case.

        CPU::switch_context(&prev->_context, next->_context);
    } else
        if(smp)
            _lock.release();

    CPU::int_enable();
}


int Thread::idle()
{
    while(_thread_count > Machine::n_cpus()) { // someone else besides idles
        if(Traits<Thread>::trace_idle)
            db<Thread>(TRC) << "Thread::idle(CPU=" << Machine::cpu_id() << ",this=" << running() << ")" << endl;

        CPU::int_enable();
        CPU::halt();
    }

    CPU::int_disable();
    if(Machine::cpu_id() == 0) {
        db<Thread>(WRN) << "We are Idle!" << endl;
        db<Thread>(WRN) << "We are Many!" << endl;
        db<Thread>(WRN) << "We are LEGION!" << endl;
        db<Thread>(WRN) << "TREMBLE OVER OF THE IDLE POWER" << endl;
        db<Thread>(WRN) << "Just kiddin" << endl;
        db<Thread>(WRN) << "The last thread has exited!" << endl;
        if(reboot) {
            db<Thread>(WRN) << "Rebooting the machine ..." << endl;
            Machine::reboot();
        } else
            db<Thread>(WRN) << "Halting the machine ..." << endl;
    }
    CPU::halt();

    return 0;
}

__END_SYS

// Id forwarder to the spin lock
__BEGIN_UTIL
unsigned int This_Thread::id()
{
    return _not_booting ? reinterpret_cast<volatile unsigned int>(Thread::self()) : Machine::cpu_id() + 1;
}
__END_UTIL
