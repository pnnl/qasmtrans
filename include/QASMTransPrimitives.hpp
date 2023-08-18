#pragma once

#include <sys/time.h>

/* Constant value of PI */
#define PI 3.14159265358979323846

namespace QASMTrans
{
    /* Basic data type for indices */
    using IdxType = long long int;
    /* Basic data type for value */
    using ValType = double;

    /***********************************************
     * CPU Timer based on Linux sys/time.h
     ***********************************************/
    // CPU timer
    inline double get_cpu_timer()
    {
        struct timeval tp;
        gettimeofday(&tp, NULL);
        // get current timestamp in milliseconds
        return (double)tp.tv_sec * 1e3 + (double)tp.tv_usec * 1e-3;
    }
    // CPU timer object definition
    typedef struct CPU_Timer
    {
        CPU_Timer() { start = stop = 0.0; }
        void start_timer() { start = get_cpu_timer(); }
        void stop_timer() { stop = get_cpu_timer(); }
        double measure()
        {
            double millisconds = stop - start;
            return millisconds;
        }
        double start;
        double stop;
    } cpu_timer;
} // namespace QASMTrans
