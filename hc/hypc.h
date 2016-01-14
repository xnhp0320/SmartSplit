#ifndef __HYPC_H__
#define __HYPC_H__

#define MAXDIMENSIONS 5
#define NUM_INDEX 8 
#define MAX_CUT_FIELD 5 
#define BALANCE_TREE_DEPTH 3 


#define NUM_RULES_MOVE_UP 1 
#define CHODIM 
//#define STACK_OP

#include <list>
#include <vector>
using namespace std;

struct range{
  unsigned long long low;
  unsigned long long high;
};

struct pc_rule{
  int priority;
  struct range field[MAXDIMENSIONS];
  int siplen, diplen;
  unsigned sip[4], dip[4];
};

struct rule_boundary
{
    struct range field[MAXDIMENSIONS];
};

struct node 
{
  int depth;
  struct rule_boundary boundary;
  list <pc_rule*> classifier;
  list <node *> children;
  int cuts[MAXDIMENSIONS];
  int ub_mask[MAXDIMENSIONS];
  int fuse_array[MAX_CUT_FIELD][NUM_INDEX];
  int fuse_array_num[MAX_CUT_FIELD];
  int no;
  int node_has_rules;
  pc_rule * rule_in_node;
  int isLeaf;
  int hard;
  int acc;
  vector<node*> ps;
};

struct cuts_node
{
    struct rule_boundary boundary;
    int depth;
    list<pc_rule *> classifier;
};

typedef unsigned int field_type;

struct dump_node{
    int depth;
    struct rule_boundary boundary;
    int rules_inside;
    int cuts[MAXDIMENSIONS];
    int ub_mask[MAXDIMENSIONS];
    int fuse_array[MAX_CUT_FIELD][NUM_INDEX];
    int fuse_array_num[MAX_CUT_FIELD];
    int no;
    int node_has_rules;
    int rule_in_node;
    int isLeaf;
    int pointer_size;
};


#define SEARCH_TEST
#define HICUTS
//#define HYPERCUTS
#define OLD_SPLIT

extern int no_div;






#endif

