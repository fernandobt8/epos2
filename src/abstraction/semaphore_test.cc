// EPOS Semaphore Abstraction Test Program

#include <utility/ostream.h>
#include <thread.h>
#include <mutex.h>
#include <semaphore.h>
#include <alarm.h>
#include <display.h>
#include <architecture/ia32/cpu.h>

using namespace EPOS;

const int iterations = 4;

Mutex table;

Thread * phil[5];
Semaphore * chopstick[5];

OStream cout;

void countDelay(int delay_ms){
    unsigned long iterations = delay_ms * (CPU::clock() / 1000);
    for(int i; i < iterations; i++) {
        asm("");
    }
}

int philosopher(int n, int l, int c)
{
    int first = (n < 4)? n : 0;
    int second = (n < 4)? n + 1 : 4;

    for(int i = iterations; i > 0; i--) {

        table.lock();
        Display::position(l, c);
        cout << "P"<< n << " is thinking on CPU# " << Machine::cpu_id() << endl;
        table.unlock();

        countDelay(200);

        chopstick[first]->p();   // get first chopstick
        chopstick[second]->p();   // get second chopstick

        table.lock();
        Display::position(l, c);
        cout << "P"<< n << " is eeeating on CPU# " << Machine::cpu_id() << endl;
        table.unlock();

        countDelay(200);

        chopstick[first]->v();   // release first chopstick
        chopstick[second]->v();   // release second chopstick
    }

    table.lock();
    Display::position(l, c);
    cout << "Philos#"<< n << " done  on CPU# " << Machine::cpu_id() << endl;
    table.unlock();

    return iterations;
}

int main()
{
    table.lock();
    Display::clear();
    Display::position(0, 0);
    cout << "The Philosopher's Dinner: on #" << Machine::cpu_id() << endl;

    for(int i = 0; i < 5; i++)
        chopstick[i] = new Semaphore;

    phil[0] = new Thread(&philosopher, 0,  5, 32);
    phil[1] = new Thread(&philosopher, 1, 10, 54);
    phil[2] = new Thread(&philosopher, 2, 16, 49);
    phil[3] = new Thread(&philosopher, 3, 16, 14);
    phil[4] = new Thread(&philosopher, 4, 10, 10);

    cout << "Philosophers are alive and angry! (on CPU# " << Machine::cpu_id() << endl;

    Display::position(7, 54);
    cout << '/' << endl;
    Display::position(13, 54);
    cout << '\\'<< endl;
    Display::position(16, 45);
    cout << '|'<< endl;
    Display::position(13, 37);
    cout << '/'<< endl;
    Display::position(7, 37);
    cout << '\\'<< endl;
    Display::position(19, 0);

    cout << "The dinner is served ... on Table#" << Machine::cpu_id() << endl;
    table.unlock();

    for(int i = 0; i < 5; i++) {
        int ret = phil[i]->join();
        table.lock();
        Display::position(20 + i, 0);
        cout << "Philosopher " << i << " ate " << ret << " times (on #" << Machine::cpu_id() << ")" << endl;
        table.unlock();
    }

     // Printing statistics (only a single CPU will print this)
    typedef Timer::Tick Count;
    cout << "\n\nRuntime of each philosopher:\n" << endl;
    for (int i = 0; i < 5; i++) {
        Count thread_runtime = 0;
        table.lock();

        cout << "Philosopher " << i << "  ";
        for (int cpu_id = 0; cpu_id < Traits<Build>::CPUS; cpu_id++) {
            Count ts_per_cpu = phil[i]->runtime_at(cpu_id);
            thread_runtime += ts_per_cpu;
            cout << "| " << cpu_id << ": " << ts_per_cpu << "  ";
        }
        cout << "| T: " << thread_runtime << endl;
        
        table.unlock();
    }

    cout << "\n\nWait time of each philosopher (not precise) :\n" << endl;
    for (int i = 0; i < 5; i++) {
        Count thread_waittime = 0;
        table.lock();

        cout << "Philosopher " << i << "  ";
        for (int cpu_id = 0; cpu_id < Traits<Build>::CPUS; cpu_id++) {
            Count ts_per_cpu = phil[i]->waittime_at(cpu_id);
            thread_waittime += ts_per_cpu;
            cout << "| " << cpu_id << ": " << ts_per_cpu << "  ";
        }
        cout << "| T: " << thread_waittime << endl;
        
        table.unlock();
    }

    for(int i = 0; i < 5; i++)
        delete chopstick[i];
    for(int i = 0; i < 5; i++)
        delete phil[i];

    cout << "Dinner is Over! on Table#" << Machine::cpu_id() << endl;

    return 0;
}
