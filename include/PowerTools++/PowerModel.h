//
//  PowerModel.h
//  PowerTools++
//
//  Created by Hassan Hijazi on 30/01/2015.
//  Copyright (c) 2015 NICTA. All rights reserved.
//

#ifndef __PowerTools____PowerModel__
#define __PowerTools____PowerModel__

#include <stdio.h>
#include <PowerTools++/Model.h>
#include <PowerTools++/Solver.h>
#include <PowerTools++/Net.h>
#include <PowerTools++/Constraint.h>
#include <PowerTools++/meta_constant.h>
#include <PowerTools++/meta_var.h>
#include <PowerTools++/meta_Constraint.h>

typedef enum { ACPOL, ACRECT, QC, OTS, DF, SOCP, DC, QC_OTS_L, QC_OTS_N, QC_OTS_O, SOCP_OTS, GRB_TEST } PowerModelType;
typedef enum { MinCost, MinLoss, MinDelta, MaxDelta } Obj;

class PowerModel {
public:
    PowerModelType      _type;
    Obj                 _objective;
    Model*              _model;
    Solver*             _solver;
    Net*                _net;
    SolverType          _stype;
    
    PowerModel();
    PowerModel(PowerModelType type, Net* net, SolverType stype);
    ~PowerModel();
    void reset();
    void build();
    void run_grb_lin_test();
    void run_grb_quad_test();
    /** Accessors */
    Function* objective();
    /** Variables */
    void add_AC_gen_vars();
    void add_AC_Pol_vars();
    void add_AC_Rect_vars();
    void add_AC_OTS_vars();
    void add_AC_SOCP_vars();
    void add_DC_vars();
    void add_SOCP_OTS_vars();
    /** Constraints */
    void add_AC_thermal(Arc* a, bool switch_lines);
    void add_AC_Angle_Bounds(Arc* a, bool switch_lines);
    void add_AC_Voltage_Bounds(Node* n);
    void add_AC_Power_Flow(Arc* a, bool polar);
    void add_AC_KCL(Node* n, bool switch_lines);
    /** Operators */
    Function sum(vector<Arc*> vec, string var, bool switch_lines);
    Function sum(vector<Gen*> vec, string var);
    
    /** Models */
    void post_AC_Polar();
    void post_AC_Rect();
    void post_AC_SOCP();
    void post_AC_OTS();
    void post_DC();
    void post_SOCP_OTS();
    /** Solve */
    void solve(int output = 1,bool relax = false);
    void min_cost();
    void min_var(var<> v);
    void max_var(var<> v);
    void min_cost_load();
    void print();
    
};

#endif /* defined(__PowerTools____PowerModel__) */
