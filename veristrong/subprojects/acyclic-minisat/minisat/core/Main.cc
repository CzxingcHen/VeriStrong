/*****************************************************************************************[Main.cc]
Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007-2010, Niklas Sorensson

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#include <errno.h>
#include <zlib.h>
#include <filesystem>
#include <iostream>

#include "minisat/utils/System.h"
#include "minisat/utils/ParseUtils.h"
#include "minisat/utils/Options.h"
#include "minisat/utils/Monitor.h"
// #include "minisat/core/Dimacs.h"
#include "minisat/core/Agnf.h"
#include "minisat/core/Solver.h"
#include "minisat/core/Polygraph.h"
#include "minisat/core/AcyclicSolver.h"
#include "minisat/core/AcyclicSolverHelper.h"

using namespace Minisat;
namespace fs = std::filesystem;

//=================================================================================================


static Solver* solver;
// Terminate by notifying the solver and back out gracefully. This is mainly to have a test-case
// for this feature of the Solver as it may take longer than an immediate call to '_exit()'.
static void SIGINT_interrupt(int) { solver->interrupt(); }

// Note that '_exit()' rather than 'exit()' has to be used. The reason is that 'exit()' calls
// destructors and may cause deadlocks if a malloc/free function happens to be running (these
// functions are guarded by locks for multithreaded use).
static void SIGINT_exit(int) {
    printf("\n"); printf("*** INTERRUPTED ***\n");
    if (solver->verbosity > 0){
        solver->printStats();
        printf("\n"); printf("*** INTERRUPTED ***\n"); }
    _exit(1); }


//=================================================================================================
// Main:


int main(int argc, char** argv)
{
    try {
        setUsageHelp("USAGE: %s [options] <input-file> <result-output-file>\n\n  where input may be either in plain or gzipped DIMACS.\n");
        setX86FPUPrecision();

        // Extra options:
        //
        IntOption    verb   ("MAIN", "verb",   "Verbosity level (0=silent, 1=some, 2=more).", 0, IntRange(0, 2));
        IntOption    cpu_lim("MAIN", "cpu-lim","Limit on CPU time allowed in seconds.\n", 0, IntRange(0, INT32_MAX));
        IntOption    mem_lim("MAIN", "mem-lim","Limit on memory usage in megabytes.\n", 0, IntRange(0, INT32_MAX));
        BoolOption   strictp("MAIN", "strict", "Validate DIMACS header during parsing.", false);
        
        parseOptions(argc, argv, true);

        AcyclicSolver S;

        double initial_time = cpuTime();

        S.verbosity = verb;
        
        // solver = &S;
        // Use signal handlers that forcibly quit until the solver will be able to respond to
        // interrupts:
        sigTerm(SIGINT_exit);

        // Try to set resource limits:
        if (cpu_lim != 0) limitTime(cpu_lim);
        if (mem_lim != 0) limitMemory(mem_lim);
        
        if (argc == 1)
            printf("Reading from standard input... Use '--help' for help.\n");
           
        if (S.verbosity > 0){
            printf("============================[ Problem Statistics ]=============================\n");
            printf("|                                                                             |\n"); }

        fs::path agnf_path = fs::current_path().append(std::string{argv[1]});
        Polygraph *polygraph = construct(agnf_path, S); // TODO: may integrate polygraph into solver

#ifdef MONITOR_ENABLED
#endif

        AcyclicSolverHelper *solver_helper = nullptr;
        try {
            solver_helper = new AcyclicSolverHelper(polygraph);
        } catch (std::runtime_error &e) {
            std::cout << "UNSAT" << std::endl;
            return 0;
        }
        S.init(solver_helper);

        FILE* res = (argc >= 3) ? fopen(argv[2], "wb") : NULL;
        
        if (S.verbosity > 0){
            printf("|  Number of variables:  %12d                                         |\n", S.nVars());
            printf("|  Number of clauses:    %12d                                         |\n", S.nClauses()); }
        
        double parsed_time = cpuTime();
        if (S.verbosity > 0){
            printf("|  Parse time:           %12.2f s                                       |\n", parsed_time - initial_time);
            printf("|                                                                             |\n"); }
 
        // Change to signal-handlers that will only notify the solver and allow it to terminate
        // voluntarily:
        sigTerm(SIGINT_interrupt);
       
        if (!S.simplify()){
            if (res != NULL) fprintf(res, "UNSAT\n"), fclose(res);
            if (S.verbosity > 0){
                printf("===============================================================================\n");
                printf("Solved by unit propagation\n");
                S.printStats();
                printf("\n"); }
            printf("UNSAT\n");
            return 0;
        }
        
        vec<Lit> dummy;
        bool ret = S.solve();
        std::cout << (ret ? "SAT" : "UNSAT") << std::endl;
        // if (S.verbosity > 0){
        //     S.printStats();
        //     printf("\n"); }
        // printf(ret == l_True ? "SATISFIABLE\n" : ret == l_False ? "UNSATISFIABLE\n" : "INDETERMINATE\n");
        // if (res != NULL){
        //     if (ret == l_True){
        //         fprintf(res, "SAT\n");
        //         for (int i = 0; i < S.nVars(); i++)
        //             if (S.model[i] != l_Undef)
        //                 fprintf(res, "%s%s%d", (i==0)?"":" ", (S.model[i]==l_True)?"":"-", i+1);
        //         fprintf(res, " 0\n");
        //     }else if (ret == l_False)
        //         fprintf(res, "UNSAT\n");
        //     else
        //         fprintf(res, "INDET\n");
        //     fclose(res);
        // }

        // free all auxliliary data structures
        delete polygraph;

#ifdef MONITOR_ENABLED
        Monitor::get_monitor()->show_statistics();
#endif

        return 0;
    } catch (OutOfMemoryException&){
        printf("===============================================================================\n");
        printf("INDETERMINATE\n");
        exit(0);
    }
}
