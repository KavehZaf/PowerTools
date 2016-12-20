//
//  Net.cpp
//  Cycle_Basis_PF
//
//  Created by Sumiran on 17/06/2014.
//  Copyright (c) 2014 NICTA. All rights reserved.
//

#include "PowerTools++/Net.h"
#include "PowerTools++/Node.h"
#include "PowerTools++/Bus.h"
#include "PowerTools++/Path.h"
#include <algorithm>
#include <map>
#include "PowerTools++/PowerModel.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include <list>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <queue>
#include <time.h>
#include <sstream>
#include <cstdlib>

using namespace std;

Net::Net(){
    bMVA=0;
    horton_net = nullptr;
    _clone = nullptr;
    _bags = nullptr;
}

void Net::add_node(Node* node){
    node->ID = (int)nodes.size();
    if(!nodeID.insert(pair<string,Node*>(node->_name, node)).second){
        cerr << "ERROR: adding the same bus twice!";
    }
    nodes.push_back(node);
}

/** Returns true if the cycle in the bag is rotated in one direction only */
bool Net::rotation_bag(vector<Node*>* bag){
    assert(bag->size()==3);
    Node* n1 = bag->at(0);
    Node* n2 = bag->at(1);
    Node* n3 = bag->at(2);
    if (_clone->has_directed_arc(n1, n2) &&  _clone->has_directed_arc(n1,n3)){
        return false;
    }
    if (_clone->has_directed_arc(n2, n1) &&  _clone->has_directed_arc(n2,n3)){
        return false;
    }
    if (_clone->has_directed_arc(n3, n1) &&  _clone->has_directed_arc(n3,n2)){
        return false;
    }
    
    return true;
}


bool Net::add_arc(Arc* a){
    bool parallel =false;
    set<Arc*>* s = NULL;
    string src, dest, key, inv_key;
    src = a->src->_name;
    dest = a->dest->_name;
    key.clear();
    inv_key.clear();
    key.append(src);
    inv_key.append(dest);
    key.append(",");
    inv_key.append(",");
    key.append(dest);
    inv_key.append(src);
    if(lineID.find(key)==lineID.end() && lineID.find(inv_key)==lineID.end()){
        s = new set<Arc*>;
        s->insert(a);
        lineID.insert(pair<string, set<Arc*>*>(key,s));
    }
    else {
        if(lineID.find(key)!=lineID.end())
            s = lineID[key];
        if(lineID.find(inv_key)!=lineID.end())
            s = lineID[inv_key];
        s->insert(a);
//        cout << "\nWARNING: adding another line between same nodes!\n";
        a->parallel = true;
        parallel = true;
        
    }
    arcs.push_back(a);
    return parallel;
}


void Net::remove_arc(Arc* a){
//    arcs.erase(arcs.at(a->id));
    arcs[a->id] = nullptr;
    lineID.erase(a->src->_name+","+a->dest->_name);
}

/* Destructors */

Net::~Net(){
    if (!nodes.empty()) {
        for (vector<Node*>::iterator it = nodes.begin(); it != nodes.end(); it++){
                delete (*it);
        }
        nodes.clear();
    }
    if(!arcs.empty()){
        for (vector<Arc*>::iterator it = arcs.begin(); it != arcs.end(); it++){
            if(*it)
                delete (*it);
        }
        arcs.clear();
    }
    if(!gens.empty()){
        for (Gen* g:gens){
            delete g;
        }
        gens.clear();
    }
    if(!cycle_basis.empty()){
        for (vector<Path*>::iterator it = cycle_basis.begin(); it != cycle_basis.end(); it++){
                delete (*it);
        }
        arcs.clear();
    }
    for (pair<string,set<Arc*>*> it:lineID) {
        delete it.second;
    }
    if (horton_net!=NULL)
        delete horton_net;
    if (_clone) {
        delete _clone;
    }
    if (_bags) {
        for(auto b:*_bags){
            delete b;
        }
        delete _bags;
    }
}

const bool node_compare(const Node* n1, const Node* n2) {
    return n1->fill_in > n2->fill_in;
}

/* return true if there is an arc (n1,n2) */
bool Net::has_directed_arc(Node* n1, Node* n2){
    Arc* a = get_arc(n1, n2);
        if (n1->ID == a->src->ID && n2->ID==a->dest->ID) {
            return true;
        }
    return false;
}


void Net::get_tree_decomp_bags(){
    _bags = new vector<vector<Node*>*>();
    vector<Node*>* bag = nullptr;
    Node* n = nullptr;
    Node* u = nullptr;
    Node* nn = nullptr;
    Arc* arc = nullptr;
    int id = 0;
    int nb3 = 0;
    while (_clone->nodes.size()>2) {
        sort(_clone->nodes.begin(), _clone->nodes.end(), node_compare);
        n = _clone->nodes.back();
        bag = new vector<Node*>();
//        cout << "new bag = { ";
        for (auto a: n->branches) {
            nn = a->neighbour(n);
            bag->push_back(nn);
//            cout << nn->_name << ", ";
        }
        _clone->remove_end_node();
        for (int i = 0; i<bag->size(); i++) {
            u = bag->at(i);
            for (int j = i+1; j<bag->size(); j++) {
                nn = bag->at(j);
                if (u->is_connected(nn)) {
                    continue;
                }
                id = (int)_clone->arcs.size() + 1;
                //arc = new Arc(u->_name+nn->_name);
                arc = new Arc(to_string(id));
                arc->id = id;
                arc->src = u;
                arc->dest = nn;
                arc->tbound.min = -1.4999;
                arc->tbound.max = 1.4999;
                arc->connect();
                _clone->add_arc(arc);
            }
        }
        bag->push_back(n);
//        cout << n->_name << "}\n";
        _bags->push_back(bag);
        if (bag->size()==3) {
            nb3++;
        }
    }
//    cout << "\nNumber of 3D bags = " << nb3 << endl;
}


/* returns true if an arc is already present between the given nodes */
bool Net::duplicate(int n1, int n2, int id1){
    int id2 = get_arc(n1, n2)->id;
    if (id2 < id1)
        return true;
    else return false;
}

/* Clone function */
Net* Net::clone(){
    Net *copy_net = new Net();
    Node* node = NULL;
    for (int i=0; i<nodes.size(); i++) {
        node = this->nodes[i];
        copy_net->add_node(node->clone());
    }
    Arc* arc = NULL;
    for (int i=0; i<arcs.size(); i++){
        /* ignores if the arc is a paralel line to an already existing line */
        if (duplicate(arcs[i]->src->ID, arcs[i]->dest->ID, arcs[i]->id)) {
            continue;
        }
        arc = arcs[i]->clone();
        /* Update the source and destination to the new nodes in copy_net */
        arc->src = copy_net->get_node(arc->src->_name);
        arc->dest = copy_net->get_node(arc->dest->_name);
        /* Add the new arc to the list of arcs IS THIS REALLY USEFULL ? */
        copy_net->add_arc(arc);
        /* Connects it to its source and destination */
        arc->connect();
    }
    return copy_net;
}

Node* Net::get_node(string id){
    return nodeID.find(id)->second;
}

/* Combines the src node with the path p to form a cycle */
void Net::combine(Node* src, Path* p){
    p->nodes.insert(p->nodes.begin(), src);
    p->nodes.push_back(src);
}



/*  @brief Remove node and all incident arcs from the network
    @note Does not remove the incident arcs from the list of arcs in the network!
    @return the id of the node removed
 */
string Net::remove_end_node(){
    Node* n = nodes.back();
    Node * nn = nullptr;
    string n_id = n->_name;
    for (auto a: n->branches) {
        nn = a->neighbour(n);
        nn->removeArc(a);
        for (auto aa: nn->branches){
            if (!aa->neighbour(nn)->is_connected(n)) {
                nn->fill_in--;
                assert(nn->fill_in >=0);
            }
        }
    }
    nodes.pop_back();
    return n_id;
}

/* Reset all distances to max value */
void Net::resetDistance(){
    for (vector<Node*>::iterator it = nodes.begin(); it != nodes.end(); it++){
        (*it)->distance = (int)nodes.size()+1;
        (*it)->predecessor = NULL;
    }
}

/* sets the cycle member fo all odes to be false */
void Net::reset_nodeCycle(){
    vector<Node*>::iterator it = nodes.begin();
    while (it != nodes.end()) {
        (*it)->cycle = false;
        it++;
    }
}

/* sets all nodesto unexplored */
void Net::reset_nodeExplored(){
    vector<Node*>::iterator it = nodes.begin();
    while (it != nodes.end()) {
        (*it)->explored = false;
        it++;
    }
}

/* returns the arc formed by node ids n1 and n2 */
Arc* Net::get_arc(Node* n1, Node* n2){
    string src, dest, key, inv_key;
    src = n1->_name;
    dest = n2->_name;
    key.clear();
    inv_key.clear();
    key.append(src);
    inv_key.append(dest);
    key.append(",");
    inv_key.append(",");
    key.append(dest);
    inv_key.append(src);
    map<string, set<Arc*>*>::iterator it= lineID.find(key);
    if (it != lineID.end()) {
        for (auto a: *it->second){
            if (!a->parallel) {
                return a;
            }
        }
    }
    it = lineID.find(inv_key);
    if (it != lineID.end()) {
        for (auto a: *it->second){
            if (!a->parallel) {
                return a;
            }
        }
    }
    return nullptr;
}



/* returns the Id of the arc formed by node ids n1 and n2 */
Arc* Net::get_arc(int n1, int n2){
    string src, dest, key, inv_key;
    src = to_string(n1);
    dest = to_string(n2);
    key.clear();
    inv_key.clear();
    key.append(src);
    inv_key.append(dest);
    key.append(",");
    inv_key.append(",");
    key.append(dest);
    inv_key.append(src);
    map<string, set<Arc*>*>::iterator it= lineID.find(key);
    if (it != lineID.end()) {
        for (auto a: *it->second){
            if (!a->parallel) {
                return a;
            }
        }
    }
    it = lineID.find(inv_key);
    if (it != lineID.end()) {
        for (auto a: *it->second){
            if (!a->parallel) {
                return a;
            }
        }
    }
    return nullptr;
}


bool Net::has_arc(int n1, int n2){
    string src, dest, key, inv_key;
    src = to_string(n1);
    dest = to_string(n2);
    key.clear();
    inv_key.clear();
    key.append(src);
    inv_key.append(dest);
    key.append(",");
    inv_key.append(",");
    key.append(dest);
    inv_key.append(src);
    map<string, set<Arc*>*>::iterator it= lineID.find(key);
    if (it != lineID.end()) {
        return true;
    }
    it = lineID.find(inv_key);
    if (it != lineID.end()) {
        return true;
    }
    return false;
}


/* finds arc paralel to the given arc and prints the self loop */
int Net::self_loops(ofstream &myfile, Arc* arc, int cycle_index){
    constraint next;
    double num, den;
    arc->in_cycle = true;
    int id1 = arc->id;
    int src = arc->src->ID;
    int dest = arc->dest->ID;
    den = (arc->r)*(arc->r) + (arc->x)*(arc->x);
    int id2;
    for (int i=id1; i<arcs.size(); i++) {
        if ((arcs[i]->src->ID == src && arcs[i]->dest->ID == dest) || (arcs[i]->src->ID == dest && arcs[i]->dest->ID == src)){
            id2 = arcs[i]->id;
            num = (arcs[i]->r)*(arcs[i]->r) + (arcs[i]->x)*(arcs[i]->x);
            arcs[i]->in_cycle = true;
            myfile << "set cycle[" << cycle_index++ << "] :=\n(" << id1 << ", " << src << ", " << dest << ")\n(" << id2 << ", " << dest << ", " << src << ")\n;" << endl;
            next.arr[0] = src, next.arr[1] = id1, next.arr[2] = dest, next.arr[3] = id2, next.arr[4] = dest;
            next.cnst = num/den;
            new_constraints.push_back(next);
            next.arr[0] = dest, next.arr[1] = id1, next.arr[2] = src, next.arr[3] = id2, next.arr[4] = src;
            next.cnst = den/num;
            new_constraints.push_back(next);
        }
    }
    return cycle_index;
}

/* writes path p on myfile */
void Net::write(ofstream &myfile, Path* p, int cycle_index){
    int src, dest;
    int id;
    list<Node*>::iterator n = p->nodes.begin();
    (*n)->cycle = true;
    src = (*n)->ID;
    n++;
    (*n)->cycle = true;
    dest = (*n)->ID;
    id = get_arc(src, dest)->id;
    arcs[id-1]->in_cycle = true;
    myfile << "set cycle[" << cycle_index+1 << "] :=\n(" << id << ", " << src << ", " ;
    n++;
    while (n != p->nodes.end()) {
        (*n)->cycle = true;
        src = dest;
        dest = (*n)->ID;
        id = get_arc(src, dest)->id;
        arcs[id-1]->in_cycle = true;
        myfile << src << ")\n(" << id << ", " << src << ", " ;
        n++;
    }
    n--;
    (*n)->cycle = true;
    myfile << (*n)->ID << ")\n;" << std::endl;
}

/* Compare function for Dijkstra's pripority queue based on the distance from the source */
struct comparator {
    bool operator() (Node* arg1, Node* arg2) {
        return (*arg1).distance > (*arg2).distance;
    }
};

/* Computes and returns the shortest path between src and dest in net */
Path* Net::Dijkstra(Node* src, Node* dest, Net* net){
    priority_queue<Node*, vector<Node*> , comparator> p_queue;
    net->resetDistance();
    src->distance = 0;
    p_queue.push(src);
    Node* current = NULL;
    Arc* arc = NULL;
    while (!p_queue.empty() && current != dest) {
		current = p_queue.top();
		p_queue.pop();
        if (current != dest) {
			for (vector<Arc*>::iterator it = current->branches.begin(); it != current->branches.end(); ++it) {
                arc = (*it);
				if (arc->neighbour(current)->distance > current->distance + 1) {
					arc->neighbour(current)->distance = current->distance + 1;
                    arc->neighbour(current)->predecessor = current;
					p_queue.push(arc->neighbour(current));
				}
			}
		}
        /* Stop when reaching destination */
        else
            break;
    }

    /* If there is no path between src and dest */
    if(dest->predecessor==NULL)
        return NULL;
    /* Reconstruct path backward */
    Path* p = new Path();
    current = dest;
    while (current) {
        /* The path should contain a pointer to the original nodes not the current copy */
        Node* n = get_node(current->_name);
        p->nodes.push_front(n);
        current = current->predecessor;
    }
    return p;
}


void Net::add_horton_nodes(Net* net){
    Node* copy = NULL;
    /* Add all neighbours of the end node (lowest degree) */
    Node* end_n = net->nodes.back();
    vector<Arc*>::iterator it = end_n->branches.begin();
    while(it != end_n->branches.end()){
        copy = (*it)->neighbour(end_n)->clone();
        net->horton_net->add_node(copy);
        it++;
    }
}


void Net::add_horton_branches(Net* net){
    Node* src;
    Node* dest;
    Path* shortest;
    Arc* arc;
    for (int i=0; i<net->horton_net->nodes.size()-1; i++) {
        src = net->horton_net->nodes[i];
        for (int j=i+1; j<net->horton_net->nodes.size(); j++) {
            dest = net->horton_net->nodes[j];
            /* computing shortest paths between neighbours of x in G-x */
            shortest = Dijkstra(net->get_node(src->_name), net->get_node(dest->_name), net);
            if (shortest!=NULL) {
                arc = new Arc(src, dest);
                arc->horton_path = shortest;
                arc->weight = shortest->length();
                net->horton_net->add_arc(arc);
            }
        }
    }
}

/* Compare function for fast_horton algorithm, ranking nodes in decreasing degree */
bool compareNodes(Node* n1, Node* n2){
    return n1->degree() > n2->degree();
}

/* Compare function for minimal_spanning_tree algorithm, ranking arcs in decreasing wheight */
bool compareArcs(Arc* a1, Arc* a2){
    return a1->weight > a2->weight;
}

/* Sort nodes in decreasing degree */
void Net::orderNodes(Net* net){
    if(!net->nodes.empty())
        sort(net->nodes.begin(), net->nodes.end(), compareNodes);
}


/* Sort nodes in decreasing degree */
void Net::orderArcs(Net* net){
    if(!net->arcs.empty())
        sort(net->arcs.begin(), net->arcs.end(), compareArcs);
}

/* Erase Horton network and free memory for cloned nodes and created arcs */
void Net::clear_horton_net(){
    if (!horton_net->nodes.empty()) {
        for (vector<Node*>::iterator it = horton_net->nodes.begin(); it != horton_net->nodes.end(); ++it) {
            delete(*it);
        }
        horton_net->nodes.clear();
    }
    if (!horton_net->arcs.empty()) {
        for (vector<Arc*>::iterator it = horton_net->arcs.begin(); it != horton_net->arcs.end(); ++it) {
            delete(*it);
        }
        horton_net->arcs.clear();
    }

    horton_net->nodeID.clear();

}

/*  @brief Computes a spanning tree of minimal weight in horton's network, then adds the corresponding cycles to the original network by connecting to src
    @note Implements Kruskal’s greedy algorithm
 */
void Net::minimal_spanning_tree(Node* src, Net* net){
    
    /* Check if the network has atleast one line */
    if (net->arcs.empty()) {
        return;
    }
    
    Path* min_tree = new Path();
    int n = (int)net->nodes.size();
    orderArcs(net);
    Arc* a = NULL;
    a = net->arcs.back();
    min_tree->nodes.push_back(a->src);
    while (min_tree->nodes.size()<n) {
        a = net->arcs.back();
        /* Check if at least one node of the arc is not already in the path, in order to avoid creating a cycle */
        if(find(min_tree->nodes.begin(), min_tree->nodes.end(), a->src) == min_tree->nodes.end() && find(min_tree->nodes.begin(), min_tree->nodes.end(), a->dest) == min_tree->nodes.end()){
            min_tree->nodes.push_back(a->src);
            min_tree->nodes.push_back(a->dest);
            combine(src, a->horton_path);
            cycle_basis.push_back(a->horton_path);
        }
        else if(find(min_tree->nodes.begin(), min_tree->nodes.end(), a->src) == min_tree->nodes.end()){
            min_tree->nodes.push_back(a->src);
            combine(src, a->horton_path);
            cycle_basis.push_back(a->horton_path);
        }
        else if (find(min_tree->nodes.begin(), min_tree->nodes.end(), a->dest) == min_tree->nodes.end()){
            min_tree->nodes.push_back(a->dest);
            combine(src, a->horton_path);
            cycle_basis.push_back(a->horton_path);
        }
        net->arcs.pop_back();
    }
    delete min_tree;
}

void Net::Fast_Horton(Net *net){
    
    
    /* Arranging nodes in descending order of degree */
    orderNodes(net);
    
    /* Removing all nodes of degree 1 */
    while (!net->nodes.empty() && net->nodes.back()->degree()<2) {
        net->remove_end_node();
        orderNodes(net);
    }

    /* net has no cycles */
    if(net->nodes.size()<3)
        return;
    
    /* Erase Horton network */
    net->clear_horton_net();

    
    /* Adding nodes to Horton network */
    add_horton_nodes(net);
    
    /* Removing the end node from the original network */
    string n_id = net->remove_end_node();
    
    /* Computing the shortest paths among neighbours of n and creating the corresponding horton network */
    add_horton_branches(net);
    
    /* Computing the minimal spanning tree on the corresponding Horton network */

    minimal_spanning_tree(get_node(n_id), net->horton_net);

    while (net->nodes.size()>2)
        Fast_Horton(net);
}

/*checks neighbours of a gen bus if present in any cycle*/
bool Net::check_neighbour(Node* n){
    bool check = false;
    for (vector<Arc*>::iterator it = n->branches.begin(); it != n->branches.end(); it++) {
        Node* neigh = (*it)->neighbour(n);
        if (!neigh->explored) {
        neigh->explored = true;
            if(neigh->_has_gen) {
                check = true;
                if (neigh->cycle) {
                    neigh->cg = true;
                }
                break;
            }
           check = check_neighbour(neigh);
            break;
        }
    }
    return check;
}

/*identifies the generator bus in the cycle*/
void Net::s_flowIn_cycle(Path* p){
    reset_nodeCycle();
    list<Node*>::iterator it = p->nodes.begin();
    while (it != --p->nodes.end() && (*it)->explored == false) {
        reset_nodeExplored();
        if((*it)->_has_gen){
            (*it)->explored = true;
            (*it)->cg = true;
            it++;
            continue;
        }
        /* to check if the neighbours of node in cycle for presence of gen */
        for (vector<Arc*>::iterator it1 = (*it)->branches.begin(); it1 != (*it)->branches.end(); it1++) {
            if(!(*it1)->in_cycle){
                Node* neigh = (*it1)->neighbour((*it));
                if (!neigh->explored) {
                    neigh->explored = true;
                    if(neigh->_has_gen) {
                        (*it)->cg = true;
                        if (neigh->cycle) {
                            neigh->cg = true;
                        }
                        break;
                    }
                    (*it)->cg = check_neighbour(neigh);
                }
            }
        }
        it++;
    }
}

/* computes the constant for each gen node in each cycle */
double Net::compute_constant(Path* cycle){
    double numi, deni, numr, denr;
    double t1i, t2i, t1r, t2r;
    double loadp, loadq;
    numi=0, deni=0, numr=0, denr=0;
    t1i=0;
    t1r=0;
    int id1, id2, ID1, ID2;
    ID1 = cycle->nodes.front()->ID;
    list<Node*>::iterator it1, it2;
    for (it1 = ++cycle->nodes.begin(), it2 = --cycle->nodes.end(); it1 != it2; it1++) {
        loadp = (*it1)->pl()/bMVA;
        loadq = (*it1)->ql()/bMVA;
        ID2 = (*it1)->ID;
        Arc* line = get_arc(ID1,ID2);
        ID1 = ID2;
        t1i = line->x + t1i;
        t1r = line->r + t1r;
        t2r=0;
        t2i=0;
        id1 = ID2;
        if(loadp != 0 || loadq != 0){
            list<Node*>::iterator it2 = find(cycle->nodes.begin(), cycle->nodes.end(), (*it1));
            for (++it2; it2 != cycle->nodes.end();it2++){
                id2 = (*it2)->ID;
                line = get_arc(id1, id2);
                id1 = id2;
                t2i = line->x + t2i;
                t2r = line->r + t2r;
            }
            denr = t1r*loadp - t1i*loadq + denr;
            numr = t2r*loadp - t2i*loadq + numr;
            deni = t1r*loadq + t1i*loadp + deni;
            numi = t2r*loadq + t2i*loadp + numi;
        }
    }
    double cnst = (numr*numr + numi*numi)/(denr*denr + deni*deni);
    return cnst;
}

/* rearranges cycle according to gen node */
Path* Net::form_cycle(Node* n, Path* p){
    Path* newPath = p->clone();
    list<Node*>::iterator cy;
    newPath->nodes.pop_back();
    cy = --newPath->nodes.end();
    
    list<Node*>::iterator cnd = find(newPath->nodes.begin(), newPath->nodes.end(), n);
    while (cy != cnd){
        newPath->nodes.push_front((*cy));
        newPath->nodes.pop_back();
        cy--;
    }
    newPath->nodes.push_front(n);
    return newPath;
}

/* adds constraint to vector new_constraints */
void Net::addConstraint(int c, Path* cycle){
    constraint New;
    list<Node*>::iterator cy;
    cy = cycle->nodes.begin();
    cy ++;
    int genID = cycle->nodes.front()->ID;
    int next = (*cy)->ID;
    int gen_next_ID = get_arc(genID, next)->id;
    cy = cycle->nodes.end();
    ----cy;
    int prev = (*cy)->ID;
    int gen_prev_ID = get_arc(genID, prev)->id;
    New.cycle_index = c;
    New.arr[0] = genID, New.arr[1] = gen_next_ID, New.arr[2] = next, New.arr[3] = gen_prev_ID, New.arr[4] = prev;
    New.cnst = compute_constant(cycle);
    //if (!isnan(New.cnst)) {
    new_constraints.push_back(New);
   //}
}

/* forms constraint for each cycle in the cycle basis */
void Net::Constraint(){
    Path* cycle;
    Node* gen = NULL;
    int c = 0;
    vector<Path*>::iterator it = cycle_basis.begin();
    while (it != cycle_basis.end()) {
        c++;
        list<Node*>::iterator n = (*it)->nodes.begin();
        list<Node*>::iterator n_end = --(*it)->nodes.end();
        while (n != n_end) {
            if ((*n)->cg) {
                gen = (*n);
                cycle = form_cycle(gen, (*it));
                addConstraint(c, cycle);
                delete cycle;
            }
            n++;
        }
        it++;
    }
}

void Net::Fast_Horton(){
    
    Net* copy_net = clone();
    copy_net->horton_net = new Net();
    
    
    Fast_Horton(copy_net);
    delete(copy_net);
}

/* adds decimal to an integer for precision */
void Net::precise(ofstream &myfile, float f){
    if(floor(f) == f)
        myfile << f << ".0" << "\t";
    else
        myfile << f << "\t";
}

//void Net::writeDAT(string name){
//    cout<<"function running"<<endl;
//    ofstream myfile;
//    myfile.open (name);
//    myfile << "######### Automatically generated DAT file #########\n";
//    myfile << "param ref_bus:= 1;\n";
//    myfile << "param mva_base:= 100.0;\n";
//    myfile << "param theta_bound := " << M_PI/12 << ";\n";
//    
//    myfile << "set buses :=\n";
//    for (int i=0; i<nodes.size(); i++) {
//        myfile << nodes[i]->ID << endl;
//    }
//    myfile << ";" << endl;
//    
//    myfile << "set gens :=\n";
//    for (int i=0; i<gens.size(); i++) {
//        myfile << i << endl;
//    }
//    myfile << ";" << endl;
//    
//    myfile << "set lines :=\n";
//    for (int i=0; i<arcs.size(); i++) {
//        myfile << arcs[i]->id << endl;
//    }
//    myfile << ";" << endl;
//    
//    myfile << "set bus_gen :=\n";
//    for (int i=0; i<gens.size(); i++) {
//        myfile << "( " << gens[i]->ID << " , " << i << " ) " << endl;
//    }
//    myfile << ";" << endl;
//    
//    myfile << "set buses_all :=\n";
//    for (int i=0; i<nodes.size(); i++) {
//        myfile << nodes[i]->ID << endl;
//    }
//    myfile << ";" << endl;
//    
//    myfile << "set gens_all :=\n";
//    for (int i=0; i<gens.size(); i++) {
//        myfile << i << endl;
//    }
//    myfile << ";" << endl;
//    
//    myfile << "set lines_all :=\n";
//    for (int i=0; i<arcs.size(); i++) {
//        myfile << arcs[i]->id << endl;
//    }
//    myfile << ";" << endl;
//    
//    myfile << "set bus_gen_all :=\n";
//    for (int i=0; i<gens.size(); i++) {
//        myfile << "( " << gens[i]->ID << " , " << i << " ) " << endl;
//    }
//    myfile << ";" << endl;
//    
//    myfile << "set arcs_from :=\n";
//    for (int i=0; i<arcs.size(); i++) {
//        myfile << "( " << arcs[i]->id << " , " << arcs[i]->src->ID << " , " << arcs[i]->dest->ID << " ) " << endl;
//    }
//    myfile << ";" << endl;
//    
//    myfile << "set arcs_to :=\n";
//    for (int i=0; i<arcs.size(); i++) {
//        myfile << "( " << arcs[i]->id << " , " << arcs[i]->dest->ID << " , " << arcs[i]->src->ID << " ) " << endl;
//    }
//    myfile << ";" << endl;
//    
//    myfile << "set arcs_from_all :=\n";
//    for (int i=0; i<arcs.size(); i++) {
//        myfile << "( " << arcs[i]->id << " , " << arcs[i]->src->ID << " , " << arcs[i]->dest->ID << " ) " << endl;
//    }
//    myfile << ";" << endl;
//    
//    myfile << "set arcs_to_all :=\n";
//    for (int i=0; i<arcs.size(); i++) {
//        myfile << "( " << arcs[i]->id << " , " << arcs[i]->dest->ID << " , " << arcs[i]->src->ID << " ) " << endl;
//    }
//    myfile << ";" << endl;
//    
//    int tot_self_loops = (int)(arcs.size() - lineID.size());
//    
//    myfile << "########## Cycles in Cycle Basis ############ \n";
//    myfile << "param num_cycles := " << cycle_basis.size()+tot_self_loops << ";\n";
//    for (int i=0; i<cycle_basis.size(); i++) {
//        write(myfile, cycle_basis[i], i);
//        s_flowIn_cycle(cycle_basis[i]);
//    }
//    
//    int cycle_index = (int)cycle_basis.size();
//    map<string, Arc*>::iterator it = lineID.begin();
//    cycle_index++;
//    
//    for (int i=0; i<lineID.size() && cycle_index <= ((int)cycle_basis.size()+tot_self_loops); i++) {
//        cycle_index = self_loops(myfile, (*it).second, cycle_index);
//        it++;
//    }
//    
//    Constraint();
//    
//    myfile << "set new_cycle_constraints:=\n";
//    vector<constraint>::iterator n = new_constraints.begin();
//    while (n != new_constraints.end()) {
//        myfile << "( ";
//        myfile << (*n).cycle_index;
//        for (int i = 0; i<5; i++) {
//            myfile << " , " << (*n).arr[i];
//        }
//        myfile << " )" << endl;
//        cout << (*n).cnst << endl;
//        n++;
//    }
//    new_constraints.clear();
//    myfile << ";" << endl;
//    
//    myfile << "param: load_p load_q v_min v_max shunt_g shunt_b kv_base load_flow_v bus_status :=\n";
//    for (int i=0; i<nodes.size(); i++) {
//        myfile << nodes[i]->ID << "\t" ;
//        precise(myfile, nodes[i]->pl()/bMVA) ;
//        precise(myfile, nodes[i]->ql()/bMVA) ;
//        precise(myfile, nodes[i]->vmin());
//        precise(myfile, nodes[i]->vmax()) ;
//        precise(myfile, nodes[i]->sh_g/bMVA) ;
//        precise(myfile,nodes[i]->sh_b/bMVA) ;
//        myfile << nodes[i]->kv_b << "\t1.0\t1" << endl;
//    }
//    myfile << ";" << endl;
//    
//    myfile << "param: gen_bus c0 c1 c2 p_min p_max q_min q_max load_flow_p gen_status :=\n";
//    for (int i=0; i<gens.size(); i++) {
//        myfile << i << "\t" << gens[i]->ID << "\t" << gens[i]->c0 << "\t" << gens[i]->c1 << "\t" << gens[i]->c2 << "\t" ;
//        precise(myfile, gens[i]->Pmin/bMVA) ;
//        precise(myfile, gens[i]->Pmax/bMVA) ;
//        precise(myfile, gens[i]->Qmin/bMVA) ;
//        precise(myfile, gens[i]->Qmax/bMVA) ;
//        myfile << "0.0\t1" << endl;
//    }
//    myfile << ";" << endl;
//    
//    myfile << "param: from_bus to_bus r x charge s tr as line_status :=\n";
//    for (int i=0; i<arcs.size(); i++) {
////        myfile << arcs[i]->id << "\t" << arcs[i]->src->ID << "\t" << arcs[i]->dest->ID << "\t" << arcs[i]->r << "\t" << arcs[i]->x << "\t" << arcs[i]->ch << "\t" << arcs[i]->S/bMVA << "\t" ;
//        myfile << arcs[i]->id << "\t" << arcs[i]->src->ID << "\t" << arcs[i]->dest->ID << "\t" << arcs[i]->r << "\t" << arcs[i]->x << "\t0.0\t" << arcs[i]->limit/bMVA << "\t" ;
//        precise(myfile,arcs[i]->tr) ;
//        precise(myfile,arcs[i]->as) ;
//        myfile << arcs[i]->status << endl;
//    }
//    myfile << ";" << endl;
//    myfile.close();
//    int c=0;
//    for (int i=0; i<arcs.size(); i++) {
//        if (!arcs[i]->in_cycle) {
//            c++;
//        }
//    }
//    cout<< "not in cycles " << c <<endl;
//}



int Net::readrad(string fname, int _timesteps){
    cout << "Loading file " << fname << endl;
    ifstream file(fname.c_str());
    if(!file.is_open()){
        cout << "Could not open file\n";
        return -1;
    }
    
    
    
    if(file.peek()==EOF)
    {cout << "File is empty" <<endl;
        return 999;
    }
    
    
    string line;
    string word;
    
    getline(file,line);                               //first line to ignore
//    _radiation.push_back(0);
    int _time_count = 0;
    //github weihao//
    while(file.peek()!=EOF)
    {
            getline(file,line);                          //second line store in 'line'

            std::stringstream copy(line);                 //store the whole line into copy
            getline(copy, word, ',');                       //first column to ignore

            getline(copy, word, ',');
            _radiation.push_back(atof(word.c_str()));

            _time_count++;

    }
    int sub_time_count = _time_count/_timesteps; //time count of each division
    float sum_rad_;
    float avg_sub_time_rad;
    vector<double> av_rad;
    
    for (int t = 1; t <= _timesteps; t++) {
        sum_rad_ = 0;
        for (int stc = (t - 1) * sub_time_count; stc < (t - 1) * sub_time_count + sub_time_count; stc++) {
            sum_rad_ += _radiation[stc]; //sum for each division
        }
        avg_sub_time_rad = sum_rad_ / sub_time_count;
        av_rad.push_back(avg_sub_time_rad);
        cout << "average rad " << " ; " << avg_sub_time_rad << endl;
        
    }
    cout << endl;
    _radiation = av_rad;
    
    av_rad.clear();

    return 0;
}





//int Net::readpvmax(string fname){
//    cout << "Loading file " << fname << endl;
//    ifstream file(fname.c_str());
//    if(!file.is_open()){
//        cout << "Could not open file\n";
//        return -1;
//    }
//    
//    
//    
//    if(file.peek()==EOF)
//    {cout << "File is empty" <<endl;
//        return 999;
//    }
//    
//    
//    string line;
//    string word;
//    
//    getline(file,line);                               //first line to ignore
//    int t=0;
//    int i=0;
//    
//    for (auto &n: nodes){
//        n->_cond[0]->_pvmax.push_back(0);                        //pvmax[0]=0
//    }
//    
//    //github weihao//
//    while(file.peek()!=EOF)
//    {
//        t++;                                          //new time interval
//        getline(file,line);                           //second line store in 'line'
//        
//        i=0;                                          //initialize i, new line
//        std::stringstream copy(line);                 //store the whole line into copy
//        getline(copy,word,',');                       //first column to ignore
//        
//        while(getline(copy,word,','))                 //start with the second column
//        {
//            nodes[i]->_cond[0]->_pvmax.push_back(atof(word.c_str())/1000/bMVA);         //*MW
//            i++;
//        }
//    }
//    for (int j = i; j < nodes.size(); j++)
//    {
//        nodes[j]->_cond[0]->_pvmax.push_back(0);
//    }
//    
//}
//
//
//



int Net::readcost(string fname, int _timesteps){
    cout << "Loading file " << fname << endl;
    ifstream file(fname.c_str());
    if(!file.is_open()){
        cout << "Could not open file\n";
        return -1;
    }
    
    
    
    if(file.peek()==EOF)
    {cout << "File is empty" <<endl;
        return 999;
    }
    
 
    string line;
    string word;
    
    getline(file,line);                               //first line to ignore
//    int t=0;
//    
//
//    for (auto &n: gens){
//        n->_timecost->c0.push_back(0);                        //c0[0]=0
//        n->_timecost->c1.push_back(0);                        //c1[0]=0
//        n->_timecost->c2.push_back(0);                        //c2[0]=0
//    }

    int _time_count = 0;
    while(file.peek()!=EOF)
    {
//        t++;                                          //new time interval start from 1
        getline(file,line);                           //second line store in 'line'
        
      
        std::stringstream copy(line);                 //store the whole line into copy
        getline(copy,word,',');                       //first column to ignore
        getline(copy,word,',');                       //c0 original unit:$/kWh
        c0.push_back(atof(word.c_str()));
        
        getline(copy,word,',');                       //c1 original unit:$/kWh   weekday
        c1.push_back(atof(word.c_str()));
        
        getline(copy,word,',');                       //c2 original unit:$/kWh
        c2.push_back(atof(word.c_str()));
        
        getline(copy,word,',');                       //c3 original unit:$/kWh   weekend
        c3.push_back(atof(word.c_str()));

        _time_count++;
//        
//        getline(copy,word,',');                       //c1
//        gens[1]->_timecost->c1.push_back(atof(word.c_str())/1000/bMVA);
//        
//        getline(copy,word,',');                       //c2
//        gens[1]->_timecost->c2.push_back(atof(word.c_str())/1000/bMVA);
//
}
    int sub_time_count = _time_count/_timesteps; //time count of each division
    float sum_gen_0;
    float sum_gen_1;
    float sum_gen_2;
    float sum_gen_3;
    float avg_sub_time_gen_0;
    float avg_sub_time_gen_1;
    float avg_sub_time_gen_2;
    float avg_sub_time_gen_3;
    vector<double> av_gen_0;
    vector<double> av_gen_1;
    vector<double> av_gen_2;
    vector<double> av_gen_3;

    for (int t = 1; t <= _timesteps; t++) {
        sum_gen_0 = 0;
        sum_gen_1 = 0;
        sum_gen_2 = 0;
        sum_gen_3 = 0;
        for (int stc = (t - 1) * sub_time_count; stc < (t - 1) * sub_time_count + sub_time_count; stc++) {
            sum_gen_0 += c0[stc]; //sum for each division
            sum_gen_1 += c1[stc];
            sum_gen_2 += c2[stc];
            sum_gen_3 += c3[stc];
        }
        avg_sub_time_gen_0 = sum_gen_0 / sub_time_count;
        avg_sub_time_gen_1 = sum_gen_1 / sub_time_count;
        avg_sub_time_gen_2 = sum_gen_2 / sub_time_count;
        avg_sub_time_gen_3 = sum_gen_3 / sub_time_count;
        av_gen_0.push_back(avg_sub_time_gen_0);
        av_gen_1.push_back(avg_sub_time_gen_1);
        av_gen_2.push_back(avg_sub_time_gen_2);
        av_gen_3.push_back(avg_sub_time_gen_3);
//        cout << "average cost c0" << " ; " << avg_sub_time_gen_0 << endl;
        cout << "average cost c1" << " ; " << avg_sub_time_gen_1 << endl;
//        cout << "average cost c2 " << " ; " << avg_sub_time_gen_2 << endl;
    }
    cout << endl;
    c0 = av_gen_0;
    c1 = av_gen_1;
    c2 = av_gen_2;
    c3 = av_gen_3;

    av_gen_0.clear();
    av_gen_1.clear();
    av_gen_2.clear();
    av_gen_3.clear();
    
    return 0;

}

/*vector<int> Net::weekday_tag(){
    vector<int>  wkd_tag;
    for(int i=0; i<24; i++){
        push.back();
    }
    return wkd_tag;
}*/




int Net::readload(string fname, int _timesteps){
    cout << "Loading file " << fname << endl;
    ifstream file(fname.c_str());
    if(!file.is_open()){          // if file is not opened
        cout << "Could not open file\n";
        return -1;
    }
    
    if(file.peek()==EOF)          // if peek naxt char is at the end
    {cout << "File is empty" <<endl;
        return 999;
    }
    
    for (auto n : nodes) {         //for all nodes
        n->_cond[0]->_pl.pop_back();                  //_pl: Conductor active power load. remove last element ??????
//        cout << "pl size = " << n->_cond[0]->_pl.size() << endl;
    }
    int i=0;                                         //number of bus, or number of buildings
    
//    int pos=0;
    
    string line;
    string word;
    
    getline(file,line);                               //first line to ignore, call "getline" once, get the first line
//    getline(file,line);                               //second line to ignore
    /*for (int tstep = 0; tstep < 24*5; tstep++ ){
        getline(file,line);
    }*/
    
    
    
    double av = 0;                  //average values,fill the remaining part buildings for average values
    double tot = 0;                 //total power consumed for all buildings in one timestep, /1000/bMVA
    double remain = 0;              //remaining power,(max-tot)+.
    double pl = 0;                  //power load
    double tot_Kw = 0;              //filtered total power consumed for all buildings in one timestep
    int _time_count = 0;            //counts all time instances
    while(file.peek()!=EOF)         //peek next char is not the end
    {
        getline(file,line);                           //third line store in 'line'

        i=0;                                          //initialize i when starting each new line
        std::stringstream copy(line);                 //store the whole line into copy, copy is the string stream
        getline(copy,word,',');                       //first column to ignore
        getline(copy,word,',');
        tag.push_back(atoi(word.c_str()));
       // cout<<"tag is "<< atoi(word.c_str()) <<"; ";
        tot = 0;                    //initialize total power when starting each new line
        tot_Kw = 0;                 //initialize filtered total power when starting each new line
        int n =0;
       
        while(getline(copy,word,','))                 //start with the second column, while each word
        {
//            int length=word.length();
            int len=word.length()-2;                  //erase 'kW'
//            if (len !=2 ) {
//                continue;
//            }
            
            string c=word.substr(0,len);              //new string, only power value
            
          
//            double test = atof(c.c_str());          //simulate pl(i,t)
//            cout << "bus" << i << " pl = ";
            tot_Kw +=  atof(c.c_str());               //convert string to float
            pl = atof(c.c_str())/(1000*bMVA);         //power demand for one building, baseMVA: max power. bMVA=100
            //cout << "base MVA is "<<bMVA<<" ;";
            
            
/*
            if (pl > 0.1) {
                nodes[i]->_cond[0]->_pl.push_back(0);
//                cerr << " 0; ";
            }
            else {
//                cout <<  " " << pl << " ;";
                nodes[i]->_cond[0]->_pl.push_back(pl);         //*MW
            }
 
*/
            if (pl <= 0.05) {                                  //filter buildings with power >5000kW
                cout <<  "pl is " << pl*1000*bMVA << " ;";
                nodes[i]->_cond[0]->_pl.push_back(pl);         //*MW, push the remaining buildings to nodes
                i++;
                n++;
            }
            
            cout <<"number of node is " << n <<" ;" ;
            tot += nodes[i]->_cond[0]->_pl[nodes[i]->_cond[0]->_pl.size()-1];  //*MW. calculate the filtered total power for each timestep
//            i++;  //indicate reading next word (next building)
//            cout << endl;
        }
        
        cout << "Original TOTAL Kw = " << tot_Kw << endl;  //cout for each line (each timestep)
        cout << "TOTAL Kw = " << tot*1000*bMVA << endl;
       // cout << "node = " << n << endl;
        
//        cout << "My TOTAL = " << tot << "__"<< nodes[i]->_cond[0]->_pl.size()-1 << endl;
        remain = fmax(0, 0.1 - tot);              //assumed limit=10MW  (which is the average total consumption (power)...making sure no bus gets negative power
                                                  //*shouble be changed again when the loadfile is accurate*
        
        double tot2 = tot;
    
        av = remain/(nodes.size() - i);          //fill the remaining part buildings for average values
        for (int j = i; j < nodes.size(); j++)
        {
            nodes[j]->_cond[0]->_pl.push_back(av);
            tot2 += av;
        }
//        cout << "TOTAL2 = " << tot2 << endl;
//        for (int j = 0; j< nodes.size(); j++)
//        {cout<<j<<" "<<nodes[j]->_cond[0]->_pl[nodes[i]->_cond[0]->_pl.size()-1]<<endl;}
        
//        t++;
        _time_count++;
     }
    /*if (pl <= 0.05) {//filter buildings with power >5000kW
        cout <<  "pl is " << pl*1000*bMVA << " ;";
        nodes[i]->_cond[0]->_pl.push_back(pl);         //*MW
        //n++;
    }*/
    cout << "total number of timesteps = " << _time_count << endl;
    int sub_time_count = _time_count/_timesteps; //time count of each division
    cout << "subtime = " << sub_time_count << endl;
    float sum_load_;
    float avg_sub_time_load;
    vector<double> av_loads;
    typedef std::pair<int, double> myPair_type;
    
    struct mypair_comp{
        bool operator()(myPair_type const& lhs, myPair_type const& rhs){
            return lhs.second > rhs.second;
        }
    };
    vector<myPair_type> pairs;//order decreasing average load
    double tot_aver = 0;
    for (int i = 0; i < nodes.size(); i++) {
        tot_aver = 0;
        cout << "av load for bus " << i << " = ";
        for (int t = 1; t <= _timesteps; t++) {
            sum_load_ = 0;
                for (int stc = (t-1)*sub_time_count; stc < (t-1)*sub_time_count + sub_time_count; stc++) {
                    sum_load_ += nodes[i]->_cond[0]->_pl[stc]; //sum for each division
                }
            avg_sub_time_load = sum_load_/sub_time_count;
            av_loads.push_back(avg_sub_time_load);
            cout << " ; " << avg_sub_time_load;
            tot_aver += avg_sub_time_load;
        }
        tot_aver /= _timesteps;
        pairs.push_back(make_pair(i, tot_aver));
        cout << endl;
        nodes[i]->_cond[0]->_pl = av_loads;
        av_loads.clear();
    }
    std::sort(pairs.begin(), pairs.end(), mypair_comp());
    for (int i = 0; i < 20; i++) { //first 20 buidling install
        //cout <<"pairs" << pairs[i].first << "  " << pairs[i].second << endl;
        nodes[pairs[i].first]->_cand = true;
    }
    for (auto n:nodes){

        if (n->ID==155){ //Concessions Building Students' Association (ANUSA)
            n->_inst = true;
            n->max_pv_size = (14.28)/(0.218);
        };
        if (n->ID==185){ //Lennox House Block F University Preschool
            n->_inst = true;
            n->max_pv_size = (5.32+4.94)/(0.218); //using lowest radiation,to get max pv size
        }
        /*else if (n->ID==135){ //Lennox House Block G Heritage Preschool
            n->_inst = true;
            n->max_pv_size = (4.94)/(0.218);
        }*/
        else if (n->ID==5){ //Frank Fenner
            n->_inst = true;
            n->max_pv_size = 115;
        }
        /*else if (n->ID==27){ //Lena Karmel
            n->_inst = true;
            n->max_pv_size = (28.56)/(0.218);
        }*/

        else if (n->ID==66){ //Sir Roland Wilson Building
            n->_inst = true;
            n->max_pv_size = 109;
        }
        else if (n->ID==83){ //John Curtin School of Medical Research
            n->_inst = true;
            n->max_pv_size = 178;
        }
        else if (n->ID==64){ //Chifley Library
            n->_inst = true;
            n->max_pv_size = 351;
        }
        else if (n->ID==55){ //Research School of Information Sciences and Engineering
            n->_inst = true;
            n->max_pv_size = 176;
        }
        else if (n->ID==110){ //Law Building / Library
            n->_inst = true;
            n->max_pv_size = 304;
        }
        else if (n->ID==11){ //Chancelry Annex Buildings
            n->_inst = true;
            n->max_pv_size = 192;
        }
        else if (n->ID==27){ //Toad Hall
            n->_inst = true;
            n->max_pv_size = (15)/(0.218);
        }
        else if (n->ID==32){ //University House
            n->_inst = true;
            n->max_pv_size = (45)/(0.218);
        }
        else if (n->ID==7){ //Laurus Wing – Ursula Hall
            n->_inst = true;
            n->max_pv_size = (50)/(0.218);
        }
        else if (n->ID==58){ //Menzies Library
            n->_inst = true;
            n->max_pv_size = (60)/(0.218);
        }
        else if (n->ID==62){ //Graduate House
            n->_inst = true;
            n->max_pv_size = (30)/(0.218);
        }
        else if (n->ID==3){ //Gould Wing, Botany and Zoology
            n->_inst = true;
            n->max_pv_size = (10)/(0.218);
        }
        else if (n->ID==71){ //Hugh Ennor Building (APF)
            n->_inst = true;
            n->max_pv_size = (10)/(0.218);
        }
        else if (n->ID==22){ //Melville Hall
            n->_inst = true;
            n->max_pv_size = (30)/(0.218);
        }
        else if (n->ID==51){ //Computer science & Information Technology Building
            n->_inst = true;
            n->max_pv_size = (45)/(0.218);
        }
        else if (n->ID==75){ //John Curtin Building (Old)
            n->_inst = true;
            n->max_pv_size = (50)/(0.218);
        }
        else {
            n->max_pv_size =  300;
        }

    }
    return 0;
}



/*int Net::readload_direct(string fname, int _timesteps){
    cout << "Loading file " << fname << endl;
    ifstream file(fname.c_str());
    if(!file.is_open()){
        cout << "Could not open file\n";
        return -1;
    }
    
    if(file.peek()==EOF)
    {cout << "File is empty" <<endl;
        return 999;
    }
    
    for (auto n : nodes) {
        n->_cond[0]->_pl.pop_back();
        //        cout << "pl size = " << n->_cond[0]->_pl.size() << endl;
    }
    int i=0;                                         //number of bus
    
    //    int pos=0;
    
    string line;
    string word;
    
    getline(file,line);                               //first line to ignore
    //    getline(file,line);                               //second line to ignore
    
    double av = 0;
    double tot = 0;
    double remain = 0;
    double pl = 0;
    double tot_Kw = 0;
    int _time_count = 0; //counts all time instances
    while(file.peek()!=EOF)
    {
        getline(file,line);                           //third line store in 'line'
        
        i=0;                                          //initialize i, new line
        std::stringstream copy(line);                 //store the whole line into copy
        getline(copy,word,',');                       //first column to ignore
        tot = 0;
        tot_Kw = 0;
        while(getline(copy,word,','))                 //start with the second column
        {
            //            int length=word.length();
            int len=word.length()-2;                  //erase 'kW'
            //            if (len !=2 ) {
            //                continue;
            //            }
            
            string c=word.substr(0,len);
            
            
            //            double test = atof(c.c_str());            //simulate pl(i,t)
            //            cout << "bus" << i << " pl = ";
            tot_Kw +=  atof(c.c_str());
            pl = atof(c.c_str())/(1000*bMVA);//power demand for one building
            
            
            
            if (pl > 0.1) {
                nodes[i]->_cond[0]->_pl.push_back(0);
                //                cerr << " 0; ";
            }
            else {
                //                cout <<  " " << pl << " ;";
                nodes[i]->_cond[0]->_pl.push_back(pl);         //*MW
            }
            
            
            tot += nodes[i]->_cond[0]->_pl[nodes[i]->_cond[0]->_pl.size()-1];                                      //*MW
            i++;
            //            cout << endl;
        }
        cout << "Original TOTAL Kw = " << tot_Kw << endl;
        cout << "TOTAL Kw = " << tot*1000*bMVA << endl;
        
        //        cout << "My TOTAL = " << tot << "__"<< nodes[i]->_cond[0]->_pl.size()-1 << endl;
        remain = fmax(0, 0.1 - tot);              //assumed limit=10MW  (which is the average total consumption (power)...making sure no bus gets negative power
        //*shouble be chenged again when the loadfile is accurate*
        
        /* double tot2 = tot;  // no need for average
        
        av = remain/(nodes.size() - i);//fill the remaining part buildings for average values
        for (int j = i; j < nodes.size(); j++)
        {
            nodes[j]->_cond[0]->_pl.push_back(av);
            tot2 += av;
        }*/
        //        cout << "TOTAL2 = " << tot2 << endl;
        //        for (int j = 0; j< nodes.size(); j++)
        //        {cout<<j<<" "<<nodes[j]->_cond[0]->_pl[nodes[i]->_cond[0]->_pl.size()-1]<<endl;}
        
        //        t++;
 /*       _time_count++;
    }
    cout << "total number of timesteps = " << _time_count << endl;
    int sub_time_count = _time_count/_timesteps; //time count of each division
    cout << "subtime = " << sub_time_count << endl;
    float sum_load_;
    float avg_sub_time_load;
    vector<double> av_loads;
    typedef std::pair<int, double> myPair_type;
    
    struct mypair_comp{
        bool operator()(myPair_type const& lhs, myPair_type const& rhs){
            return lhs.second > rhs.second;
        }
    };
    vector<myPair_type> pairs;//order decreasing average load
    double tot_aver = 0;
    for (int i = 0; i < nodes.size(); i++) {
        tot_aver = 0;
        cout << "av load for bus " << i << " = ";
        for (int t = 1; t <= _timesteps; t++) {
            sum_load_ = 0;
            for (int stc = (t-1)*sub_time_count; stc < (t-1)*sub_time_count + sub_time_count; stc++) {
                sum_load_ += nodes[i]->_cond[0]->_pl[stc]; //sum for each division
            }
            avg_sub_time_load = sum_load_/sub_time_count;
            av_loads.push_back(avg_sub_time_load);
            cout << " ; " << avg_sub_time_load;
            tot_aver += avg_sub_time_load;
        }
        tot_aver /= _timesteps;
        pairs.push_back(make_pair(i, tot_aver));
        cout << endl;
        nodes[i]->_cond[0]->_pl = av_loads;
        av_loads.clear();
    }
    std::sort(pairs.begin(), pairs.end(), mypair_comp());
    for (int i = 0; i < 20; i++) { //first 20 buidling install
        //cout <<"pairs" << pairs[i].first << "  " << pairs[i].second << endl;
        nodes[pairs[i].first]->_cand = true;
    }
    for (auto n:nodes){
        
        if (n->ID==64){
            n->_inst = true;
            n->max_pv_size = (14.28)/(0.945);
        };
        if (n->ID==42){
            n->_inst = true;
            n->max_pv_size = (5.32)/(0.945); //using lowest radiation,to get max pv size
        }
        else if (n->ID==135){
            n->_inst = true;
            n->max_pv_size = (4.94)/(0.945);
        }
        else if (n->ID==27){
            n->_inst = true;
            n->max_pv_size = (28.56)/(0.945);
        }
        else if (n->ID==156){
            n->_inst = true;
            n->max_pv_size = (34.08)/(0.945);
        }
        
        else if (n->ID==3){
            n->_inst = true;
            n->max_pv_size = (34.08)/(0.945);
        }
        else {
            n->max_pv_size =  150;
        }
        
    }
    return 0;
}*/






    
    
    
    
//    
//int Net::choosetime(){
//    string time;
//    cout<<"type in time interval number"<<endl;
//    cin>>time;
//    int t=atoi(time.c_str());
////    cout<<t<<endl;
//    
//    if (t> (nodes[0]->_cond[0]->_pl.size()))
//    { cout<<"invalid time shot"<<endl;
//        return -1;}
//    else {
//    
//   
//    for (auto &n: nodes)
//    {
//        n->_cond[0]->_pl[0]=n->_cond[0]->_pl[t];
////        cout<<n->_cond[0]->_pl[0]<<endl;
////        n->_cond[0]->_pvmax[0]=n->_cond[0]->_pvmax[t];
////        cout<<n->_cond[0]->_pvmax[0]<<endl;
//    }
//        
////    for (auto &n: gens){
////        n->_timecost->c0[0]=n->_timecost->c0[t];
////        n->_timecost->c1[0]=n->_timecost->c1[t];
////        n->_timecost->c2[0]=n->_timecost->c2[t];
////        }
//
//    
//    return 0;
//}
//}



int Net::readFile(string fname){
    string name;
    vector<string> bus_name;
    double pl = 0, ql = 0, gs = 0, bs = 0, kvb = 0, vmin = 0, vmax = 0, vs = 0;
    int id = 0;
    cout << "Loading file " << fname << endl;
    ifstream file(fname.c_str());
    if(!file.is_open()){
        cout << "Could not open file\n";
        return -1;
    }
    _clone = new Net();
    string word;
    while (word.compare("function")){
        file >> word;
    }
    file.ignore(6);
    file >> word;
//    getline(file, word);
    _name = word;
//    cout << _name << endl;
    while (word.compare("mpc.baseMVA")){
        file >> word;
    }
    file.ignore(3);
    getline(file, word,';');
    bMVA = atoi(word.c_str());
//    cout << "BaseMVA = " << bMVA << endl;
    
    /* Node name */
    while (word.compare("mpc.bus_name")){
        file >> word;
    }
    getline(file, word);
    while(word.compare("};")){
        file >> word;
        bus_name.push_back(word.substr(1,(word.length()-3)));
        //cout<<"Bus name is "<<name <<"; "<<"\n";
    }
    file.seekg (0, file.beg);
    
    /* Nodes data */
    while (word.compare("mpc.bus")){
        file >> word;
    }
    getline(file, word);
    Node* node = NULL;
    Node* node_clone = NULL;
    file >> word;
    while(word.compare("];")){
        name = word.c_str();
        //cout<<"Bus name is "<<name<<"; ";
        id = atoi(name.c_str());
        //cout<<"Bus name is "<<id<<"; ";
        file >> ws >> word >> ws >> word;
        pl = atof(word.c_str())/bMVA;
        file >> word;
        ql = atof(word.c_str())/bMVA;
        file >> word;
        gs = atof(word.c_str())/bMVA;
        file >> word;
        bs = atof(word.c_str())/bMVA;
        file >> ws >> word >> ws >> word;
        vs = atof(word.c_str());
        file >> ws >> word >> ws >> word;
        kvb = atof(word.c_str());
        file >> ws >> word >> ws >> word;
//        vmax = atof(word.c_str());
        vmax = 1.06;
        getline(file, word,';');
//        vmin = atof(word.c_str());
        vmin = 0.94;
        node = new Node(bus_name[id-1], pl, ql, gs, bs, vmin, vmax, kvb, 1);
        node_clone = new Node(bus_name[id-1], pl, ql, gs, bs, vmin, vmax, kvb, 1);
        cout<<"Bus name is "<<bus_name[id-1] <<"; "<<"\n";
        node->vs = vs;
        add_node(node);
        _clone->add_node(node_clone);
//        node->print();
        file >> word;
    }
    file.seekg (0, file.beg);
    /* Generator data */
    while (word.compare("mpc.gen")){
        file >> word;
    }
    double qmin = 0, qmax = 0, pmin = 0, pmax = 0, ps = 0, qs = 0;
    int status = 0;
    getline(file, word);
    file >> word;
    while(word.compare("];")){
        name = word.c_str();
        node = get_node(name);
        file >> word;
        ps = atof(word.c_str())/bMVA;
        file >> word;
        qs = atof(word.c_str())/bMVA;
        file >> word;
        qmax = atof(word.c_str())/bMVA;
        file >> word;
        qmin = atof(word.c_str())/bMVA;
        file >> ws >> word >> ws >> word >> ws >> word;
        status = atof(word.c_str());
        file >> word;
        pmax = atof(word.c_str())/bMVA;
        file >> word;
        pmin = atof(word.c_str())/bMVA;
        getline(file, word,'\n');
        if(status==1){
            node->_has_gen = true;
//            node->_nb_gen++;
            Gen* g = new Gen(node, to_string(node->_gen.size()), pmin, pmax, qmin, qmax);
            g->ps = ps;
            g->qs = qs;
            gens.push_back(g);
            node->_gen.push_back(g);
        }
//        g->print();
//        getline(file, word);
        file >> word;
    }
    file.seekg (0, file.beg);
    /* Generator costs */
    while (word.compare("mpc.gencost")){
        file >> word;
    }
    double c0 = 0, c1 = 0,c2 = 0;
    getline(file, word);
    //cout<<"Number of generators = " << gens.size() << endl;
    for (int i = 0; i < gens.size(); i++) {
        file >> ws >> word >> ws >> word >> ws >> word >> ws >> word >> ws >> word;
        c2 = atof(word.c_str());
        file >> word;
        c1 = atof(word.c_str());
        file >> word;
        c0 = atof(word.c_str());
        gens[i]->set_costs(c0, c1, c2);
//        gens[i]->print();
        getline(file, word);
    }
    file.seekg (0, file.beg);
    /* Lines data */
    m_theta_lb = 0;
    m_theta_ub = 0;
    while (word.compare("mpc.branch")){
        file >> word;
    }
    getline(file, word);
    double res = 0;
    Arc* arc = NULL;
    Arc* arc_clone = NULL;
    string src, dest;
    file >> word;
    while(word.compare("];")){
        src = word;
        file >> dest;
        id = (int)arcs.size() + 1;
        //arc = new Arc(src+dest);
        //arc_clone = new Arc(src+dest);
        arc = new Arc(to_string(id));
        arc_clone = new Arc(to_string(id));
        arc->id = id;
        arc_clone->id = id;
        arc->src = get_node(src);
        arc->dest= get_node(dest);
        arc_clone->src = _clone->get_node(src);
        arc_clone->dest = _clone->get_node(dest);
        file >> word;
        arc->r = atof(word.c_str());
        file >> word;
        arc->x = atof(word.c_str());
        res = pow(arc->r,2) + pow(arc->x,2);
        if (res==0) {
            cerr << " line with r = x = 0" << endl;
            exit(-1);
        }
        arc->g = arc->r/res;
        arc->b = -arc->x/res;
        
        file >> word;
        arc->ch = atof(word.c_str());
        file >> word >> ws >> word >> ws >> word;   //reading rate C;
        arc->limit = atof(word.c_str())/bMVA;
        file >> word;
        if(atof(word.c_str()) == 0)
            arc->tr = 1.0;
        else
            arc->tr = atof(word.c_str());
        file >> word;
        arc->as = atof(word.c_str())*M_PI/180;
        file >> word;
        arc->cc = arc->tr*cos(arc->as);
        arc->dd = arc->tr*sin(arc->as);
        arc->status = atof(word.c_str());
        arc_clone->status = arc->status;
        file >> word;
//        arc->tbound.min = atof(word.c_str())*M_PI/180;
        arc->tbound.min = -35*M_PI/180.;
        arc_clone->tbound.min = arc->tbound.min;
        m_theta_lb += arc->tbound.min;
        file >> word;
//        arc->tbound.max = atof(word.c_str())*M_PI/180;
        arc->tbound.max = 35*M_PI/180.;
        arc_clone->tbound.max = arc->tbound.max;
        m_theta_ub += arc->tbound.max;
        arc->smax = max(pow(arc->src->vbound.max,2)*(arc->g*arc->g+arc->b*arc->b)*(pow(arc->src->vbound.max,2) + pow(arc->dest->vbound.max,2)), pow(arc->dest->vbound.max,2)*(arc->g*arc->g+arc->b*arc->b)*(pow(arc->dest->vbound.max,2) + pow(arc->src->vbound.max,2)));
        if(arc->status==1){
            arc->connect();
            if(!add_arc(arc)){// not a parallel line
                arc_clone->connect();
                _clone->add_arc(arc_clone);
            }
            else {
                delete arc_clone;
            }
        }
        else {
            delete arc_clone;
            delete arc;
        }
//        arc->print();
        getline(file, word,'\n');
        file >> word;
    }
    file.close();
//    for (auto n:nodes) {
//        n->print();
//        cout << "node" << n->ID << ": fill_in = " << n->fill_in << endl;
//    }
    get_tree_decomp_bags();
    return 0;
}


