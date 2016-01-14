#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#include "pc.h"
#include <time.h>


struct pc_rule * rule_set;
int rule_set_cnt;

#ifdef STACK_OP
uint32_t *stack;
uint32_t stack_p;
#endif

#ifdef STAT
int max_acc = 0;
int acc = 0;
int sum_acc = 0;
uint32_t total_size = 0;
#endif


void node_dim(struct dump_node *n, int *chosen_dim, int *chosen)
{
    memset(chosen_dim, 0, MAXDIMENSIONS * sizeof(int));
    *chosen = 0;
    int i;

    for(i = 0; i <MAXDIMENSIONS; i++) {
        if(n->cuts[i] != 1) {
            chosen_dim[(*chosen)++] = i;
        }
    }
}

uint32_t calc_s(struct dump_node *dn, int dim)
{
    int cuts = dn->cuts[dim];
    struct range rdim = dn->boundary.field[dim];
    unsigned long long s = ceil((double)(rdim.high - rdim.low + 1)/cuts); 
#ifdef NO_DIV
    if ( (1ULL << (unsigned)log2(s)) != s) {
        //printf("we need div %llu\n", s);
        s = 1ULL<<((unsigned)ceil(log2(s)));
    }
#endif
    return (uint32_t)s;
}

int real_cuts(struct dump_node *dn, int dim, unsigned long long *rs)
{
    int cuts = dn->cuts[dim];
    struct range rdim = dn->boundary.field[dim];
    unsigned long long s = ceil((double)(rdim.high - rdim.low + 1)/cuts); 
#ifdef NO_DIV
    if ( (1ULL << (unsigned)log2(s)) != s) {
        //printf("we need div %llu\n", s);
        s = 1ULL<<((unsigned)ceil(log2(s)));
    }
#endif
    *rs = s;

    struct range test;
    int i;
    test.low = rdim.low;
    test.high = rdim.low + s -1;

    for(i = 1; i < cuts; i++) {
        test.low = test.high + 1;
        test.high = test.low + s -1;

        if(test.high > rdim.high) {
            test.high = rdim.high;
        }

        if(test.low > test.high) {
            break;
        }
    }
    return i;
}


void dump2node(struct dump_node *dn, struct node *curr_node)
{
    int chosen_dim[MAXDIMENSIONS];
    int chosen;
    int type = 0;
    int i;
#ifdef HYBRID
    int j;
#endif
    unsigned long long s;

    node_dim(dn, chosen_dim, &chosen);
    curr_node->rules_inside = dn->rules_inside;
#if defined(HYPERCUTS) || defined(ABCII)
    curr_node->rule_in_node = dn->rule_in_node;
#endif
    for(i = 0; i < chosen; i++ ) {
        curr_node->b.min[i] = dn->boundary.field[chosen_dim[i]].low;
        curr_node->dim[i] = chosen_dim[i];

#ifdef HYBRID
        if(dn->ub_mask[chosen_dim[i]] == 1) {
            curr_node->num[i] = (uint8_t)(dn->fuse_array_num[i]);
            for(j = 0; j < dn->fuse_array_num[i]; j++) {
                curr_node->comp_table[i][j] = (uint16_t)dn->fuse_array[i][j];
            }
#ifndef NO_DIV
            curr_node->s[i] = calc_s(dn, chosen_dim[i]);
#else
            curr_node->s[i] = (uint8_t)log2(calc_s(dn, chosen_dim[i]));
#endif
            type |= (1<<i);
        }
        else {
#endif //HYBRID
            curr_node->num[i] = real_cuts(dn, chosen_dim[i], &s);
#ifndef NO_DIV
            curr_node->s[i] = (uint32_t)s;
#else
            curr_node->s[i] = (uint8_t)log2(s);
#endif

#ifdef HYBRID
        }
#endif //HYBRID

#ifdef ABCII
        curr_node->cuts[i] = dn->cuts[chosen_dim[i]];
#endif
    }

    if(chosen != 0) 
        curr_node->type = type;
    else
        curr_node->type = 4;

}

void dump2hinode(struct dump_node *dn, struct hinode *curr_node)
{
    int chosen_dim[MAXDIMENSIONS];
    int chosen;
    unsigned long long s;

    node_dim(dn, chosen_dim, &chosen);

    if(dn->rules_inside) {
        curr_node->type |= 0x80;
        curr_node->num = dn->rules_inside;
    }

    if(chosen > 1) {
        printf("parse error, chosen>1\n");
    }

    if(chosen != 0) {
        curr_node->min = dn->boundary.field[chosen_dim[0]].low;
        curr_node->type |= chosen_dim[0];

        curr_node->num = real_cuts(dn, chosen_dim[0], &s);
#ifndef NO_DIV
            curr_node->s = (uint32_t)s;
#else
            curr_node->s = (uint8_t)log2(s);
#endif
    }

}


struct hinode * load_tree_hi(FILE *fp, struct tree_info_hi *info)
{
    struct hinode * root = NULL;

    int node_cnt;
    fread(&node_cnt, sizeof(int), 1, fp);
    printf("need to load %d node\n", node_cnt);
    int level_cnt;
    fread(&level_cnt, sizeof(int), 1, fp);
    printf("in total %d levels\n", level_cnt);
    info->node_cnt = node_cnt;
    info->level_cnt = level_cnt;


    int i;
    struct hinode ** level_node = (struct hinode **)malloc(level_cnt * sizeof(*level_node));
    int *level_node_cnt = (int *)malloc(level_cnt * sizeof(int));
    info->level_node_cnt = (int *)malloc(level_cnt * sizeof(int));
    info->level_node = level_node;


    for(i = 0; i < level_cnt; i++) {
        int ncnt;
        fread(&ncnt, sizeof(int), 1, fp);
        printf("level %d has %d nodes\n", i+1, ncnt);
        level_node_cnt[i] = ncnt;
        info->level_node_cnt[i] = ncnt;
        level_node[i] = (struct hinode *)calloc(1, ncnt * sizeof(struct hinode));
    }

    struct dump_node dn;
    int *ddt = NULL;
    int *pcode = NULL;
    struct hinode *curr_node = NULL;

    node_cnt = 0;
    int child = 0;
    int curr_level = 0;
    int next_level_cnt = 0;



    while(!feof(fp) && curr_level < level_cnt) {
        fread(&dn, sizeof(dn), 1, fp);
        curr_node = level_node[curr_level] + node_cnt;
        node_cnt ++;
        
        dump2hinode(&dn, curr_node);

        if(dn.rules_inside) {
            ddt = (int*)malloc(dn.rules_inside * sizeof(*ddt));
            fread(ddt, sizeof(int), dn.rules_inside, fp);
            curr_node->ps = calloc(1, dn.rules_inside * sizeof(uint32_t));
            memcpy(curr_node->ps, ddt, dn.rules_inside * sizeof(uint32_t));
            free(ddt);
        }
        

        if(dn.pointer_size) {
            pcode = (int*)malloc(dn.pointer_size * sizeof(int));
            fread(pcode, sizeof(int), dn.pointer_size, fp);
            fread(&child, sizeof(int), 1, fp);

            curr_node->ps = (struct hinode**)malloc(dn.pointer_size * 
                    sizeof(struct hinode *));

            struct hinode **np = curr_node->ps;

            for(i = 0; i < dn.pointer_size;i++ ){ 
                if(pcode[i] != -1) {
                    np[i] = level_node[curr_level + 1] + pcode[i]
                        + next_level_cnt;
                }
                else
                    np[i] = NULL;
            }
#ifdef FAST
            if(dn.pointer_size <= 64) {
                for (i = 0; i < dn.pointer_size; i++) {
                    if(np[i]) {
                        curr_node->epb |= (1ULL << i);
                    }
                }
                free(np);
                curr_node->ps = level_node[curr_level + 1] + next_level_cnt; 
            }
#endif
            free(pcode);
            next_level_cnt += child;
        }



        if(node_cnt == level_node_cnt[curr_level]) {
            node_cnt = 0;
            curr_level ++;
            next_level_cnt = 0;
        }

    }

    root = level_node[0];
    return root;
}


struct node * load_tree(FILE *fp, struct tree_info *info)
{
    struct node * root = NULL;

    int node_cnt;
    fread(&node_cnt, sizeof(int), 1, fp);
    printf("need to load %d node\n", node_cnt);
#ifdef STAT
    total_size += node_cnt * sizeof(struct node);
#endif
    int level_cnt;
    fread(&level_cnt, sizeof(int), 1, fp);
    printf("in total %d levels\n", level_cnt);
    info->node_cnt = node_cnt;
    info->level_cnt = level_cnt;


    int i;
    struct node ** level_node = (struct node **)malloc(level_cnt * sizeof(*level_node));
    int *level_node_cnt = (int *)malloc(level_cnt * sizeof(int));
    info->level_node_cnt = (int *)malloc(level_cnt * sizeof(int));
    info->level_node = level_node;


    for(i = 0; i < level_cnt; i++) {
        int ncnt;
        fread(&ncnt, sizeof(int), 1, fp);
        printf("level %d has %d nodes\n", i+1, ncnt);
        level_node_cnt[i] = ncnt;
        info->level_node_cnt[i] = ncnt;
        level_node[i] = (struct node *)calloc(1, ncnt * sizeof(struct node));
    }

    struct dump_node dn;
    int *ddt = NULL;
    int *pcode = NULL;
    struct node *curr_node = NULL;

    node_cnt = 0;
    int child = 0;
    int curr_level = 0;
    int next_level_cnt = 0;



    while(!feof(fp) && curr_level < level_cnt) {
        fread(&dn, sizeof(dn), 1, fp);
        curr_node = level_node[curr_level] + node_cnt;
        node_cnt ++;
        
        dump2node(&dn, curr_node);

        if(dn.rules_inside) {
            ddt = (int*)malloc(dn.rules_inside * sizeof(*ddt));
            fread(ddt, sizeof(int), dn.rules_inside, fp);
            curr_node->ps = calloc(1, dn.rules_inside * sizeof(uint32_t));
#ifdef STAT
            total_size += dn.rules_inside * sizeof(uint32_t);
#endif
            memcpy(curr_node->ps, ddt, dn.rules_inside * sizeof(uint32_t));
            free(ddt);
        }
        

        if(dn.pointer_size) {
            pcode = (int*)malloc(dn.pointer_size * sizeof(int));
            fread(pcode, sizeof(int), dn.pointer_size, fp);
            fread(&child, sizeof(int), 1, fp);

            curr_node->ps = (struct node**)malloc(dn.pointer_size * 
                    sizeof(struct node *));

#ifdef STAT
            total_size += dn.pointer_size * sizeof(struct node *);
#endif
            struct node **np = curr_node->ps;

            for(i = 0; i < dn.pointer_size;i++ ){ 
                if(pcode[i] != -1) {
                    np[i] = level_node[curr_level + 1] + pcode[i]
                        + next_level_cnt;
                }
                else
                    np[i] = NULL;
            }
#ifdef FAST
            if(dn.pointer_size <= 64) {
                for (i = 0; i < dn.pointer_size; i++) {
                    if(np[i]) {
                        curr_node->epb |= (1ULL << i);
                    }
                }
                free(np);
                curr_node->ps = level_node[curr_level + 1] + next_level_cnt; 
            }
#endif
            free(pcode);
            next_level_cnt += child;
        }



        if(node_cnt == level_node_cnt[curr_level]) {
            node_cnt = 0;
            curr_level ++;
            next_level_cnt = 0;
        }

    }

    root = level_node[0];
    return root;
}

#if defined(HYPERCUTS) || defined(ABCII)
inline int check_rule_p(uint32_t index, uint32_t *ft)
{
    struct pc_rule *r = &(rule_set[index]);
    if(ft[0] <= r->field[0].high && ft[0] >= r->field[0].low &&
            ft[1] <= r->field[1].high && ft[1] >= r->field[1].low &&
            ft[2] <= r->field[2].high && ft[2] >= r->field[2].low &&
            ft[3] <= r->field[3].high && ft[3] >= r->field[3].low &&
            ft[4] <= r->field[4].high && ft[4] >= r->field[4].low) {
        return index;
    }
    else {
        return -1;
    }

}
#endif


inline int check_rule(uint32_t index, uint32_t *ft)
{
    struct pc_rule *r = &(rule_set[index]);
    if(ft[0] <= r->field[0].high && ft[0] >= r->field[0].low &&
            ft[1] <= r->field[1].high && ft[1] >= r->field[1].low &&
            ft[2] <= r->field[2].high && ft[2] >= r->field[2].low &&
            ft[3] <= r->field[3].high && ft[3] >= r->field[3].low &&
            ft[4] <= r->field[4].high && ft[4] >= r->field[4].low) {
        return 1;
    }
    else {
        return -1;
    }

}

int g_linear_search(uint32_t *ft)
{
    int ret = -1;
    int i;
    for(i = 0; i < rule_set_cnt; i++) {
        ret = check_rule(i, ft);
        if(ret == 1) {
            ret = i;
            break;
        }
    }

    return ret;
}
#ifdef ABCII
inline int linear_search_abcii(struct anode* n, uint32_t *ft)
{
    int ret = -1;
    int i;
    uint32_t *r = n->ps;
    for(i = 0; i < n->num; i++) {
        ret = check_rule(r[i], ft);
        if(ret == 1) {
            ret = r[i];
            break;
        }
    }
    return ret;
}
#endif
#ifdef HYPERCUTS
inline int linear_search_hypercuts(struct hnode* n, uint32_t *ft)
{
    int ret = -1;
    int i;
    uint32_t *r = n->ps;
    for(i = 0; i < n->num; i++) {
        ret = check_rule(r[i], ft);
        if(ret == 1) {
            ret = r[i];
            break;
        }
    }
    return ret;
}
#endif

inline int linear_search_hi(struct hinode* n, uint32_t *ft)
{
    int ret = -1;
    int i;
    uint32_t *r = n->ps;
    for(i = 0; i < n->num; i++) {
        ret = check_rule(r[i], ft);
        if(ret == 1) {
            ret = r[i];
            break;
        }
    }
    return ret;
}

inline int linear_search(struct node* n, uint32_t *ft)
{
    int ret = -1;
    int i;
    uint32_t *r = n->ps;
    for(i = 0; i < n->rules_inside; i++) {
        ret = check_rule(r[i], ft);
        if(ret == 1) {
            ret = r[i];
            break;
        }
    }
    return ret;
}

inline int comp_table_search(uint32_t v, uint16_t *t, uint8_t num)
{
    int i;
    for(i = num - 1; i>=0; i--) {
        if ( v >= t[i])
            return i;
    }
    return 0;
}
#ifdef HYPERCUTS

#ifdef STACK_OP
inline int reverse_linear_search(uint32_t * stack, uint32_t stack_p, uint32_t *ft)
{
    int ret = -1;
    int min_ret = -1;
    while( -- stack_p)  {
        ret = check_rule_p(stack[stack_p], ft); 
        if((uint32_t)ret < (uint32_t)min_ret)
            min_ret = ret;
    }
    return min_ret;
}
#endif

int bits_extra[5] = {0, 0, 16, 16, 24};
int search_rules_hypercuts(struct hnode* n, uint32_t *ft) {
    int bits_pos[5] = {0,0,0,0,0};
    uint32_t min_priority = 0xFFFFFFFF;
    int dim1;
    int dim2;
    int num1;
    int num2;
    uint32_t f1;
    uint32_t f2;
    uint32_t final;
    uint64_t bp;
    uint32_t next;
    int ret;

#ifdef STACK_OP
    stack_p = 1;
#endif

    while(!(n->type & 0x80)) {
        dim1 = n->type >> (1*3);
        num1 = n->cuts >> (1*4);
        dim2 = n->type & 0x07;
        num2 = n->cuts & 0x0f;

        if(num1) {
            f1 = ft[dim1] << (bits_pos[dim1] + bits_extra[dim1]); 
            f1 = f1 >> (32 - num1);
            bits_pos[dim1] += num1;
        }
        else 
            f1 = 0;
       
        //if(num2) {
            f2 = ft[dim2] << (bits_pos[dim2] + bits_extra[dim2]);
            f2 = f2 >> (32 - num2);
            bits_pos[dim2] += num2;
        //}
        //else
        //    f2 = 0;

        final = ((f1<<num2) | f2);

        if(n->filter!= 0xFFFFFFFF) {
#ifndef STACK_OP
            ret = check_rule_p(n->filter, ft);
            if((uint32_t)ret < min_priority) 
                min_priority = ret;
#else
            stack[stack_p] = n->filter;
            stack_p ++;
#endif
        }

        if(n->epb & (1ULL<<final)) {
            bp = n->epb;
            if(final != 0) { 
                bp <<= (64 - final);
                next = __builtin_popcountll(bp);
            }
            else
                next = 0;
            n = (struct hnode*)(n->ps) + next;
        }
        else{
#ifndef STACK_OP
            return min_priority;
#else
            return reverse_linear_search(stack, stack_p, ft);
#endif
        }
    }

    ret = linear_search_hypercuts(n, ft);

#ifdef STACK_OP
    min_priority = reverse_linear_search(stack, stack_p, ft);
#endif

    if((uint32_t)ret < min_priority)
        min_priority = ret;

    return min_priority;

}
#endif

int search_rules_hi(struct hinode *n, uint32_t *ft)
{
    int v;
    int index;
    int ret;
    struct hinode **np;

    while(n && !(n->type & 0x80)) {
#ifndef NO_DIV
        v = (ft[n->type & 0x7f] - n->min) / n->s;
#else
        v = (ft[n->type & 0x7f] - n->min) >> (n->s);
#endif

        index = v;
        if(index >= n->num)
            return -1;

#ifndef FAST
        np = n->ps;
        n = np[index];
#else
        if(n->epb){
            uint64_t bp = n->epb;
            if(n->epb & (1ULL<<index)) {
                if(index != 0) {
                    bp <<= (64 - index);
                    n = (struct hinode*)n->ps + __builtin_popcountll(bp);
                }
                else
                    n = (struct hinode*)n->ps;
            }
            else 
                return -1;
        }
        else {
            np = n->ps;
            n = np[index];
        }

#endif
    }

    if(!n) {
        return -1;
    }

    ret = linear_search_hi(n, ft);
    return ret;
}

int search_rules(struct node *n, uint32_t *ft)
{
    int v;
    int index;
    int old_index;
    int ret;
    int i;
    struct node **np;

    while(n && n->type != 4) {
        old_index = 0;
        for(i = 1; i >= 0; i--) {
            if(n->num[i] == 0)
                continue;
            if(ft[n->dim[i]] < n->b.min[i])
                return -1;
#ifndef NO_DIV
            v = (ft[n->dim[i]] - n->b.min[i]) / n->s[i];
#else
            v = (ft[n->dim[i]] - n->b.min[i]) >> (n->s[i]);
#endif

#ifdef HYBRID
            if(n->type & (1<<i)) {
                index = comp_table_search(v, n->comp_table[i], n->num[i]);
            }
            else
                index = v;
#else
            index = v;
#endif
            if(index >= n->num[i])
                return -1;

            index = old_index * n->num[i] + index;
            old_index = index;

        }
#ifndef FAST
        np = n->ps;
        n = np[index];
#ifdef STAT
        acc+=2;
#endif
#else
        if(n->epb){
            uint64_t bp = n->epb;
            if(n->epb & (1ULL<<index)) {
                if(index != 0) {
                    bp <<= (64 - index);
                    n = (struct node*)n->ps + __builtin_popcountll(bp);
                }
                else
                    n = (struct node*)n->ps;
            }
            else 
                return -1;
        }
        else {
            np = n->ps;
            n = np[index];
        }

#endif
    }

    if(!n) {
        return -1;
    }

    ret = linear_search(n, ft);
    return ret;
}
#if 0 
FILE *fpr;
FILE *fpt;
FILE *ifp;

void parseargs(int argc, char *argv[]) {
    int	c;
    while ((c = getopt(argc, argv, "r:t:l:")) != -1) {
        switch (c) {
            case 'r':
                fpr = fopen(optarg, "r");
                break;
            case 't':
                fpt = fopen(optarg, "r");
                break;
            case 'l':
                ifp = fopen(optarg, "r");
                break;
            default:
                break;
        }
    }

    
    if(fpr == NULL){
        printf("can't open ruleset\n");
        exit(-1);
    }

    if(ifp == NULL) {
        printf("can't open tree file\n");
        exit(-1);
    }

}
#endif



int CheckIPBounds(struct realrange fld)
{
    if (fld.low > 0xFFFFFFFF)
    {
        printf("Error: IPRange is buggy!(%u)\n",fld.low);
        return 1;
    }
    if (fld.high > 0xFFFFFFFF)
    {
        printf("Error: IPRange is buggy!(%u)\n",fld.high);
        return 1;
    }
    if (fld.low > fld.high)
    {
        printf("Error: IPRange is buggy!(%u - %u)\n",fld.low,fld.high);
        return 1;
    }
    return 0;
}

int CheckPortBounds(struct realrange fld)
{
    if (fld.low > 0xFFFF)
    {
        printf("Error: PortRange is buggy!(%u)\n",fld.low);
        return 1;
    }
    if (fld.high > 0xFFFF)
    {
        printf("Error: PortRange is buggy!(%u)\n",fld.high);
        return 1;
    }
    if (fld.low > fld.high)
    {
        printf("Error: PortRange is buggy!(%u - %u)\n",fld.low,fld.high);
        return 1;
    }
    return 0;
}

void IP2Range(unsigned ip1,unsigned ip2,unsigned ip3,unsigned ip4,unsigned iplen,struct pc_rule *rule,int index)
{
    unsigned tmp;
    unsigned Lo,Hi;

    if(iplen == 0){
        Lo = 0;
        Hi = 0xFFFFFFFF;

    }else if(iplen > 0 && iplen <= 8) {
        tmp = ip1 << 24;
        tmp &= (0xffffffff << (32-iplen));

        Lo = tmp;
        Hi = Lo + (1<<(32-iplen)) - 1;
    }else if(iplen > 8 && iplen <= 16){
        tmp =  ip1 << 24; 
        tmp += ip2 << 16;
        tmp &= (0xffffffff << (32-iplen));

        Lo = tmp;
        Hi = Lo + (1<<(32-iplen)) - 1;
    }else if(iplen > 16 && iplen <= 24){
        tmp = ip1 << 24; 
        tmp += ip2 << 16; 
        tmp += ip3 << 8; 
        tmp &= (0xffffffff << (32-iplen));

        Lo = tmp;
        Hi = Lo + (1<<(32-iplen)) - 1;
    }else if(iplen > 24 && iplen <= 32){
        tmp = ip1 << 24; 
        tmp += ip2 << 16; 
        tmp += ip3 << 8; 
        tmp += ip4;
        tmp &= (0xffffffff << (32-iplen));

        Lo = tmp;
        Hi = Lo + (1<<(32-iplen)) - 1;
    }else{
        printf("Error: Src IP length exceeds 32\n");
        exit(1);
    }

    rule->field[index].low  = Lo;
    rule->field[index].high = Hi;

    if (CheckIPBounds(rule->field[index]))
    {
        printf("Error: IP2Range bounds check for %d failed\n",index);
        exit(1);
    }

    //if(rule->field[1].low == 930850816 )
    //    printf("here\n");
    //printf("\t Prefix: %u.%u.%u.%u/%u\n",ip1,ip2,ip3,ip4,iplen);
    //printf("\t Range : %llu : %llu\n",rule->field[index].low,rule->field[index].high);

}

int loadrules(FILE *fp) {
    int i = 0;
    int wild = 0;
    unsigned sip1, sip2, sip3, sip4, siplen;
    unsigned dip1, dip2, dip3, dip4, diplen;
    unsigned proto, protomask;
    unsigned junk, junkmask;

    struct pc_rule rule;
    rule_set = (struct pc_rule*)malloc(100000 * sizeof(struct pc_rule));
    

    while(1) {
        wild = 0;
        if(fscanf(fp,"@%u.%u.%u.%u/%u\t%u.%u.%u.%u/%u\t%u : %u\t%u : %u\t%x/%x\t%x/%x\n",
                    &sip1, &sip2, &sip3, &sip4, &siplen, &dip1, &dip2, &dip3, &dip4, &diplen, 
                    &rule.field[2].low, &rule.field[2].high, &rule.field[3].low, &rule.field[3].high,
                    &proto, &protomask, &junk, &junkmask) != 18) break;


        IP2Range(sip1,sip2,sip3,sip4,siplen,&rule,0);
        IP2Range(dip1,dip2,dip3,dip4,diplen,&rule,1);

        if(protomask == 0xFF){
            rule.field[4].low = proto;
            rule.field[4].high = proto;
        }else if(protomask == 0){
            rule.field[4].low = 0;
            rule.field[4].high = 0xFF;
            wild++;
        }else{
            printf("Protocol mask error\n");
            return 0;
        }
        if ((rule.field[0].low == 0) && (rule.field[0].high == 0xffffffff)) {
            wild++;
        }
        if ((rule.field[1].low == 0) && (rule.field[1].high == 0xffffffff)) {
            wild++;
        }
        if ((rule.field[2].low == 0) && (rule.field[2].high == 65535)) {
            wild++;
        }
        if ((rule.field[3].low == 0) && (rule.field[3].high == 65535)) {
            wild++;
        }
        if (wild != 5) {
            rule_set[i] = rule;
            i++;
        }
    }
    return i;
}

int load_ft(FILE *fpt, uint32_t *ft) 
{
    int ret;
    static int cnt = 0;
    unsigned int junk, junkmask;
    if(fpt == NULL) 
        return 0;

    ret = fscanf(fpt, "%u\t%u\t%u\t%u\t%u\t%u\t%u\n", &(ft[0]),
            &(ft[1]),
            &(ft[2]),
            &(ft[3]),
            &(ft[4]),
            &junk,
            &junkmask);
    
    if(ret != 7)
        return 0;
    cnt ++;
    return cnt;

}


#ifdef HYPERCUTS
int node2hnode(struct node *n, struct hnode *hn)
{
    int i;
    int cnt = 0;

    if(n->type == 4){
        hn->type |= 0x80;
        hn->num = n->rules_inside;
        hn->ps = calloc(1, n->rules_inside * sizeof(uint32_t));
        memcpy(hn->ps, n->ps, n->rules_inside * sizeof(uint32_t));
    }
    else {
        int index;
        for(i = 1; i>=0; i--) {
            hn->type |= (n->dim[i] << (i*3));
            hn->cuts |= ((unsigned)(log2(n->num[i])) << (i*4));
        }

        if(n->num[0] && n->num[1])
            index = n->num[0] * n->num[1];
        else
            index = n->num[0];

        for(i = 0; i < index; i++) {
            if(((struct node**)(n->ps))[i]) {
                hn->epb |= (1ULL<<i);
                cnt ++;
            }
        }

        if(n->rule_in_node != -1)
            hn->filter = n->rule_in_node;
        else
            hn->filter = 0xFFFFFFFF;
    }

    return cnt;
}


struct hnode* convert_to_hypercuts_tree(struct tree_info *info)
{
    printf("converting to the HyperCuts Tree\n");
    struct hnode ** level_node = (struct hnode **)malloc(info->level_cnt * sizeof(*level_node));

    int i;
    for(i = 0; i < info->level_cnt; i++ ){
        level_node[i] = (struct hnode*)calloc(1, info->level_node_cnt[i] * 
                sizeof(struct hnode));
    }

#ifdef STACK_OP
    stack = (uint32_t *)malloc( (info->level_cnt+1) * sizeof(uint32_t));
#endif

    int node_cnt = 0;
    int curr_level = 0;
    int next_level_cnt = 0;
    int child_cnt = 0;

    struct hnode * curr_hnode = NULL;
    struct node* curr_node = NULL;

    while(curr_level < info->level_cnt) {
        curr_hnode = level_node[curr_level] + node_cnt;
        curr_node = info->level_node[curr_level] + node_cnt;
        child_cnt = node2hnode(curr_node, curr_hnode);
        if(!curr_hnode->ps)
            curr_hnode->ps = level_node[curr_level + 1] + next_level_cnt;
        next_level_cnt += child_cnt; 

        node_cnt ++;
        if(node_cnt == info->level_node_cnt[curr_level]) {
            node_cnt = 0;
            curr_level ++;
            next_level_cnt = 0;
        }
    }
    return level_node[0];
}
#endif
#ifdef ABCII
//bt means btree
struct bt{
    uint8_t left;
    uint8_t right;
};

void bt_insert(struct bt* t, uint32_t x, int prefix, int len, int *n)
{
    int i;
    int pos = 0;
    x <<= (32 - len);
    for(i=0;i<prefix;i++){
        if(x & 0x80000000U) {
            if (t[pos].right) {
                pos = t[pos].right;
            }
            else {
                t[pos].right = *n;
                pos = *n;
                (*n)++;
            }
        }
        else {
            if(t[pos].left) {
                pos = t[pos].left;
            }
            else {
                t[pos].left = *n;
                pos = *n;
                (*n)++;
            }
        }
        x <<= 1;
    }
}



uint16_t encode_bt(struct bt *t, int n)
{
    uint16_t code = 0;
    uint8_t q[64];
    uint8_t in = 0;
    uint8_t out = 0;
    uint8_t node;

    //push root
    q[in++]= 0;
    while(in!=out) {
        node = q[out++];
        if(node >= n) {
            printf("error, n > max tree index\n");
        }
        if(t[node].left && t[node].right){
            code |= (1<<(out-1));
            q[in++] = t[node].left;
            q[in++] = t[node].right;
        }
        //else, leave the code as 0
    }
    //remove root ndoe
    code >>= 1;
    return code;
}


uint16_t makecsb(struct node* n, int d)
{
    int i;
    int diff;
    int diff_bits;
    int total_bits;
    int v; 
    int nw = 1;

    struct bt t[32];
    memset(t, 0, 32 * sizeof(struct bt));
    total_bits = (unsigned)log2(n->cuts[d]);

    for(i=0; i<n->num[d]; i++) {
        v = n->comp_table[d][i];
        //printf("v %x\n", v);
        //printf("n->cuts[d] %x\n", n->cuts[d]);
        if(i!= n->num[d] - 1) {
            diff = n->comp_table[d][i+1] - n->comp_table[d][i];
            diff_bits = (unsigned)log2(diff);
            bt_insert(t, v, total_bits - diff_bits, total_bits, &nw);   
        }
        else {
            diff_bits = (unsigned)log2(n->cuts[d] 
                    - n->comp_table[d][i]);
            bt_insert(t, v, total_bits - diff_bits, total_bits,
                    &nw);
        }
        //printf("prefix %d, total %d\n", total_bits - diff_bits, total_bits);
    }

    return encode_bt(t, nw);
}

int node2anode(struct node *n, struct anode *an)
{
    int i;
    int cnt = 0;
    if(n->type == 4) {
        an->type |= 0x80;
        an->num = n->rules_inside;
        an->ps = calloc(1, n->rules_inside * sizeof(uint32_t));
        memcpy(an->ps, n->ps, n->rules_inside *sizeof(uint32_t));
    }
    else {
        int index;
        for (i= 1; i>= 0; i--) {
            an->type |= (n->dim[i] << (i*3));
            //printf("makecsb\n");

            //give a flag bit to show this dim has cuts
            //because this implementation only use 7 cuts
            //at most, so we only have 2(k-1)=12 bits
            //we have 4 bits to store the num
            if(n->num[i]) 
                an->csb[i] = (makecsb(n, i) | n->num[i]<<12);
            //printf("code %x\n", an->csb[i]);
        }

        if(n->num[0] && n->num[1])
            index = n->num[0] * n->num[1];
        else
            index = n->num[0];

        for(i = 0; i < index; i++) {
            if(((struct node**)(n->ps))[i]) {
                an->epb |= (1ULL<<i);
                cnt ++;
            }
        }

        if(n->rule_in_node != -1)
            an->filter = n->rule_in_node;
        else
            an->filter = 0xFFFFFFFF;

    }
    return cnt;

}

inline int ones(uint16_t csb, int i)
{
    //if(i==0)
    //    return 0;
    return __builtin_popcount(csb & (0xffff>>(15-i)));
}

inline int zeros(uint16_t csb, int i)
{
    return i - __builtin_popcount(csb & (0xffff>>(16-i)));
}

inline int decode_csb(uint16_t csb, uint32_t f, int *p)
{
    int bits_cnt = 1;
    int pos = (f >> 31);
    f<<=1;

    while(csb & (1<<pos)) {
        pos = 2 * ones(csb, pos) + (f >> 31);
        f<<=1;
        bits_cnt ++;
    }
    
    *p = zeros(csb, pos);

    return bits_cnt;
}


int bits_extra[5] = {0, 0, 16, 16, 24};
int search_rules_abcii(struct anode *n, uint32_t *ft)
{
    int bits_pos[5] = {0,0,0,0,0};
    uint32_t min_priority = 0xFFFFFFFF;
    int dim0;
    int dim1;
    int n1;
    int n0;
    int next;
    int ret;


    while(!(n->type & 0x80)) {
        dim1 = n->type >> (1*3);
        dim0 = n->type & 0x07;

        if(n->csb[1] & 0xf000){
            bits_pos[dim1] += decode_csb(n->csb[1], 
                    (ft[dim1]<<(bits_extra[dim1]+bits_pos[dim1])), &n1);
        }
        else
            n1 = 0;

        bits_pos[dim0] += decode_csb(n->csb[0],
                (ft[dim0]<<(bits_extra[dim0] + bits_pos[dim0])), &n0);

        next = n1*(n->csb[0]>>12) + n0;
        

        if(n->filter!= 0xFFFFFFFF) {
            ret = check_rule_p(n->filter, ft);
            if((uint32_t) ret < min_priority) 
                min_priority = ret;
        }

        if(n->epb & (1ULL << (next))) {
            if(next != 0) {
                next = __builtin_popcountll((n->epb << (64 - next)));
            }
            n = (struct anode*)(n->ps) + next;
        }
        else {
            return min_priority;
        }
        
    }

    ret = linear_search_abcii(n, ft);

    if((uint32_t) ret < min_priority)
        min_priority = ret;

    return min_priority;
}


struct anode* convert_to_abc_tree(struct tree_info* info)
{
    printf("converting to the ABC-II tree\n");
    struct anode** level_node = (struct anode**)malloc(info->
            level_cnt * sizeof(*level_node));
    int i;
    for(i = 0; i< info->level_cnt; i++) {
        level_node[i] = (struct anode*)calloc(1, 
                info->level_node_cnt[i] * sizeof(struct anode));
    }

    int node_cnt = 0;
    int curr_level = 0;
    int next_level_cnt = 0;
    int child_cnt = 0;

    struct anode * curr_anode = NULL;
    struct node* curr_node = NULL;

    while(curr_level < info->level_cnt) {
        curr_anode = level_node[curr_level] + node_cnt;
        curr_node = info->level_node[curr_level] + node_cnt;
        child_cnt = node2anode(curr_node, curr_anode);
        if(!curr_anode->ps)
            curr_anode->ps = level_node[curr_level + 1] + next_level_cnt;
        next_level_cnt += child_cnt; 

        node_cnt ++;
        if(node_cnt == info->level_node_cnt[curr_level]) {
            node_cnt = 0;
            curr_level ++;
            next_level_cnt = 0;
        }
    }
    return level_node[0];
}


#endif



#if 0
int main(int argc, char *argv[])
{
    printf("node size %d\n", sizeof(struct node));
    parseargs(argc, argv);
    rule_set_cnt = loadrules(fpr);
    printf("load rules %d\n", rule_set_cnt);

#ifndef HICUTS
    struct node * root;
    struct tree_info info;

    root = load_tree(ifp, &info);
#else
    struct hinode * hiroot;
    struct tree_info_hi info;

    hiroot = load_tree_hi(ifp, &info);
#endif

#ifdef HYPERCUTS
    struct hnode * hroot;
    hroot = convert_to_hypercuts_tree(&info);
#endif
#ifdef ABCII
    struct anode * aroot;
    aroot = convert_to_abc_tree(&info);
#endif



#if 0
    int ret;
    int ret2;
    int trace_no = 0;
    int flag = 0;
    uint32_t ft[MAXDIMENSIONS];
    while((trace_no = load_ft(fpt, ft)) != 0) {
         if(trace_no == 30)
            printf("here\n");
#if !defined(HYPERCUTS) && !defined(ABCII) && !defined(HICUTS)
        ret = search_rules(root, ft);
#elif defined(HYPERCUTS)
        ret = search_rules_hypercuts(hroot, ft);
#elif defined(ABCII)
        ret = search_rules_abcii(aroot, ft);
#elif defined(HICUTS)
        ret = search_rules_hi(hiroot, ft);
#endif
        ret2 = g_linear_search(ft);
        if(ret != ret2) {
            printf("ret %d, ret2 %d\n",ret, ret2);
            printf("trace no: %d\n", trace_no);
        }
        if(!flag) 
            flag = 1;
    }

    if(flag == 1) {
        printf("matching finished\n");
    }
#endif
#if 1 
    uint32_t *ft = (uint32_t *)malloc(1000000 * MAXDIMENSIONS * sizeof(uint32_t));
    int i = 0;
    int trace_cnt = 0;

    while(load_ft(fpt, ft+i*MAXDIMENSIONS) && i < 1000000){
        i++;
    }
    trace_cnt = i;
    long total_time = 0;
    printf("loading %d entries\n", trace_cnt);


    struct timespec tp_a, tp_b;
    int ret;
    clock_gettime(CLOCK_MONOTONIC, &tp_b);
    for(i=i-1;i>=0;i--) {
#if !defined(HYPERCUTS) && !defined(ABCII) && !defined(HICUTS)
        ret = search_rules(root,ft+i*MAXDIMENSIONS);
#elif defined(HYPERCUTS)
        ret = search_rules_hypercuts(hroot,ft+i*MAXDIMENSIONS);
#elif defined(ABCII)
        ret = search_rules_abcii(aroot,ft+i*MAXDIMENSIONS);
#elif defined(HICUTS)
        ret = search_rules_hi(hiroot, ft+i*MAXDIMENSIONS);
#endif

    }
    clock_gettime(CLOCK_MONOTONIC, &tp_a);

    printf("%lu %lu\n", tp_a.tv_sec, tp_a.tv_nsec);
    printf("%lu %lu\n", tp_b.tv_sec, tp_b.tv_nsec);
    total_time = ((long)tp_a.tv_sec - (long)tp_b.tv_sec) * 1000000000ULL + 
            (tp_a.tv_nsec - tp_b.tv_nsec);

    printf("time: %ld\n", total_time); 
    double ave_time = (double)total_time / trace_cnt;
    printf("ave time: %.1fns\n", ave_time);
    double gbps = (1/ave_time) * 1e9 / 1.48e6;
    printf("gbps: %.1fGbps\n", gbps);  

#endif
    
    return 0;

}


#endif

struct table_entry * load_hs(FILE *fp) 
{
    unsigned int numentry;
    fread(&numentry, sizeof(uint32_t), 1, fp);
    printf("Hs Table %d\n", numentry);
    struct table_entry *root = (struct table_entry*) calloc(numentry, sizeof(struct table_entry));
#ifdef STAT
    total_size += numentry * sizeof(struct table_entry);
#endif

    if(!root) {
        printf("No memory\n");
        exit(-1);
    }
    unsigned int i = 0;

    struct dump_entry de;
    for(; i< numentry; i++) {
        fread(&de, sizeof(struct dump_entry), 1, fp);
        if(de.type == 0) {
            (root+i)->type = 0;
            (root+i)->dim = de.dim;
            (root+i)->mp = de.mp;
            (root+i)->index = de.next + root;
            if((root+i)->index == 0x18) {
                printf("here\n");
            }
        }
        else {
            uint32_t *rules;
            rules = (uint32_t *)malloc(de.dim * sizeof(uint32_t));
#ifdef STAT
            total_size += de.dim * sizeof(uint32_t);
#endif
            fread(rules, sizeof(uint32_t), de.dim, fp); 
            (root+i)->type = 1;
            (root+i)->dim = de.dim;
            (root+i)->mp = 0;
            (root+i)->index = rules;
        }
    }
    return root;
}

inline int hs_linear_search(struct table_entry *n, uint32_t *ft)
{
    int ret = -1;
    int i;
    uint32_t *in = n->index;
    for(i=0;i<n->dim;i++) {
        ret = check_rule(in[i], ft); 
#ifdef STAT
        acc++;
#endif
        if(ret == 1) {
            ret = in[i];
            break;
        }
    }
    return ret;
}



int lookup_table(struct table_entry *rootnode, uint32_t *ft) 
{
    struct table_entry * node;

    node = rootnode;
    while (node->type != 1) {
        if (ft[node->dim] <= node->mp) {
            node = node->index;
        }
        else
            node = (struct table_entry*)node->index + 1;
#ifdef STAT
            acc++;
#endif
    }

    return hs_linear_search(node, ft);
}






