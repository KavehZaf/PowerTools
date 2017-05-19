//
//  PTSolver.cpp
//  PowerTools++
//
//  Created by Hassan on 30/01/2015.
//  Copyright (c) 2015 NICTA. All rights reserved.
//

#include "PowerTools++/PTSolver.h"

#ifdef USE_BONMIN
#include <coin/BonBonminSetup.hpp>
#include <coin/BonCbc.hpp>
#endif

namespace {
    void gurobiNotAvailable()
    {
        cerr << "Can't use Gurobi for solver: this version of PowerTools "
        "was compiled without Gurobi support." << endl;
        exit(1);
    }
    void bonminNotAvailable()
    {
        cerr << "Can't use Bonmin for solver: this version of PowerTools "
        "was compiled without Bonmin support." << endl;
        exit(1);
    }
}


PTSolver::PTSolver(Model* model, SolverType stype){
    _stype = stype;
    switch (stype) {
        case ipopt:
            prog.ipopt_prog = new IpoptProgram(model);
            break;
        case gurobi:
#ifdef USE_GUROBI
            try{
                prog.grb_prog = new GurobiProgram(model);
            }catch(GRBException e) {
                cerr << "\nError code = " << e.getErrorCode() << endl;
                cerr << e.getMessage() << endl;
                exit(1);
            }
#else
            gurobiNotAvailable();
#endif
            break;
        case bonmin:
#ifdef USE_BONMIN
            prog.bonmin_prog = new BonminProgram(model);
#else
            bonminNotAvailable();
#endif
            break;
    }
}

PTSolver::~PTSolver(){
    if (_stype == gurobi) {
#ifdef USE_GUROBI
        delete prog.grb_prog;
#else
        gurobiNotAvailable();
#endif        
    }
    if (_stype == ipopt) delete prog.ipopt_prog;
}

void PTSolver::set_model(Model* m) {
    
    if (_stype == gurobi){
#ifdef USE_GUROBI
        prog.grb_prog->model = m;
#else
        gurobiNotAvailable();
#endif
    }
    if (_stype == ipopt) prog.ipopt_prog->model = m;
}


int PTSolver::run(int output, bool relax){
    //GurobiProgram* grbprog;
    // Initialize the IpoptApplication and process the options

    if (_stype==ipopt) {
            IpoptApplication iapp;
            ApplicationReturnStatus status = iapp.Initialize();
            if (status != Solve_Succeeded) {
                std::cout << std::endl << std::endl << "*** Error during initialization!" << std::endl;
                return (int) status;
            }

        if(prog.ipopt_prog->model->_objt==maximize){
            *prog.ipopt_prog->model->_obj *= -1;
        }
        SmartPtr<TNLP> tmp = new IpoptProgram(prog.ipopt_prog->model);
//        prog.ipopt_prog;
            //            iapp.Options()->SetStringValue("hessian_constant", "yes");
//                        iapp.Options()->SetStringValue("derivative_test", "second-order");
            //            iapp->Options()->SetNumericValue("tol", 1e-6);
//                        iapp.Options()->SetNumericValue("tol", 1e-6);
            //            iapp->Options()->SetStringValue("derivative_test", "second-order");
            //            iapp.Options()->SetNumericValue("bound_relax_factor", 0);
            //            iapp.Options()->SetIntegerValue("print_level", 5);
            
            //            iapp.Options()->SetStringValue("derivative_test_print_all", "yes");
        status = iapp.OptimizeTNLP(tmp);
        if(prog.ipopt_prog->model->_objt==maximize){
            *prog.ipopt_prog->model->_obj *= -1;
        }
            if (status == Solve_Succeeded) {
                // Retrieve some statistics about the solve
                
                //                printf("\n\nSolution of the primal variables:\n");
                //                _model->print_solution();
//                return status;
                return 100;
            }
        if (status == Solved_To_Acceptable_Level) {
            return 150;
        }
    }
    else if(_stype==gurobi)
    {
#ifdef USE_GUROBI
        try{
            //                prog.grbprog = new GurobiProgram();
            prog.grb_prog->_output = output;
            prog.grb_prog->reset_model();
            prog.grb_prog->prepare_model();
            bool ok = prog.grb_prog->solve(relax);
            return ok ? 100 : -1;
        }catch(GRBException e) {
            cerr << "\nError code = " << e.getErrorCode() << endl;
            cerr << e.getMessage() << endl;
        }
#else
        gurobiNotAvailable();
#endif
    }
    else if(_stype==bonmin) {
#ifdef USE_BONMIN
        if(prog.bonmin_prog->model->_objt==maximize){
            *prog.bonmin_prog->model->_obj *= -1;
        }

        bool ok = true;
        using namespace Bonmin;
        BonminSetup bonmin;
        bonmin.initializeOptionsAndJournalist();
        bonmin.initialize(prog.bonmin_prog);
        try {
            Bab bb;
            bb(bonmin);
            auto status = bb.mipStatus();
            switch (bb.mipStatus())
            {
                case Bab::MipStatuses::FeasibleOptimal:
                case Bab::MipStatuses::Feasible:
                    ok = true;
                    break;
                default:
                    ok = false;
            }
        }
        catch(TNLPSolver::UnsolvedError *E) {
            //There has been a failure to solve a problem with Bonmin.
            std::cerr << "Bonmin has failed to solve a problem" << std::endl;
            ok = false;
        }
        catch(OsiTMINLPInterface::SimpleError &E) {
            std::cerr << E.className() << "::" << E.methodName() << std::endl << E.message() << std::endl;
            ok = false;
        }
        catch(CoinError &E) {
            std::cerr << E.className() << "::" << E.methodName() << std::endl << E.message() << std::endl;
            ok = false;
        }
        catch(...) {
            std::cerr << "Unknown error: Bonmin failed to solve the problem." << std::endl;
            ok = false;
        }

        if(prog.bonmin_prog->model->_objt==maximize){
            *prog.bonmin_prog->model->_obj *= -1;
        }

        return ok ? 100 : -1;
#else
        bonminNotAvailable();
#endif
    }
    return -1;
}
