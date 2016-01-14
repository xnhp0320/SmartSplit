#include <stdio.h>
#include <stdint.h>


#define TABLE_LENGTH 8
#define MAXDIMENSIONS 5
#define MAX_CUT_FIELD 5 
#define NUM_INDEX TABLE_LENGTH
#define NO_DIV
#define STAT
//#define STACK_OP
//#define HYPERCUTS
//#define FAST
//#define ABCII
//#define HICUTS

#ifdef STAT
extern uint32_t total_size;
extern int max_acc;
extern int acc ;
extern int sum_acc;
#endif

struct boundary2{
    uint32_t min[2];
};

struct range{
  unsigned long long low;
  unsigned long long high;
};

struct rule_boundary
{
    struct range field[MAXDIMENSIONS];
};


struct realrange{
    uint32_t low;
    uint32_t high;
};




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

#ifndef NO_DIV
struct node{
    uint8_t type;
    uint8_t rules_inside;
    uint8_t dim[2]; 
    uint16_t num[2];
    struct boundary2 b;
    void *ps;
    uint32_t s[2];
    uint16_t comp_table[2][8];
#ifdef FAST
    uint64_t epb;
#endif
}__attribute__((__aligned__(64)));
#else
struct node{
    uint8_t type;
    uint8_t rules_inside;
    uint8_t dim[2]; 
    uint8_t s[2];
    uint16_t num[2];
    struct boundary2 b;
    void *ps;
#ifdef HYBRID
    uint16_t comp_table[2][8];
#endif
#ifdef HYPERCUTS
    int rule_in_node;
#endif
#ifdef ABCII
    int rule_in_node;
    uint32_t cuts[2];
#endif
#ifdef FAST
    uint64_t epb;
#endif
}__attribute__((__aligned__(64)));
#endif

struct hinode{
    uint8_t type;
    uint8_t s;
    uint16_t num;
    uint32_t min;
    void *ps;
#ifdef FAST
    uint64_t epb;
#endif
};

// type -> 1 bits leaf, 
//      -> 6 bits for cut dim
// cuts -> 4 bits for dim 1
// cuts -> 4 bits for dim 2
//
struct hnode{
    uint8_t type;
    uint8_t cuts;
    uint8_t num;
    uint32_t filter;
    void *ps;
    uint64_t epb;
};

struct tree_info {
    int node_cnt;
    int level_cnt;
    int *level_node_cnt;
    struct node **level_node; 
};

struct tree_info_hi {
    int node_cnt;
    int level_cnt;
    int *level_node_cnt;
    struct hinode **level_node; 
};

//abc-II tree
//

struct anode{
    uint16_t csb[2];
    uint32_t filter;
    void *ps;
    uint64_t epb;
    uint8_t type;
    uint8_t num;
};



struct pc_rule{
    struct realrange field[MAXDIMENSIONS];
};


struct dump_entry {
    uint32_t type;
    uint32_t dim;
    uint32_t mp;
    uint32_t next;

};

struct table_entry {
    uint32_t type;
    uint32_t dim;
    uint32_t mp;
    void * index;
};



int loadrules(FILE *fp);
int load_ft(FILE *fpt, uint32_t *ft);
struct node * load_tree(FILE *fp, struct tree_info *info);
int search_rules(struct node *n, uint32_t *ft);
int g_linear_search(uint32_t *ft);
struct table_entry * load_hs(FILE *fp);
int lookup_table(struct table_entry *rootnode, uint32_t *ft);
extern int rule_set_cnt;
