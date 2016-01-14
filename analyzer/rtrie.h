#ifndef RTRIE_H_
#define RTRIE_H_

#include <string>
#include <list>
#include <map>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <cmath>
#include <vector>
#include <stdint.h>
#include "hypc.h"

using namespace std;

struct rg{
    uint32_t low;
    uint32_t high;

    rg(uint32_t l, uint32_t h): low(l), high(h) {

    };
    rg() {
        low =0;
        high = 0;
    }
    rg(const rg &a) {
        low = a.low;
        high = a.high;
    }
};

struct rnode{
    rg b;
    list<pc_rule*> rlist;
    struct rnode *right, *left;

    rnode(rg a) : b(a) {
        right = NULL;
        left = NULL;
    };
    rnode(): b(0,0) { 
        right = NULL;
        left = NULL;
    }

    void setb(const rg &sb) {
        b = sb;
    }
};

bool rt_qry_insert(rnode *n, rg r, vector<pc_rule*> &set, pc_rule* index);

#endif
