//
//  Line.h
//  Cycle_Basis_PF
//
//  Created by Sumiran on 16/06/2014.
//  Copyright (c) 2014 NICTA. All rights reserved.
//

#ifndef Cycle_Basis_PF_Line_h
#define Cycle_Basis_PF_Line_h
#include <PowerTools++/Complex.h>
#include <PowerTools++/Bound.h>


class Line {
public:
    int id;
    std::string _name;
    double limit;
    double ch;
    double tr;
    double as;
    double r;
    double x;
    double g;
    double b;
    double cc;
    double dd;
    double smax;
    
    int status;
    Bound tbound;
    Complex _Si_;
    Complex _Sj_;
    Complex _W_;

    var<>   wr;
    var<>   wi;
    var<>   pi;
    var<>   qi;
    
    vector<var<>>   pi_t;
    vector<var<>>   qi_t;
    vector<var<>>   pj_t;
    vector<var<>>   qj_t;
    vector<var<>>   wr_t;
    vector<var<>>   wi_t;
    vector<var<>>   p_loss;



    var<>   pj;
    var<>   qj;
    var<>   vcs;    /** vi*vj*cs */
    var<>   vsn;    /** vi*vj*sn */
    var<>   vv;     /** vi*vj */
    var<>   cs;     /** cos(thetai - thetaj - as) */
    var<>   sn;     /** sin(thetai - thetaj - as) */
    var<>   ci;     /** square of the current magnitude */
    var<>   w_line_ij;
    var<>   w_line_ji;
    var<>   delta;
    var<bool> on;
    
    Line();
    Line(std::string name);
    ~Line();
    
    void init_complex();

    void print();

};

#endif
