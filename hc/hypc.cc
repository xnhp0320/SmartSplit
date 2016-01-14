#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <list>
#include <map>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <cmath>
#include <vector>
#include <arpa/inet.h>
#include <string.h>


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

#include "hypc.h"
#include "sp_tree.h"

vector<pc_rule> classifier;
//list <pc_rule*> filter_rules; 
FILE *fpr;
FILE *fpt;
FILE *ofp;
FILE *ifp;


int hypercuts = 0; 
int bucketSize = 16;
int node_cnt = 0;
int spfac = 4;
int part_on_cut = 0;
int level_r = 0;


#define MAX_CUTS 65535 
#define MAX_CUTS_FOR_BITMAP_OP 64
#define LIMIT_CUTS 



//int select_small_ranges = 1;

double IPbin = 0.05;
double bin = 0.5;

void remove_redund(node *curr_node);
void NodeStats(node *curr_node);
void show_stats();
int check_s(node *curr_node, int dim);

//stats
int node_count = 0;
int total_move_up_rules = 0;
int level = 0;
int max_level_num = 0;
int max_level = 0;
int max_level_depth = 0;
int mem_acc = 0;
int mem_acc_max = 0;
int mem_acc_sum = 0;

int cuts_on_sip = 0;
int cuts_on_dip = 0;
int cuts_on_sp = 0;
int cuts_on_dp = 0;
int cuts_on_protocol = 0;
int cuts_total = 0;

int leaf_count = 0;
int nonleaf_count = 0;
int small_count = 0;
//part on cut

list<pc_rule*> filter;


//leaves depth count
map<int,int> leaves_depth;
map<int,int> node_depth;
map<pc_rule*, int> rule_in_leaves;
map<int,int> cuts_stats;
map<int,int> sn_depth;

#define LEAF_ 4 
#define PTR_SIZE 4

//node memory for hybrid algorithm:
//1 bytes for isleaf, dimension chosen, cut chosen
// 1 + 5 + 2 = 8 bits
//boundary -> 12B = 8B min + 4B num
//for nodiv
//S -1 1B
//S -2 1B
//for div
//S-1 4B
//S-2 4B
//4B for pointer

// so for div version:
//     1 + 12 + 8 + 4 = 25B
// for no_div version
//     1 + 12 + 2 + 4 = 19B

// for hybridcuts
//     19B + 32B = 51B
// for abc 
//     we use 18B for one node


//for hypercuts 
//4 config
//1 - BITMAP_OP && MOVERULEUP 18
//2 - NO_DIV && MOVERULEUP 23
//3 - NO_DIV && !MOVERULEUP 19
//4 - !NO_DIV && !MOVERULEUP 25
//choose 1 and 3 

//for hybridcuts
//2 config
//1 - BTIMAP_OP && !MOVERULEUP 59
//2 - NO_DIV && !BITMAP && !MOVERULEUP 51
//chose 1 and 2

//type
int hypercuts_type = 0;
int hybridcuts_type = 0;
int evenness = 0;
int hybrid_node = 0;
int hypercuts_node = 0; 
int predim = -1;

int no_div = 0;
int bitmap_op = 0;
int moveruleup = 0;


void check_config()
{
    if(hypercuts) {
        hybridcuts_type = 0;
        evenness = 0;
        if(hypercuts_type == 1) {
            bitmap_op = 1;
            moveruleup = 1;
            no_div = 0;
        }
        else if(hypercuts_type == 2) {
            bitmap_op = 0;
            moveruleup = 0;
            no_div = 1;
        }
    }
    else{
        if(evenness) {
            bitmap_op =1;
            moveruleup = 1;
            no_div = 0;
        }
        else if(hybridcuts_type == 1) {
            bitmap_op = 1;
            no_div = 1;
            moveruleup = 0;
        }
        else if (hybridcuts_type == 2) {
            bitmap_op = 0;
            no_div = 1;
            moveruleup = 0;
        }

        hypercuts_type = 0;
    }

    if(!hypercuts) { 
        if(evenness){
            printf("Building ABC-II Tree\n");
            hybrid_node = 18; 
        }
        else if (hybridcuts_type == 1) {
            printf("Building HybridCuts Tree with BITMAP_OP\n");
            hybrid_node = 59;
        }
        else if (hybridcuts_type == 2) {
            printf("Building HybridCuts Tree without BITMAP_OP\n");
            hybrid_node = 51;
        }
        else {
            printf("wrong config\n");
            exit(-1);
        }
    }
    else {
        if(hypercuts_type == 1) {
            printf("Building HyperCuts Tree with BITMAP_OP and RuleMoveUp\n"); 
            hypercuts_node = 19;
        }
        else if (hypercuts_type == 2) {
            printf("Building HyperCuts Tree with RegionCompaction and NodeMerging\n"); 
#ifdef HYPERCUTS
            hypercuts_node = 19;
#endif
#ifdef HICUTS
            hypercuts_node = 12;
#endif
        }
        else {
            printf("wrong config\n");
            exit(-1);
        }
    }

    printf("\tno_div = %d\n", no_div);
    printf("\tbitmap_op = %d\n", bitmap_op);
    printf("\tmoveruleup =  %d\n", moveruleup);
    printf("======================\n");
}


unsigned int mem_total = 0;
unsigned int mem_pointer = 0;
unsigned int mem_leaf = 0;
unsigned int mem_header = 0;


void clear_stats()
{
    mem_total = 0;
    mem_pointer = 0;
    mem_leaf = 0;
    mem_header = 0;

    leaves_depth.clear();
    rule_in_leaves.clear();

    level = 0;
    node_count = 0;
    cuts_stats.clear();
    node_depth.clear();

    max_level_num = 0;
    max_level = 0;
    max_level_depth = 0;
    mem_acc = 0;
    mem_acc_max = 0;

    cuts_on_sip = 0;
    cuts_on_dip = 0;
    cuts_on_sp = 0;
    cuts_on_dp = 0;
    cuts_on_protocol = 0;
    cuts_total = 0;

    
    leaf_count = 0;
    nonleaf_count = 0;
    small_count = 0;
    sn_depth.clear();
}

#if 0
int more_mem(const vector<node*> &ps)
{
    unsigned i;
    int cnt = 1;
    
    for(i=1;i < ps.size(); i++) {
        if(ps[i] != ps[i-1])
            cnt ++;
    }

    vector<node*> tmp = ps;
    sort(tmp.begin(), tmp.end());
    vector<node*>::iterator it = unique(tmp.begin(), tmp.end());
    tmp.erase(it, tmp.end());

    if(tmp[0] == NULL)
        tmp.erase(tmp.begin());

    int m = cnt - tmp.size();
    if(m>1) {
        printf("ps size %u", ps.size());
        printf(" m %d ", m);
        printf(" tmp %u ", tmp.size());
        printf(" cpr %u ", cnt);
        for(i=0;i<ps.size();i++) 
            printf(" %p ", ps[i]);
        printf("\n");
    }
    return m;
}
#endif

void check_cuts(node *n)
{
    if(n->isLeaf)
        return;

    if(n->depth > 5)
        return;

    for(int i = 0; i < MAXDIMENSIONS; i++) {
        if(n->cuts[i] > 1) {
#if 1
            if(i == 0)
                cuts_on_sip += n->cuts[i];
            if(i == 1)
                cuts_on_dip += n->cuts[i];
            if(i == 2)
                cuts_on_sp += n->cuts[i];
            if(i == 3)
                cuts_on_dp += n->cuts[i];
            if(i == 4)
                cuts_on_protocol += n->cuts[i];
            cuts_total += n->cuts[i];
#endif
#if 0
            if(i == 0)
                cuts_on_sip += 1;
            if(i == 1)
                cuts_on_dip += 1;
            if(i == 2)
                cuts_on_sp += 1;
            if(i == 3)
                cuts_on_dp += 1;
            if(i == 4)
                cuts_on_protocol += 1;
            cuts_total += 1; 
#endif
        }
    }
}


void NodeStats(node *curr_node)
{
    node_count ++;

    if(curr_node->isLeaf)
        leaf_count ++;
    else
        nonleaf_count ++;

    if(!curr_node->isLeaf){
        if(curr_node->classifier.size() <= 2 * (size_t)bucketSize) {
            small_count ++;
            sn_depth[curr_node->depth]++;
        }
    }

    if(!hypercuts){
        if(!curr_node->isLeaf) { 
            mem_total += hybrid_node;
            mem_header += hybrid_node;
        }
        else{
            mem_total += PTR_SIZE * curr_node->classifier.size();
            mem_leaf += PTR_SIZE * curr_node->classifier.size();
        }
    }
    else {
        if(!curr_node->isLeaf) {
            mem_total += hypercuts_node;
            mem_header += hypercuts_node;
        }
        else {
            mem_total += PTR_SIZE * curr_node->classifier.size();
            mem_leaf += PTR_SIZE * curr_node->classifier.size();
        }
    }

    if (hypercuts_type == 2 || hybridcuts_type == 2){
        mem_total += curr_node->ps.size() * PTR_SIZE;
        mem_pointer += curr_node->ps.size() * PTR_SIZE;
    }

    if(hybridcuts_type == 1) {
        if(curr_node->ps.size() > 64) {
            mem_total += curr_node->ps.size() * PTR_SIZE;
            mem_pointer += curr_node->ps.size() * PTR_SIZE;
        }
    }

    
    if(curr_node->node_has_rules) {
        total_move_up_rules += curr_node->node_has_rules;
    }

    mem_acc = curr_node->acc;

    if(curr_node->depth > level)
        level = curr_node->depth;

    if(mem_acc > mem_acc_max) {
        mem_acc_max = mem_acc;
#if 0
        int cuts = 1;
        int chosen = 0;

        for(int i = 0; i < MAXDIMENSIONS; i++) {
            if(curr_node->cuts[i] > 1) {
                if(curr_node->ub_mask[i]) {
                    cuts *= curr_node->fuse_array_num[chosen];
                }
                else {
                    cuts *= curr_node->cuts[i];
                }
                chosen ++;
            }
        } 
#endif
    }

    check_cuts(curr_node);



    
    if(curr_node->children.size() == 0) {
        //leaves
        leaves_depth[curr_node->depth] ++;
    }

    //if(!curr_node->isLeaf)
    node_depth[curr_node->depth] ++;

    if(curr_node->depth<=LEAF_ && curr_node->children.size() == 0) {
        for(list<pc_rule*>::iterator it = curr_node->classifier.begin(); it != curr_node->classifier.end();it++ ) {
            rule_in_leaves[*it] ++;
        }
    }


    if(!curr_node->isLeaf) {
#if 0
        int cuts = 1;
        int chosen = 0;

        for(int i = 0; i < MAXDIMENSIONS; i++) {
            if(curr_node->cuts[i] > 1) {
                if(curr_node->ub_mask[i]) {
                    cuts *= curr_node->fuse_array_num[chosen];
                }
                else {
                    cuts *= curr_node->cuts[i];
                }
                chosen ++;
            }
        } 
#endif
        cuts_stats[curr_node->ps.size()] ++;
    }

    if(curr_node->isLeaf) {
        if(mem_acc + (int)curr_node->classifier.size() > max_level){
            max_level = mem_acc + curr_node->classifier.size();
            max_level_num = curr_node->classifier.size();
            max_level_depth = mem_acc;
        }
        mem_acc_sum += (mem_acc + curr_node->classifier.size());
    }

    
}

void show_stats()
{
    printf("*****************************\n");
    printf("node count : %d\n", node_count);
    printf("leaf count : %d\n", leaf_count);
    printf("non-leaf count : %d\n", nonleaf_count);
    printf("small count : %d %.2f\n", small_count, (double)small_count/nonleaf_count);

    printf("move_up rules : %d\n", total_move_up_rules);
    printf("max_levels: %d\n", level);
    printf("max memory access: %d\n", max_level);
    printf("max memory access level: %d\n", max_level_depth);
    printf("max memory access num: %d\n", max_level_num);
    printf("average memory access num: %.2f\n", (double)mem_acc_sum/leaf_count);
    printf("cuts stats\n");
    printf("cuts on sip %d %.2f\n", cuts_on_sip, (double)cuts_on_sip/cuts_total);
    printf("cuts on dip %d %.2f\n", cuts_on_dip, (double)cuts_on_dip/cuts_total);
    printf("cuts on sp  %d %.2f\n", cuts_on_sp, (double)cuts_on_sp/cuts_total);
    printf("cuts on dp %d  %.2f\n", cuts_on_dp, (double)cuts_on_dp/cuts_total);
    printf("cuts on pro %d %.2f\n", cuts_on_protocol, (double)cuts_on_protocol/cuts_total);
    printf("cuts total %d \n", cuts_total);

    map<int,int>::iterator it;
    //for(it = leaves_depth.begin(); it != leaves_depth.end(); it++) {
    //    printf("depth %d nodes %d\n", it->first, it->second);
    //}

    for(it = node_depth.begin(); it != node_depth.end(); it++) {
        printf("depth %d nodes %d\n", it->first, it->second);
    }

    for(it = cuts_stats.begin(); it != cuts_stats.end(); it++) {
        printf("cuts %d %d\n", it->first, it->second);
    }

    for(it = sn_depth.begin(); it != sn_depth.end(); it++) {
        printf("sn depth %d %d\n", it->first, it->second);
    }

    printf("leaves < %d: %lu\n", LEAF_,rule_in_leaves.size());
    printf("filer rules: %lu\n", filter.size());
    printf("memory %u\n", mem_total);
    printf("pointer memory %u\n", mem_pointer);
    printf("header memory %u\n",  mem_header);
    printf("leaf memory %u\n",  mem_leaf);
    //dump_rules(filter, "filter");


}

int CheckIPBounds(range fld)
{
    if (fld.low > 0xFFFFFFFF)
    {
        printf("Error: IPRange is buggy!(%llu)\n",fld.low);
        return 1;
    }
    if (fld.high > 0xFFFFFFFF)
    {
        printf("Error: IPRange is buggy!(%llu)\n",fld.high);
        return 1;
    }
    if (fld.low > fld.high)
    {
        printf("Error: IPRange is buggy!(%llu - %llu)\n",fld.low,fld.high);
        return 1;
    }
    return 0;
}

int CheckPortBounds(range fld)
{
    if (fld.low > 0xFFFF)
    {
        printf("Error: PortRange is buggy!(%llu)\n",fld.low);
        return 1;
    }
    if (fld.high > 0xFFFF)
    {
        printf("Error: PortRange is buggy!(%llu)\n",fld.high);
        return 1;
    }
    if (fld.low > fld.high)
    {
        printf("Error: PortRange is buggy!(%llu - %llu)\n",fld.low,fld.high);
        return 1;
    }
    return 0;
}

int CheckProtoBounds(range fld)
{
    if (fld.low > 0xFF)
    {
        printf("Error: ProtoRange is buggy!(%llu)\n",fld.low);
        return 1;
    }
    if (fld.high > 0xFF)
    {
        printf("Error: ProtoRange is buggy!(%llu)\n",fld.high);
        return 1;
    }
    if (fld.low > fld.high)
    {
        printf("Error: ProtoRange is buggy!(%llu - %llu)\n",fld.low,fld.high);
        return 1;
    }
    return 0;
}




void IP2Range(unsigned ip1,unsigned ip2,unsigned ip3,unsigned ip4,unsigned iplen,pc_rule *rule,int index)
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

void ClearMem(node *curr_node)
{
    curr_node->classifier.clear();
    curr_node->children.clear();
    curr_node->ps.clear();
    delete curr_node;
}



void clear_mem_for_middle(node *curr_node)
{
    curr_node->classifier.clear();
    curr_node->children.clear();
    //delete curr_node;
#if 0 // here is for large rule set
    curr_node->ps.clear();
    delete curr_node;
#endif
}

void clear_mem_for_leaf(node *curr_node)
{
    curr_node->children.clear();
#if 0 
    curr_node->classifier.clear();
    delete curr_node;
#endif
}


int loadrules(FILE *fp) {
    int i = 0;
    int wild = 0;
    unsigned sip1, sip2, sip3, sip4, siplen;
    unsigned dip1, dip2, dip3, dip4, diplen;
    unsigned proto, protomask;
    unsigned junk, junkmask;
    unsigned pri;

    pc_rule rule;

    while(1) {
        wild = 0;
        if(fscanf(fp,"@%u.%u.%u.%u/%u\t%u.%u.%u.%u/%u\t%llu : %llu\t%llu : %llu\t%x/%x\t%x/%x\t%d\n",
                    &sip1, &sip2, &sip3, &sip4, &siplen, &dip1, &dip2, &dip3, &dip4, &diplen, 
                    &rule.field[2].low, &rule.field[2].high, &rule.field[3].low, &rule.field[3].high,
                    &proto, &protomask, &junk, &junkmask, &pri) != 19) break;

        //if(fscanf(fp,"@%u.%u.%u.%u/%u %u.%u.%u.%u/%u %llu : %llu %llu : %llu %x/%x\n",
        //            &sip1, &sip2, &sip3, &sip4, &siplen, &dip1, &dip2, &dip3, &dip4, &diplen, 
        //            &rule.field[2].low, &rule.field[2].high, &rule.field[3].low, &rule.field[3].high,
        //            &proto, & protomask) != 16) break;
        rule.siplen = siplen;
        rule.diplen = diplen;
        rule.sip[0] = sip1;
        rule.sip[1] = sip2;
        rule.sip[2] = sip3;
        rule.sip[3] = sip4;
        rule.dip[0] = dip1;
        rule.dip[1] = dip2;
        rule.dip[2] = dip3;
        rule.dip[3] = dip4;

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
        rule.priority = pri;
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
            classifier.push_back(rule);
        }
        i++;
    }
    return i;
}


void parseargs(int argc, char *argv[]) {
    int	c;
    while ((c = getopt(argc, argv, "r:t:h:pb:s:o:l:xy:ed:")) != -1) {
        switch (c) {
            case 'r':
                fpr = fopen(optarg, "r");
                break;
            case 't':
                fpt = fopen(optarg, "r");
                break;
            case 'h':
                hypercuts = 1;
                hypercuts_type = atoi(optarg);
                break;
            case 'b':
                bucketSize = atoi(optarg);
                break;
            case 'p':
                part_on_cut = 1;
                break;
            case 's':
                spfac = atoi(optarg);
                break;
            case 'o':
                ofp = fopen(optarg, "w");
                break;
            case 'l':
                ifp = fopen(optarg, "r");
                break;
            case 'x':
                level_r = 1;
                break;
            case 'y':
                hybridcuts_type = atoi(optarg);
                break;
            case 'e':
                evenness = 1;
                break;
            case 'd':
                predim = atoi(optarg);
                break;
            default:
                break;
        }
    }

    
    if(fpr == NULL){
        printf("can't open ruleset file\n");
        exit(-1);
    }

}

void load_rule_ptr(vector <pc_rule> &rule_list,list <pc_rule*> &ruleptr_list,int start,int end)
{
    printf("Rule:%d - %d\n",start,end);
    int count = 0;
    for (vector <pc_rule>::iterator i = rule_list.begin();i != rule_list.end();++i) 
    {
        if (count >= start && count <= end)
            ruleptr_list.push_back(&(*i));
        count++;
    }
}

//check range size this fucntion need to be improved in the future
#ifdef CHECK_OLD
int check_range_size(const range & check, const rule_boundary & rb, int index)
{
    double field;
    if(index == 0 || index == 1) {
        field = (((double)(check.high - check.low))/0xFFFFFFFF);
        if(field >= IPbin) {
            return 1;
        }
        else {
            //this is for range_in_boundary_1D
            //when the range is big, it should be reshaped into the 
            //range boundary.
            //
            if(check.low == rb.field[index].low &&
                    check.high == rb.field[index].high) {
                return 1;
            }
            else 
                return 0;
        }
    } 
    else if (index == 2 || index == 3) {
        field = (((double)(check.high - check.low))/65535);
        if(field >= bin) {
            return 1;
        }
        else 
            //this is for range_in_boundary_1D
            //when the range is big, it should be reshaped into the 
            //range boundary.
            //
            if(check.low == rb.field[index].low &&
                    check.high == rb.field[index].high) {
                return 1;
            }
            else 
                return 0;
    }
//usually the Protocol is a fixed value
    return 0;
}
#else
int check_range_size(const range & check, const rule_boundary & rb, int index)
{
    double field;
    field = (((double)(check.high - check.low))/(rb.field[index].high - rb.field[index].low));
    if(field >= bin) {
        return 1;
    } 
    else
        return 0;
}

#endif

void init_boundary(rule_boundary * boundary)
{
    boundary->field[0].high = 0xffffffff;
    boundary->field[1].high = 0xffffffff;
    boundary->field[2].high = 0xffff;
    boundary->field[3].high = 0xffff;
    boundary->field[4].high = 0xff;

    boundary->field[0].low = 0;
    boundary->field[1].low = 0;
    boundary->field[2].low = 0;
    boundary->field[3].low = 0;
    boundary->field[4].low = 0;
}

inline range range_in_boundary_1D(range r, range boundary)
{
    range ret;
    if (r.low > boundary.low) {
        ret.low = r.low; 
    }
    else {
        ret.low = boundary.low;
    }

    if (r.high < boundary.high) {
        ret.high = r.high;
    }
    else {
        ret.high = boundary.high;
    }
    return ret;
}

void dump_rule(pc_rule *i, FILE *out)
{
    fprintf(out, "@%d.%d.%d.%d/%d\t%d.%d.%d.%d/%d\t%lld : %lld\t%lld : %lld\t",
            (i)->sip[0], (i)->sip[1], (i)->sip[2], (i)->sip[3], (i)->siplen, \
            (i)->dip[0], (i)->dip[1], (i)->dip[2], (i)->dip[3], (i)->diplen, \
            (i)->field[2].low, (i)->field[2].high, \
            (i)->field[3].low, (i)->field[3].high);

    if((i)->field[4].high == 0xFF && (i)->field[4].low == 0)
        fprintf(out, "0x%x/0x%x\t",0,0);
    else
       fprintf(out, "0x%llx/0x%llx\t", (i)->field[4].low, 0xffull);

    fprintf(out, "0x%x/0x%x\n", 0,0);

}

void dump_rule_with_priority(pc_rule *i, FILE *out)
{
    fprintf(out, "@%d.%d.%d.%d/%d\t%d.%d.%d.%d/%d\t%lld : %lld\t%lld : %lld\t",
            (i)->sip[0], (i)->sip[1], (i)->sip[2], (i)->sip[3], (i)->siplen, \
            (i)->dip[0], (i)->dip[1], (i)->dip[2], (i)->dip[3], (i)->diplen, \
            (i)->field[2].low, (i)->field[2].high, \
            (i)->field[3].low, (i)->field[3].high);

    if((i)->field[4].high == 0xFF && (i)->field[4].low == 0)
        fprintf(out, "0x%x/0x%x\t",0,0);
    else
       fprintf(out, "0x%llx/0x%llx\t", (i)->field[4].low, 0xffull);

    fprintf(out, "0x%x/0x%x\t", 0,0);
    fprintf(out, "%d\n", i->priority);

}


void dump_boundary(node *curr_node) 
{
    for(int i = 0; i<MAXDIMENSIONS;i++) {
        printf("%d %llu - %llu\n", i, curr_node->boundary.field[i].low, curr_node->boundary.field[i].high);
    }

}

void dump_rules(list<pc_rule*> ruleset, string file)
{
    FILE *fp = fopen(file.c_str(), "w");

    for (list<pc_rule *>::iterator i = ruleset.begin();
            i != ruleset.end();
            i++){
        dump_rule(*i, fp);
    }
    fclose(fp);
} 

void dump_rules_with_priority(list<pc_rule*> ruleset, string file)
{
    FILE *fp = fopen(file.c_str(), "w");

    for (list<pc_rule *>::iterator i = ruleset.begin();
            i != ruleset.end();
            i++){
        dump_rule_with_priority(*i, fp);
    }
    fclose(fp);
} 

void dump_children_rules(node *curr_node, char *pname)
{
    int i = 0;
    char name[64];
    snprintf(name, 64, "%s%d", pname, i);
    for(list<node*>::iterator it = curr_node->children.begin();
            it != curr_node->children.end();
            it ++ ){
        dump_rules((*it)->classifier, name);
        i++; 
        snprintf(name, 64, "%s%d", pname, i);
    }
}



bool is_equal(pc_rule rule1, pc_rule rule2, rule_boundary boundary)
{
    int count = 0;
    range r1, r2;
    for (int i = 0;i < MAXDIMENSIONS;i++)
    {
        //if (rule1.field[i].low > boundary.field[i].low) {
        //    r1.low = rule1.field[i].low;
        //} else {
        //    r1.low = boundary.field[i].low;
        //}
        //if (rule1.field[i].high < boundary.field[i].high) {
        //    r1.high = rule1.field[i].high;
        //} else {
        //    r1.high = boundary.field[i].high;
        //}

        r1 = range_in_boundary_1D(rule1.field[i], boundary.field[i]);

        //if (rule2.field[i].low > boundary.field[i].low) {
        //    r2.low = rule2.field[i].low;
        //} else {
        //    r2.low = boundary.field[i].low;
        //}
        //if (rule2.field[i].high < boundary.field[i].high) {
        //    r2.high = rule2.field[i].high;
        //} else {
        //    r2.high = boundary.field[i].high;
        //}

        r2 = range_in_boundary_1D(rule2.field[i], boundary.field[i]);

        if (r1.low <= r2.low && r1.high >= r2.high)

        {
            count++;
        }
    }

    if (count == MAXDIMENSIONS)
        return true;
    else
        return false;
}

bool myequal(pc_rule* first, pc_rule* second)
{
    return (first->priority == second->priority);
}

void remove_redund(node *curr_node)
{
    list <pc_rule*> rulelist;
    //list <pc_rule*> dellist;
    //list <pc_rule*> cftlist;

    rulelist.clear();

    for (list<pc_rule*>::iterator rule = curr_node->classifier.begin();
            rule != curr_node->classifier.end();++rule)
    {
        int found = 0;
        for (list<pc_rule*>::iterator mule = rulelist.begin();
                mule != rulelist.end();++mule)
        {
            if (is_equal(**mule,**rule, curr_node->boundary) == true)
            {
                found = 1;
                //dellist.push_back(*rule);
                //cftlist.push_back(*mule);
                break;
            }
        }
        if (found != 1)
        {
            rulelist.push_back(*rule);
        }
    }
    // Now add back the rules 
    curr_node->classifier.clear();
    curr_node->classifier = rulelist;
    curr_node->classifier.unique(myequal);
    //dump_rules_with_priority(dellist, "del");
    //dump_rules_with_priority(cftlist, "cft");
}


node *init_root_node(list<pc_rule*> p_classifier)
{
    node* root = new node;
    root->depth = 1;
    root->acc = 1;
    root->isLeaf = 0;
    init_boundary(&root->boundary);
    root->children.clear();
    root->no = 0;
    root->node_has_rules = 0;
    root->hard = 0;
    for (int i=0;i < MAXDIMENSIONS;i++){
        root->cuts[i] = 1;
        root->ub_mask[i]=0;
    }
    
    memset(root->fuse_array, 0, MAX_CUT_FIELD * NUM_INDEX * sizeof(int));
    memset(root->fuse_array_num, 0, MAX_CUT_FIELD * sizeof(int));
    for (list <pc_rule*>::iterator i = p_classifier.begin();i != p_classifier.end();++i) 
    {
        root->classifier.push_back((*i));
    }

    remove_redund(root);
    dump_rules(root->classifier, "a");
    printf("after redund %lu\n", root->classifier.size());
    return root;
}



//void partition_on_cuts(int *chosen_dimension, int chosen, node *curr_node)
//{
//    int is_erase = 0;
//    for (list<pc_rule*>::iterator it = curr_node->classifier.begin();
//            it != curr_node->classifier.end();
//            ) {
//        is_erase = 0;
//
//        for(int i=0;i<chosen;i++) {
//            if(check_range_size((*it)->field[chosen_dimension[i]], chosen_dimension[i])) {
//                filter_rules.push_back((*it));
//                it = curr_node->classifier.erase(it);
//                is_erase = 1;
//                break;
//            }
//        }
//
//        if(!is_erase)
//            it++;
//
//    }
//
//    filter_rules.sort();
//    filter_rules.unique();
//    printf("removing %u rules\n", filter_rules.size());
//}





void init_node(node* n, range *rn, node* parrent_node)
{
    int flag;
    n->depth = parrent_node->depth + 1;
    n->acc = parrent_node->acc + 1;
    if(!bitmap_op) {
        n->acc ++;
    }
    
    for(int i = 0; i < MAXDIMENSIONS; i++) {
        n->cuts[i] = 1;
        n->ub_mask[i] = 0;
        n->boundary.field[i] = rn[i];
    }

    n->node_has_rules = 0;
    n->no = node_cnt;
    n->isLeaf = 0;
    n->hard = 0;
    node_cnt++;

    memset(n->fuse_array, 0, MAX_CUT_FIELD * NUM_INDEX * sizeof(int));
    memset(n->fuse_array_num, 0, MAX_CUT_FIELD *  sizeof(int));

    for (list<pc_rule*>::iterator it = parrent_node->classifier.begin();
            it != parrent_node->classifier.end();
            ++ it) {
        flag = 1;

        for(int j=0; j < MAXDIMENSIONS; j++) {
            if ( (*it)->field[j].low > rn[j].high || 
                    (*it)->field[j].high < rn[j].low){
                flag = 0;
                break;
            }
        }

        if(flag == 1) {
            n->classifier.push_back(*it);
            //dump_rule(*it, stdout);
        }
    }
    n->children.clear();
    remove_redund(n);
    
}


//you can not pick a perfect method to choose s.
//if you use ceil(range/cuts), s is bigger, however, if s is large enough
//it will eliminate the end range
//for example [0,119] cutting to 16 pieces
//s = (119-0+1)/16 = 7.5
//if you use ceil(s) = 8
//then you end up 15 pieces.
//
//however if you don't use ceil
//you end up some segment end point which is large then cuts
//and even some case, s is reduced to 0


int multidim_cutting(node *curr_node, int *chosen_dimension, int chosen, range *rn, int old_index)
{
    unsigned long long s;
    range r;
    int pindex = old_index;
    
    r = curr_node->boundary.field[chosen_dimension[chosen]];
    s = ceil((double)(r.high - r.low + 1)/curr_node->cuts[chosen_dimension[chosen]]);
  
    if(no_div) {
        if ( (1UL << (unsigned)log2(s)) != s) {
            //printf("we need div %llu\n", s);
            s = 1ULL<<((unsigned)ceil(log2(s)));
        }
    }
    


    //if(s != (r.high - r.low +1)/curr_node->cuts[chosen_dimension[chosen]]) {
    //    printf("%d\n", s);
    //}


    if ( curr_node->ub_mask[chosen_dimension[chosen]] ) {
        rn[chosen_dimension[chosen]].low = r.low;
        rn[chosen_dimension[chosen]].high = (r.low + curr_node->fuse_array[chosen][1] * s - 1);
    }
    else {
        rn[chosen_dimension[chosen]].low = r.low;
        rn[chosen_dimension[chosen]].high = (r.low + s - 1);
    }
    int iter = 1;

    //equi-size fuse_array is 0
    //
    //int maxi = max(curr_node->cuts[chosen_dimension[chosen]], fuse_max_entry(curr_node->fuse_array[chosen_dimension[chosen]])); 
    int i = 0;

    for(;;)
    {
        if(chosen > 0) 
            pindex = multidim_cutting(curr_node, chosen_dimension, chosen -1, rn, pindex);
        else {
            int flag;
            int empty = 1;

            for (list<pc_rule*>::iterator it = curr_node->classifier.begin();
                    it != curr_node->classifier.end();
                    ++ it) {

                flag = 1;
                for(int j=0; j < MAXDIMENSIONS; j++) {
                    if ( (*it)->field[j].low > rn[j].high || 
                            (*it)->field[j].high < rn[j].low){
                        flag = 0;
                        break;
                    }
                }
                if(flag == 1) {
                    empty = 0;
                    break;
                }
            }

            if(!empty) {
                node * new_child = new node;
                init_node(new_child, rn, curr_node);
                curr_node->children.push_back(new_child);
                curr_node->ps[pindex] = new_child;
            }
            else {
                //printf("generate an empty range\n");
                curr_node->ps[pindex] = NULL;
            }

        }

        if(curr_node->ub_mask[chosen_dimension[chosen]]) {
            int fuse_num = curr_node->fuse_array_num[chosen];
            //i = curr_node->fuse_array[chosen][iter];
            iter ++;
            if(iter < fuse_num) {
                rn[chosen_dimension[chosen]].low = rn[chosen_dimension[chosen]].high + 1; 
                //rn[chosen_dimension[chosen]].high = (rn[chosen_dimension[chosen]].low + 
                //        (curr_node-> fuse_array[chosen][iter] - curr_node->fuse_array[chosen][iter-1]) * s) - 1; 
                rn[chosen_dimension[chosen]].high = r.low + curr_node->fuse_array[chosen][iter] * s - 1;
                pindex ++;
            }
            else if(iter == fuse_num) {
                rn[chosen_dimension[chosen]].low = rn[chosen_dimension[chosen]].high + 1; 
                rn[chosen_dimension[chosen]].high = r.high; 
                pindex ++;
                if(rn[chosen_dimension[chosen]].high < rn[chosen_dimension[chosen]].low) {
                    printf("something wrong for unbalance cutting\n");
                    exit(-1);
                }
            }

            else
                break;
        }
        else {
            ++i;
#if 0 
            //old processing
            //has already created too much useless nodes.

            if(i>=curr_node->cuts[chosen_dimension[chosen]])
                break;
            else
                pindex ++;

            rn[chosen_dimension[chosen]].low = rn[chosen_dimension[chosen]].high + 1; 
            rn[chosen_dimension[chosen]].high = (rn[chosen_dimension[chosen]].low + s - 1); 

            if(rn[chosen_dimension[chosen]].high > r.high) {
                rn[chosen_dimension[chosen]].high = r.high;
            }
            if(rn[chosen_dimension[chosen]].low > rn[chosen_dimension[chosen]].high) {
                printf("something wrong for balance cutting\n");
                printf("cuts == %d, s == %llu, r.low = %llu, r.high = %llu\n", 
                        curr_node->cuts[chosen_dimension[chosen]],
                        s,
                        r.low,
                        r.high);
                rn[chosen_dimension[chosen]].low = rn[chosen_dimension[chosen]].high;

                //exit(-1);
            }
#endif
            if(i>=curr_node->cuts[chosen_dimension[chosen]])
                break;
            else{
                range test;
                test.low = rn[chosen_dimension[chosen]].high + 1;
                test.high = test.low + s -1;

                if(test.high > r.high) {
                    test.high = r.high;
                }
                if(test.low > test.high) {
                    //printf("we have a larger s which makes the cutting small\n");
                    //printf("cuts == %d, s == %llu, r.low = %llu, r.high = %llu\n", 
                    //    curr_node->cuts[chosen_dimension[chosen]],
                    //    s,
                    //    r.low,
                    //    r.high);
                    //we don't need to create any new nodes.
                    break;
                }
                rn[chosen_dimension[chosen]].high = test.high;
                rn[chosen_dimension[chosen]].low = test.low;
                pindex ++;
            }
       }
    }
    return pindex;
}

int check_s(node *curr_node, int dim)
{
    int cuts = curr_node->cuts[dim];
    range rdim = curr_node->boundary.field[dim];
    unsigned long long s = ceil((double)(rdim.high - rdim.low + 1)/cuts); 

    range test;
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

void init_pointer_array(node *curr_node, int *chosen_dimension, int chosen)
{
    int cnt = 1;
    for(int i = 0; i<chosen;i++) {
        //ub cuts nodes
        if(curr_node->ub_mask[chosen_dimension[i]]) {
            cnt *= curr_node->fuse_array_num[i];
        }
        else {
            cnt *= check_s(curr_node, chosen_dimension[i]);
        }
    }
    //printf("for this nodes pointer array %d\n", cnt);

    curr_node->ps.resize(cnt);
}

void perform_cuts(node *curr_node, int *chosen_dimension, int chosen)
{
    if(curr_node->hard == 1)
        return;

    range rn[MAXDIMENSIONS];
    for(int i=0; i < MAXDIMENSIONS; i++) {
        rn[i] = curr_node->boundary.field[i];
    }

    init_pointer_array(curr_node, chosen_dimension, chosen);
    

    //chosen == number, but I need a index here
    multidim_cutting(curr_node, chosen_dimension, chosen-1, rn, 0);
    
}

bool less_range_func(const range & a, const range &b)
{
    if(a.low != b.low)
        return a.low < b.low;
    else 
        return a.high < b.high;
    //return a.low < b.low;
}
typedef bool (*less_range)(const range &a, const range &b);


#define NIPQUAD(addr) \
    ((unsigned char *)&addr)[0], \
    ((unsigned char *)&addr)[1], \
    ((unsigned char *)&addr)[2], \
    ((unsigned char *)&addr)[3]

int cutting_guess(node *curr_node, map<range, int, less_range> & range_map, int d,
        int *fuse_array, int *fuse_array_num) 
{
    int large_prefix = 0;
    int small_prefix = 0;
    
       
    split_tree tr(curr_node->boundary.field[d], bucketSize, 16);

    for(map<range, int, less_range>::iterator mit = range_map.begin();
            mit != range_map.end();
            ++mit) {
    
        if(!evenness) {
#ifdef OLD_SPLIT
            if(check_range_size(mit->first, curr_node->boundary, d)) {
                large_prefix += mit->second;
            }
            else {
                //struct in_addr addr;
                //addr.s_addr = htonl(mit->first.low);
                //printf("%s %d\n", inet_ntoa(addr), prefix_length);


                small_prefix += mit->second;
                tr.split_add_range(mit->first, mit->second);
            }
#else
            tr.split_add_range(mit->first, mit->second);
#endif

        }
        else {
            tr.split_add_range(mit->first, mit->second);
        }

    }

    if(evenness) 
        tr.split_create_evenness();
    else
#ifdef OLD_SPLIT
        tr.split_create();
#else
        tr.split_create_cost();
#endif
    //if(large_prefix == 0 ) {
    //    printf("small nodes\n");
    //}

    //if(curr_node->depth == 4 && d == 0) {
    //    printf("here");
    //}

    //tr.split_create_breadth();

    //double tr_ave = tr.ave_height();

    //ave_length = length_sum / (large_prefix + small_prefix); 

    //printf("dimension %d: %d\n", d, large_prefix + small_prefix);
    //printf("\tlarge rule %d, small rule %d\n", large_prefix, small_prefix);
    //printf("\tave_length %lf\n", ave_length);
    //printf("\ttr_h %d\n", tr_h);
    //printf("\ttr_ave %lf\n", tr_ave);


    //printf("range fushion:\n");
    //for(map<int, int>::iterator mit = prefix_hist.begin();
    //        mit != prefix_hist.end();
    //        ++mit) {
    //    printf("%d %d\n", mit->first, mit->second);

    //}
    //char name[16];
    //sprintf(name, "%d-%d", curr_node->depth, d);
    ////
    //tr.dump_tree(name);

    int btd = tr.balance_tree_depth(); 
    //printf("balance_tree_depth %d\n", btd);
    //if(btd == 14) {
    //    printf("here\n");
    //    dump_rules(curr_node->classifier, "xxx");
    //}

    if (btd < BALANCE_TREE_DEPTH) {
        //printf("perform equip-dense cutting\n");
        btd = tr.unbalance_tree_depth(fuse_array, fuse_array_num);
        //printf("btd %d\n",btd);
        if(*fuse_array_num > NUM_INDEX)
        {
            printf(" fuse_array num > NUM_INDEX\n");
            exit(-1);
        }
    }
    else {
        *fuse_array_num = 0;
    }

    return btd;

} 

//void hist_prefix_length(node *curr_node, int *chosen_dimension, int chosen, vector< map<range, int, less_range> > & range_map, int *btd) 
//{
//    //int btd[5] = {0,0,0,0,0};
//
//    if(chosen == 0) {
//        printf("error no dimension is chosen\n");
//        return;
//    }
//
//    for(int i = 0; i < chosen; i++) {
//        if(chosen_dimension[i] == 0 || chosen_dimension[i] == 1) {
//            btd[chosen_dimension[i]] = hist_IP_prefix(curr_node, range_map[chosen_dimension[i]], chosen_dimension[i]);
//        } 
//        else {
//            btd[chosen_dimension[i]] = hist_IP_prefix(curr_node, range_map[chosen_dimension[i]], chosen_dimension[i]);
//        }
//    }
//
//    
//    
//}


void final_chosen_dim(node * curr_node, int *chosen_dim, int *chosen)
{
    int new_chosen_dim[MAXDIMENSIONS];
    int new_chosen = 0; 
    for(int i = 0; i < *chosen; i++) {
        if(chosen_dim[i] != -1) {
            new_chosen_dim[new_chosen] = chosen_dim[i];
            if(new_chosen < i && curr_node->ub_mask[chosen_dim[i]]) {
                memcpy(curr_node->fuse_array[new_chosen], curr_node->fuse_array[i], NUM_INDEX * sizeof(int));
                curr_node->fuse_array_num[new_chosen] = curr_node->fuse_array_num[i];
                memset(curr_node->fuse_array[i], 0, NUM_INDEX * sizeof(int));
                curr_node->fuse_array_num[i] = 0;
            }
            new_chosen ++;
        }
    }

    if(new_chosen == 0) {
        printf("can not proceed because no field can be cutted effectively\n");
        printf("this node has %lu rules\n", curr_node->classifier.size());
        printf("we can only leave it as it is\n");
        remove_redund(curr_node);
        printf("this node has %lu rules\n", curr_node->classifier.size());
        curr_node->hard = 1;
    }


    memcpy(chosen_dim, new_chosen_dim, MAXDIMENSIONS * sizeof(int));
    *chosen = new_chosen;

}


void init_cuts_node(cuts_node *cn, rule_boundary & b, int d)
{
    cn->boundary = b;
    cn->depth = d;
    cn->classifier.clear();
}

void push_rules(cuts_node *cn, node* curr_node, int index)
{
    for(list<pc_rule *>::iterator it = curr_node->classifier.begin();
            it != curr_node->classifier.end();
            it ++){
        if( (*it)->field[index].low > cn->boundary.field[index].high ||
                (*it)->field[index].high < cn->boundary.field[index].low) {
            ;
        }
        else {
            cn->classifier.push_back(*it);
        }
    }
}

int compute_cuts(node *curr_node, int *select_dim, 
        int *chosen_dimension,
        int *chosen,
        bool td 
        ) 
{
    for (int i = 0; i< MAXDIMENSIONS; ++i) {
        if ( select_dim[i] == 1) {
            chosen_dimension[(*chosen)++] = i;
        }
    }

    int smpf = int(floor(curr_node->classifier.size() * spfac));
    int sm = 0;

    cuts_node *cn = new cuts_node;
    cuts_node *cn_l, *cn_r;
    rule_boundary b;


    init_cuts_node(cn, curr_node->boundary, 1);
    b = curr_node->boundary;

    list<cuts_node*> cuts_list;
    cuts_list.push_back(cn);

    range r[2];
    int choose; 



    int prev_depth = -1;
    int nump[2] = {0,0};

    if(*chosen == 1)
        choose = 0;
    
    if(*chosen == 2)
        choose = 1;

    if(*chosen > 2){
        printf("current hypercuts do not support > 2 fields cutting\n");
        exit(-1);
    }

    while(!cuts_list.empty()) {

        cn = cuts_list.front();
        
        if(prev_depth != cn->depth) {

            if(sm < smpf) {

                if(*chosen == 2) {
                    choose = choose ^ 1;
                }

                nump[choose]++;
                sm = 1<<(nump[0] + nump[1]); 
                if(bitmap_op) {
                    //sm also means the cutting number 
                    //when enable BITMAP_OP
                    if(sm > MAX_CUTS_FOR_BITMAP_OP) {
                        int cnt = sm;
                        while(cnt > MAX_CUTS_FOR_BITMAP_OP){
                            nump[choose]--;
                            cnt = 1<<(nump[0] + nump[1]);
                            if(*chosen == 2)
                                choose = choose ^ 1;
                        }
                        break;
                    }
                }
                if (hypercuts_type == 2) {
                    if(sm > MAX_CUTS) {
                        int cnt = sm;
                        while(cnt > MAX_CUTS){
                            nump[choose]--;
                            cnt = 1<<(nump[0] + nump[1]);
                            if(*chosen == 2)
                                choose = choose ^ 1;
                        }
                        break;
                    }
                }
                prev_depth = cn->depth;
            }
            else {
                break;
            }
        } 
        cuts_list.pop_front();
        
        //choose the first dimension to cut
        r[0].low = cn->boundary.field[chosen_dimension[choose]].low;
        r[1].high = cn->boundary.field[chosen_dimension[choose]].high;

        r[0].high = cn->boundary.field[chosen_dimension[choose]].low + 
            (cn->boundary.field[chosen_dimension[choose]].high - 
             cn->boundary.field[chosen_dimension[choose]].low + 1)/2 - 1;

        r[1].low = r[0].high + 1;


        if(r[0].low <= r[0].high) {
            b.field[chosen_dimension[choose]] = r[0];
            cn_l = new cuts_node;
            init_cuts_node(cn_l, b, cn->depth + 1);
            push_rules(cn_l, curr_node, chosen_dimension[choose]);
            cuts_list.push_back(cn_l);
            sm += cn_l->classifier.size();
        }


        if(r[1].low <= r[1].high ) {
            b.field[chosen_dimension[choose]] = r[1];
            cn_r = new cuts_node;
            init_cuts_node(cn_r, b, cn->depth + 1);
            push_rules(cn_r, curr_node, chosen_dimension[choose]);
            cuts_list.push_back(cn_r);
            sm += cn_r->classifier.size();
        }  

        cn->classifier.clear();
        delete cn;

    }

    if(*chosen == 2 && !td) {
        curr_node->cuts[chosen_dimension[0]] = (1 <<nump[0]);
        curr_node->cuts[chosen_dimension[1]] = (1 <<nump[1]);
 
        //printf("%d cuts on %d\n", 1<<nump[0], chosen_dimension[0]);
        //printf("%d cuts on %d\n", 1<<nump[1], chosen_dimension[1]);
        //int cnt;
        //cnt = (1<<nump[0]) * (1<<nump[1]);
        //printf("%d cuts\n", cnt);
        //if(cnt > MAX_CUTS) {
        //    printf("%d cuts\n",cnt);
        //}

    }

    if(*chosen == 1 && !td) {
        curr_node->cuts[chosen_dimension[0]] = (1 << nump[0]);
        //printf("%d cuts on %d\n", 1<<nump[0], chosen_dimension[0]);
    }

    

    while(!cuts_list.empty())
    {
        cn = cuts_list.front();
        cuts_list.pop_front();
        delete cn;
    }

    if(td) {
        return sm;
    }
    else 
        return 0;

}


int cutting_check(node *curr_node, int *chosen_dimension, int chosen)
{
    int cnt = 1;
    int balance_cutting = 0;
    //need to provide a price function for the memory explode 
    for(int i = 0; i < chosen; i++) {
        if(curr_node->ub_mask[chosen_dimension[i]]) {
            cnt *= curr_node->fuse_array_num[i];
            balance_cutting =0;
        }
        else {
            cnt *= curr_node->cuts[chosen_dimension[i]];
            balance_cutting ++;
        }
    }

#ifndef LIMIT_CUTS
    if(balance_cutting == 2) {
        //all is balance field
        //memset(curr_node->cuts, 1, MAXDIMENSIONS * sizeof(int));
        for(int i = 0; i < MAXDIMENSIONS; i++ ) {
            curr_node->cuts[i] = 1;
        }
        return -1;
    }
    else 
        return 0;
#endif
    
#ifdef LIMIT_CUTS
    int flag = 0;
    while(cnt > MAX_CUTS) {
        flag = 1;
        printf("%d large then MAX_CUTS %d\n", cnt, MAX_CUTS);
        //reduce to small part
        //curr_node->cuts[0] = 1;
        //cnt = curr_node->cuts[0];
        for(int i = 0; i < chosen; i++) {
            if(curr_node->ub_mask[chosen_dimension[i]]) {
            }
            else {
                curr_node->cuts[chosen_dimension[i]] /= 2;
                cnt/=2;
            }
        }
    }

    if(flag)
        printf("back to %d cuts\n", cnt);
    return 0;
#endif

//#else
//    if (cnt > MAX_CUTS) {
//        printf("cnt %d\n", cnt);
//        memset(curr_node->cuts, 1, MAXDIMENSIONS * sizeof(int));
//        return -1;
//    }
//    else 
//        return 0;
//#endif
}

void reset_boundary(node *curr_node, int *dim_chosen, int chosen)
{
    unsigned long long l = 0;
    for(int i = 0; i < chosen; i++) {
        l = curr_node->boundary.field[dim_chosen[i]].high - 
            curr_node->boundary.field[dim_chosen[i]].low + 1;
        if(l != (1ULL<<((unsigned long long)log2(l)))) {
            l = 1ULL<<((unsigned long long)ceil(log2(l)));
            curr_node->boundary.field[dim_chosen[i]].low = 
                curr_node->boundary.field[dim_chosen[i]].high - l + 1;

        }
    }
}


int analyze_chosen_dimensions(node *curr_node, int *select_dim, 
        int *chosen_dimension,
        int *chosen,
        vector< map<range, int, less_range> >  &range_map)
{

    //all the dimensions
    //int chosen_dimension[MAX_DIMENSIONS];
    //int chosen = 0;

    int btd[MAXDIMENSIONS];

    for (int i = 0; i< MAXDIMENSIONS; ++i) {
        if ( select_dim[i] == 1) {
            chosen_dimension[(*chosen)++] = i;
        }
        btd[i] = 1;
    }


    int fuse_array[NUM_INDEX];
    int fuse_array_num = 0;


    //int check_error = 1;
    //int field_num = 0;
    for(int i = 0; i < *chosen; i++) {
        btd[chosen_dimension[i]] = cutting_guess(curr_node, range_map[chosen_dimension[i]],
                chosen_dimension[i], fuse_array, &fuse_array_num);
        //even unbalance cutting
        //we can't cut on this field
        
        if(btd[chosen_dimension[i]] == 0) {
            printf("can't perform even unbalance cutting\n");
            //delete this field in the chosen dimension
            //first mark it
            chosen_dimension[i] = -1;
            
            continue;
        }

        if (fuse_array_num != 0) {
            curr_node->cuts[chosen_dimension[i]] = (1<<btd[chosen_dimension[i]]);
            curr_node->ub_mask[chosen_dimension[i]] = 1;
            memcpy(curr_node->fuse_array[i], fuse_array, fuse_array_num * sizeof(int));
            curr_node->fuse_array_num[i] = fuse_array_num;
            //if(fuse_array_num == 1 && check_error == 1) {
            //    check_error = 1;
            //}
            //else 
            //{
            //    check_error = 0;
            //}
            //field_num ++;
        }
        else {
            //if the fuse_array_num == 0
            //and the curr_node can not even perform unequal-size cutting
            //the btd == 0
            //so this line of code is still right. 
            //we will check this in the final_chosen_dim
            //so don't worry.
            curr_node->cuts[chosen_dimension[i]] = 1 << btd[chosen_dimension[i]];
            //check_error = 0;
        }

    }

//    if(check_error) {
//        printf("all the fuse_array has 1 entry, algorithm can't continue\n");
//        //dump_rules(curr_node->classifier, "no_cuts");
//        //dump_boundary(curr_node);
//        //exit(-1);
//
//    }
//
    //delete the un-cutting dim
    //chosen dim also will impact the cutting order
    final_chosen_dim(curr_node, chosen_dimension, chosen);
    return cutting_check(curr_node, chosen_dimension, *chosen);


    
    //hist_prefix_length(curr_node, chosen_dimension, *chosen, range_map, btd);

}

//void calc_num_cuts(node *curr_node, int *select_dim)
//{
//    int done;
//    vector<int> nr;
//
//    for(int i = 0; i < MAXDIMENSIONS; i++) {
//        if(select_dim[i] == 1) {
//            done = 0;
//            while(!done) {
//                
//
//
//            }
//        }
//    }
//
//}


bool compare_second(const pair<int,int> &i, const pair<int,int> &j)
{
    if(i.second > j.second)
        return true;
    else 
        return false;
}

#ifdef CHODIM
typedef unsigned long long ull;
void split_choose_dim(list<pc_rule*> pc, rule_boundary &rb, int &dimension, int &hard)
{
    unsigned int dim;
    unsigned int num; 
    unsigned int pos;
    float  hightAvg, hightAll;
    unsigned int d2s = 0xFF;
    ull maxDiffSegPts = 1; 
    range bound;

    vector<vector<ull> > segPoints;
    vector<vector<ull> > segPointsInfo;
    vector<ull> tempSegPoints;

    segPoints.resize(MAXDIMENSIONS);
    segPointsInfo.resize(MAXDIMENSIONS);
    tempSegPoints.resize(2*pc.size());

    for (dim = 0; dim < MAXDIMENSIONS; dim ++) {
        /* N rules have 2*N segPoints */
        segPoints[dim].resize(2*pc.size());
        segPointsInfo[dim].resize(2*pc.size());
        num = 0;
        for(auto r = pc.begin(); r != pc.end(); r++) {
            bound = range_in_boundary_1D((*r)->field[dim], rb.field[dim]);
            segPoints[dim][2*num] = bound.low;
            segPoints[dim][2*num + 1] = bound.high;
            num ++;
        }
    }

    for (dim = 0; dim < MAXDIMENSIONS; dim ++) {
        sort(segPoints[dim].begin(), segPoints[dim].end());
    }

    hightAvg = 2 * pc.size() + 1;
    for (dim = 0; dim < MAXDIMENSIONS; dim ++) {
        unsigned int    i;
        //unsigned int    *hightList;
        unsigned int    diffSegPts = 1; /* at least there are one differnt segment point */

        tempSegPoints[0] = segPoints[dim][0];
        for (num = 1; num < 2*pc.size(); num ++) {
            if (segPoints[dim][num] != tempSegPoints[diffSegPts-1]) {
                tempSegPoints[diffSegPts] = segPoints[dim][num];
                diffSegPts ++;
            }
        }
        pos = 0;
        for (num = 0; num < diffSegPts; num ++) {
            int ifStart = 0;
            int ifEnd   = 0;
            segPoints[dim][pos] = tempSegPoints[num];
            for (auto r = pc.begin(); r != pc.end(); r++) {
                if ((*r)->field[dim].low == tempSegPoints[num]) {
                    /*printf ("\n>>rule[%d] range[0]=%x", i, ruleset->ruleList[i].range[dim][0]);*/
                    /*this segment point is a start point*/
                    ifStart = 1;
                    break;
                }
            }
            for (auto r = pc.begin(); r != pc.end(); r++) {
                if ((*r)->field[dim].high == tempSegPoints[num]) {
                    /*printf ("\n>>rule[%d] range[1]=%x", i, ruleset->ruleList[i].range[dim][1]);*/
                    /* this segment point is an end point */
                    ifEnd = 1;
                    break;
                }
            }
            if (ifStart && ifEnd) {
                segPointsInfo[dim][pos] = 0;
                pos ++;
                segPoints[dim][pos] = tempSegPoints[num];
                segPointsInfo[dim][pos] = 1;
                pos ++;
            }
            else if (ifStart) {
                segPointsInfo[dim][pos] = 0;
                pos ++;
            }
            else {
                segPointsInfo[dim][pos] = 1;
                pos ++;
            }
        }
        if (pos >= 3) {
            hightAll = 0;
            //hightList = (unsigned int *) malloc(pos * sizeof(unsigned int));
            for (i = 0; i < pos-1; i++) {
                //hightList[i] = 0;
                for (auto r = pc.begin(); r != pc.end(); r++) {
                    bound = range_in_boundary_1D((*r)->field[dim], rb.field[dim]);
                    if (bound.low <= segPoints[dim][i] \
                            && bound.high >= segPoints[dim][i+1]) {
                        //hightList[i]++;
                        hightAll++;
                    }
                }
            }
            if (hightAvg > hightAll/(pos-1)) {  /* possible choice for d2s, pos-1 is the number of segs */
                /* select current dimension */
                d2s = dim;
                hightAvg = hightAll/(pos-1);
            }
            /* print segment list of each dim */

            //free(hightList);
        } /* pos >=3 */

        if (maxDiffSegPts < pos) {
            maxDiffSegPts = pos;
        }

    }
    if (maxDiffSegPts <= 2) {
        hard = 1;
        dimension = -1;
    }

    dimension = d2s;
}
#endif

void get_range_map(node *curr_node, vector< map<range, int, less_range> > & range_map) 
{
    range check;
    map<range, int, less_range>::iterator rit;
    for (int i = 0; i < MAXDIMENSIONS; ++i)
    {
        range_map.push_back(map<range, int, less_range>(less_range_func));
        for (list<pc_rule*>::iterator rule = curr_node->classifier.begin();
                rule != curr_node->classifier.end();++rule)
        {
            check = range_in_boundary_1D((*rule)->field[i], curr_node->boundary.field[i]);
            rit = (range_map[i]).find(check);
            if (rit != range_map[i].end()) {
                ((range_map[i])[check]) ++;
            }
            else {
                ((range_map[i])[check]) = 1;
            }
        }
    }
}


void choose_dimensions_to_cut_option3(node *curr_node, int *select_dim)
{
    int choose_dim[MAXDIMENSIONS] = {-1, -1, -1, -1, -1};
    int chosen = 0;
    int d2c = 0;
    select_dim[0] = 1;

    int msm = compute_cuts(curr_node, select_dim, 
            choose_dim, &chosen, true);
    select_dim[0] = 0;
    chosen = 0;
            
    for(int dim= 1; dim< MAXDIMENSIONS; dim++) {
        select_dim[dim] = 1;
        int sm = compute_cuts(curr_node, select_dim, 
            choose_dim, &chosen, true);
        select_dim[dim] = 0;
        chosen = 0;
        if( sm < msm) {
            d2c = dim;
            msm = sm;
        }
    }

    select_dim[d2c] = 1;

}

void choose_dimensions_to_cut(node *curr_node, int *select_dim, vector< map<range,int, less_range> > & range_map)
{
    int unique_elements[MAXDIMENSIONS];
    double average = 0;
    //vector< map<range, int, less_range> > range_map_to_check;

    range check;
    //map<range, int, less_range> * range_map = new map<range, int, less_range>[MAXDIMENSIONS](less_range_func);
    //vector< map<range, int, less_range> > range_map;

    map<range, int, less_range>::iterator rit;

    for (int i = 0; i < MAXDIMENSIONS; ++i)
    {
        range_map.push_back(map<range, int, less_range>(less_range_func));
        //range_map_to_check.push_back(map<range, int, less_range>(less_range_func));

        for (list<pc_rule*>::iterator rule = curr_node->classifier.begin();
                rule != curr_node->classifier.end();++rule)
        {

            //if (select_small_ranges) {
            //    if (check_range_size((*rule)->field[i], i)) {
            //        continue;
            //    }
            //}

            check = range_in_boundary_1D((*rule)->field[i], curr_node->boundary.field[i]);
            rit = (range_map[i]).find(check);

            if (rit != range_map[i].end()) {
                ((range_map[i])[check]) ++;
                //(range_map[i])[(*rule)->field[i]] ++;
            }
            else {
                ((range_map[i])[check]) = 1;
                //(range_map[i])[(*rule)->field[i]] = 1;
            }
        }

        unique_elements[i] = (int)range_map[i].size();
        //printf("unique_elements %d \n", unique_elements[i]);
    }

    int dims_cnt = 0;
    for (int i = 0;i < MAXDIMENSIONS;++i)
    {
        if (curr_node->boundary.field[i].high > curr_node->boundary.field[i].low)
        {
            average += unique_elements[i];
            dims_cnt++;
        }
    }
    average = average / dims_cnt;
    
   
#ifdef HYPERCUTS 
    for (int i = 0;i < MAXDIMENSIONS;++i)
        select_dim[i] = 0;

    if(curr_node->no == 0 && predim != -1){ 
        select_dim[predim] = 1;
        return;
    }

    int dim_count = 0;
    for (int i = 0;i < MAXDIMENSIONS;++i)
    {
        if (curr_node->boundary.field[i].high > curr_node->boundary.field[i].low)
        {
            if (unique_elements[i] >= average)
            {
                    select_dim[i] = 1;
                    //printf("select %d\n", i);
                    dim_count++;
            }

            //if(dim_count == 2) {
            //    break;
            //}
        }
    }

    vector<pair<int, int> > s;
    for(int i = 0; i < MAXDIMENSIONS; ++i) {
        if(select_dim[i]) {
            s.push_back(pair<int, int>(i, unique_elements[i]));
        }
    }

    sort(s.begin(), s.end(), compare_second);
    memset(select_dim, 0, MAXDIMENSIONS * sizeof(int));
    dim_count = 0;

    for(vector<pair<int,int > >::iterator it = s.begin();
            it != s.end();
            it++ ) {
        select_dim[it->first] = 1;
        //printf("select %d\n", it->first);
        dim_count ++;

        if(dim_count ==2 ) {
            break;
        }
    }
#endif
    
#ifdef HICUTS
    for (int i = 0;i < MAXDIMENSIONS;++i)
        select_dim[i] = 0;

    int max = 0;
    int dim = 0;
    for (int i = 0;i < MAXDIMENSIONS;++i)
    {
        if (curr_node->boundary.field[i].high > curr_node->boundary.field[i].low)
        {
            if (unique_elements[i] >= max)
            {
                dim = i; 
                max = unique_elements[i];
            }

            //if(dim_count == 2) {
            //    break;
            //}
        }
    }
    select_dim[dim] = 1;

#endif


    //calc_num_cuts(curr_node, select_dim);


}


int samerules(node * r1, node * r2) {
    if (r1->classifier.empty() || r2->classifier.empty()) {
        return 0;
    }
    if (r1->classifier.size() != r2->classifier.size()) {
        return 0;
    }
    int found = 0;
    size_t num = 0;
    for (list<pc_rule*>::iterator itr1 = r1->classifier.begin();itr1 != r1->classifier.end();itr1++) {
        found = 0;
        for (list<pc_rule*>::iterator itr2 = r2->classifier.begin();itr2 != r2->classifier.end();itr2++) {
            if ((**itr1).priority == (**itr2).priority) {
                found = 1;
                num++;
                break;
            }
        }
        if (!found) {
            return 0;
        }
    }
    if (num != r1->classifier.size()) {
        printf("ERROR: Too many or too few rules matched\n");
    }
    return 1;
}

int find_itr_index(node *curr_node, node * itr)
{
    int pindex = 0;
    for(vector<node*>::iterator it = curr_node->ps.begin();
            it != curr_node->ps.end();
            it ++) {
        if(*it == itr) {
            break;
        }
        pindex ++;
    }

    if((unsigned)pindex == curr_node->ps.size()) {
        printf("can't find the node pointer, error\n");
        return -1;
    }

    return pindex;
}

list<node*> nodeMerging(node * curr_node) {
    list<node*> newlist = curr_node->children;
//#ifdef NODE_MERGE
    size_t num = 0;
    list<node*>::iterator itr2;
    /*	for(list<node*>::iterator junk = curr_node->children.begin(); junk != curr_node->children.end();junk++) {
        remove_redund(junk++);
        }*/

    for (list<node*>::iterator itr1 = newlist.begin(); itr1 != newlist.end(); itr1++) {
        itr2 = itr1;
        itr2++;
        while(itr2 != newlist.end()) {
            if (samerules(*itr1, *itr2)) {
                num++;
                //printf("samerules returned true\n");
                for (int i = 0; i < MAXDIMENSIONS; i++) {
                    if ((**itr1).boundary.field[i].low > (**itr2).boundary.field[i].low) {
                        (**itr1).boundary.field[i].low = (**itr2).boundary.field[i].low;
                    }
                    if ((**itr1).boundary.field[i].high < (**itr2).boundary.field[i].high) {
                        (**itr1).boundary.field[i].high = (**itr2).boundary.field[i].high;
                    }
                }
                int pindex = find_itr_index(curr_node, *itr2);
                curr_node->ps[pindex] = *itr1;
                ClearMem(*itr2);
                itr2 = newlist.erase(itr2);
            } else {
                itr2++;
            }
        }
    }
    if (num > curr_node->children.size()) {
        printf("Odd: Of %lu children, %lu were identical\n",newlist.size(),num);
    }
//#endif
    return newlist;
}

//void analyze_cm_list(node *curr_node)
//{
//    map<int, int> cm_map;
//    map<int, int>::iterator ci;
//    int bitmap = 0;
//
//    for(list<pc_rule*>::iterator pi = curr_node->classifier.begin();
//            pi != curr_node->classifier.end();
//            ++ pi) {
//        bitmap = 0;
//        for(int i= 0; i< MAXDIMENSIONS; i++)
//        if(!check_range_size((*pi)->field[i], i)) {
//            bitmap |= (1<<i);
//            ci = cm_map.find(bitmap);
//            if(ci != cm_map.end()) {
//                ci->second++;
//            }
//            else {
//                cm_map[bitmap] = 1;
//            }
//        }
//    }
//
//
//    for(ci = cm_map.begin();ci != cm_map.end(); ++ci) {
//        printf("%x %d\n", ci->first, ci->second);
//    }
//
//}

bool compare_rules(pc_rule *first, pc_rule *second)
{
    if(first->priority < second->priority) 
        return true;
    return false;
}

void partition_on_cuts(node *curr_node, int *select_dim, vector< map<range, int, less_range> > &range_map)
{
    int chosen_dim[MAXDIMENSIONS];
    int chosen = 0;

    for(int i = 0; i<MAXDIMENSIONS; i++) {
        if(select_dim[i]) {
            chosen_dim[chosen++] = i;
        }
    }

    int state = 0;

    for(list<pc_rule*>::iterator it = curr_node->classifier.begin();
            it != curr_node->classifier.end();
            ) {
        state = 0; 

        for(int i = 0; i< chosen; i++){
            if(check_range_size((*it)->field[chosen_dim[i]], curr_node->boundary, i)) {
                state ++;
            }
        }

        if(state == chosen) {
            //we need to delete it
            filter.push_back((*it));
            for(int j = 0; j< chosen; j++) {
                int cnt = range_map[chosen_dim[j]][(*it)->field[chosen_dim[j]]];    
                if(cnt -1 == 0) {
                    range_map[chosen_dim[j]].erase((*it)->field[chosen_dim[j]]);
                }
                else
                    range_map[chosen_dim[j]][(*it)->field[chosen_dim[j]]] -- ;
            }
            it = curr_node->classifier.erase(it); 
        }
        else {
            it ++;
        }
    }
    filter.sort();
    filter.unique();
    filter.sort(compare_rules);
    //printf("filter out %u\n", filter.size());

}


void regionCompaction(node * curr_node);
void calc_cuts(node *curr_node)
{
    int select_dim[MAXDIMENSIONS];

    int chosen_dimension[MAXDIMENSIONS] = {-1,-1,-1,-1,-1};
    int chosen = 0;
#ifndef LIMIT_CUTS
    int ret;
#endif

    //int cuts[MAXDIMENSIONS];

    vector< map<range, int, less_range> > range_map;
#ifndef CHODIM
    //choose_dimensions_to_cut_option3(curr_node, select_dim);
    choose_dimensions_to_cut(curr_node,select_dim, range_map);
#else
    int dim;
    get_range_map(curr_node, range_map);
    split_choose_dim(curr_node->classifier, curr_node->boundary, dim, curr_node->hard);
    memset(select_dim, 0, sizeof(int)*MAXDIMENSIONS);
    select_dim[dim] = 1;
#endif

    if(part_on_cut) {
        partition_on_cuts(curr_node, select_dim, range_map);
        regionCompaction(curr_node);
    }
    if(!hypercuts) { 
#ifndef LIMIT_CUTS 
        ret = analyze_chosen_dimensions(curr_node, select_dim, chosen_dimension, &chosen, range_map); 
#else
        analyze_chosen_dimensions(curr_node, select_dim, chosen_dimension, &chosen, range_map); 
#endif
    }
    else {
#ifndef CHODIM
        compute_cuts(curr_node, select_dim, chosen_dimension, &chosen, false);
#else
        if(curr_node->hard != 1) {
            compute_cuts(curr_node, select_dim, chosen_dimension, &chosen, false);
        }
#endif
    }


// if balance cutting is too large, we fall back into the hypercuts cutting
#ifndef LIMIT_CUTS
    if(ret == -1) {
        //back to early state
        memset(chosen_dimension, 0, sizeof(int) * MAXDIMENSIONS);
        chosen = 0;
        compute_cuts(curr_node, select_dim, chosen_dimension, &chosen, range_map);
    }
#endif

    
    perform_cuts(curr_node, chosen_dimension, chosen);

    //for(list<node*>::iterator ci = curr_node->children.begin();
    //        ci != curr_node->children.end();
    //        ++ci) {
    //    printf("this node is %u\n", (*ci)->classifier.size());
    //}


    //for (int i = 0;i < chosen_cnt;i++)
    //  printf("chosen_dim[%d] = %d [%llu - %llu]\n",i,chosen_dim[i],
    //      curr_node->boundary.field[chosen_dim[i]].low,
    //      curr_node->boundary.field[chosen_dim[i]].high);


}


void regionCompaction(node * curr_node) 
{
    range r1;
    range ret[MAXDIMENSIONS];

    for(int i =0; i< MAXDIMENSIONS;++i) {
        //inverse value
        ret[i].low = 0xFFFFFFFF;
        ret[i].high = 0;
    }


    for (list<pc_rule*>::iterator itr = (curr_node->classifier).begin();
            itr != (curr_node->classifier).end();
            ++itr) {

        for(int i=0; i< MAXDIMENSIONS; ++i) {
            r1 = range_in_boundary_1D((*itr)->field[i], curr_node->boundary.field[i]);

            if(r1.low < ret[i].low)
                ret[i].low = r1.low;

            if(r1.high > ret[i].high)
                ret[i].high = r1.high;
        }
    }

    for(int i =0; i< MAXDIMENSIONS;++i) {
        curr_node->boundary.field[i] = ret[i];
    }

}

bool sort_for_moveup(pc_rule * r1, pc_rule *r2)
{
    //put the loweset priority rules into the front
    if(r1->priority > r2->priority)
        return true;
    else
        return false;

}

void moveRulesUp(node* curr_node) {
    curr_node->node_has_rules = 0;
    list<pc_rule*> rulesMovedUp, setToCheck;
    int emptyIntersect = 1;
    rulesMovedUp = ((curr_node->children).front())->classifier; // start with this set
    // get list of rules that exist in all
    for (list <node*>::iterator item = (curr_node->children).begin();item != (curr_node->children).end();++item) {
        if (emptyIntersect) {
            setToCheck.clear();
            setToCheck = rulesMovedUp;
            rulesMovedUp.clear();
            for (list<pc_rule*>::iterator ptr = (*item)->classifier.begin(); ptr != (*item)->classifier.end(); ptr++) {
                for (list<pc_rule*>::iterator setptr = setToCheck.begin();setptr != setToCheck.end();setptr++) {
                    if (*setptr == *ptr) {
                        rulesMovedUp.push_back(*setptr);
                        break;
                    }
                }
            }
            if (rulesMovedUp.empty()) {
                emptyIntersect = 0;
            }
        }
    }

    rulesMovedUp.sort();
    rulesMovedUp.unique();
#ifdef STATCK_OP
    rulesMovedUp.sort(sort_for_moveup);
#endif
    //dump_rules(rulesMovedUp, "up");
    if (rulesMovedUp.size() > NUM_RULES_MOVE_UP) {
        // truncate to first bucketSize children
        //printf(" too many rules move up %u\n", rulesMovedUp.size());
        rulesMovedUp.resize(NUM_RULES_MOVE_UP);
    }

    // remove duplicate rules from all children
    list<node*> empty_set;
    if (emptyIntersect) {
        for(list<pc_rule*>::iterator setptr = rulesMovedUp.begin();setptr != rulesMovedUp.end();setptr++)
        {
            //setptr--;		
            for (list <node*>::iterator item = (curr_node->children).begin();item != (curr_node->children).end();++item) {
                for (list<pc_rule*>::iterator ptr = (*item)->classifier.begin();ptr != (*item)->classifier.end();ptr++) {
                    if (*setptr == *ptr) {
                        ptr = (*item)->classifier.erase(ptr);
                        (*item)->acc ++;
                        if((*item)->classifier.size() == 0) {
                            //printf("empty nodes caused by moveup\n");
                            empty_set.push_back(*item);
                        }
                        break;
                    }
                }
            }
            //Total_Rule_Size++;
            //Total_Rules_Moved_Up++;
            curr_node->node_has_rules ++;
            curr_node->acc ++;
            curr_node->rule_in_node = *setptr;
        }
    }
#if 1 
    if(empty_set.size() != 0 ){
        for(list<node*>::iterator it = empty_set.begin();
                it != empty_set.end();
                it ++ )
        {
            int pindex = find_itr_index(curr_node, *it);
            curr_node->ps[pindex] = NULL;
            for (list <node*>::iterator item = (curr_node->children).begin();item != (curr_node->children).end();++item) {
                if(*item == *it) {
                    //printf("kill one empty node\n");
                    curr_node->children.erase(item);
                    ClearMem(*item);
                    break;
                }
            }
        }
    }
#endif
    //printf("Rules moved up: %d\n", rulesMovedUp.size());
}

void level_restrict(node *curr_node, int bucketSize, int _LIMIT)
{
    if(curr_node->depth >= _LIMIT)
    {
        printf("this node has %lu rules, in depth %d\n", curr_node->classifier.size(),  _LIMIT);
        curr_node->hard = 1;
        curr_node->isLeaf = 1;
    }
}

void test_build()
{
    if(mem_total > 1 * 1024 * 1024){
        printf("fail to build a memory acceptable tree\n");
        show_stats();
        exit(-1);
    }
}


node* create_tree(list <pc_rule*> p_classifier)
{
    printf("Incoming No of Rules in this tree = %lu\n",p_classifier.size());
#ifdef CHODIM
    printf("split choose dim\n");
#endif
#ifdef STACK_OP
    printf("stack op\n");
#endif

    list <node*> worklist;
    node* curr_node;

    node* root = init_root_node(p_classifier);

    if (root->classifier.size() > (unsigned int)bucketSize)
        worklist.push_back(root);
    else
    {
        NodeStats(root);
    }


    list<node*> topush;

    while(!worklist.empty()){

        curr_node = worklist.back();
        worklist.pop_back();
        //printf(" this node has %lu rules, depth %d, no %d\n", curr_node->classifier.size(),
        //        curr_node->depth, curr_node->no);

        //if(curr_node->no == 137) {
        //    printf("here\n");
        //    dump_rules(curr_node->classifier, "no_cuts");
        //}

        // heuristic 3
        if(!bitmap_op) 
            regionCompaction(curr_node);
#if 0
        if(level_r) {
            level_restrict(curr_node, bucketSize, 7);
            if(curr_node->hard)
                goto skip;
        }
#endif

        calc_cuts(curr_node);


        if(moveruleup) 
            moveRulesUp(curr_node);

        //dump_children_rules(curr_node, "ml");
        //printf("before merging %u\n", curr_node->children.size());
        if(!bitmap_op)
            topush = nodeMerging(curr_node);
        else
            topush = curr_node->children;

        //topush = curr_node->classifier;

        //int a = topush.size();
        //printf("after merging %u\n", topush.size());
        //if (a<b) {
        //    printf("reduce merging\n");
        //}

        //rulesmoveup
        //ignore rulesmoveup 
        //remeber TO ADD IT LATER!!
        //
        //
        //node merging
        //if any node has the same ruleset with others
        //these two nodes should be merged
        //curr_node->children.clear();
        //curr_node->children = topush;
        for(list<node*>::iterator item = topush.begin();
                item != topush.end();
                item ++) {

           // if((*item)->depth > 3)
           //     break;
           // if((*item)->depth == 3) 

           //     dump_rules((*item)->classifier, "x2");
                //printf("this item %u\n", (*item)->classifier.size());

            if((*item)->classifier.size() > (size_t)bucketSize
                    && (*item)->hard != 1) 
            {
                worklist.push_back(*item);
            }
            else {
                if(!(*item)->classifier.empty()) {
                    (*item)->isLeaf = 1;
                    NodeStats(*item);

                }
                clear_mem_for_leaf(*item);
            }


            //break;
        }
        topush.clear();

        //printf("now worklist size %d\n", worklist.size());
        //if(worklist.size() == 36) {
        //    printf("here\n");
        //}
        //
#if 0
skip:
#endif
        if(!curr_node->children.size()) {
            //printf("a leaf node here, it is weird %d\n", curr_node->children.size());
        }

        NodeStats(curr_node);
        clear_mem_for_middle(curr_node);

        //test_build();

    }

    printf("in the end rules are %lu\n", p_classifier.size() - filter.size()); 

    return root;
}

//dump tree into a file, 
//because each time, when you needs a tree
//it takes too long to compile a tree from
//all these rules.


void node2dump(node *curr_node, 
        dump_node *dn, 
        int **ddt,
        int **pcode) {

    dn->depth = curr_node->depth;
    dn->boundary = curr_node->boundary;
    dn->rules_inside = curr_node->classifier.size();
    memcpy(dn->cuts, curr_node->cuts, MAXDIMENSIONS * sizeof(int));
    memcpy(dn->ub_mask, curr_node->ub_mask, MAXDIMENSIONS * sizeof(int));
    memcpy(dn->fuse_array, curr_node->fuse_array, MAX_CUT_FIELD * NUM_INDEX * sizeof(int));
    memcpy(dn->fuse_array_num, curr_node->fuse_array_num, MAX_CUT_FIELD * sizeof(int));
    dn->no = curr_node->no;
    dn->node_has_rules = curr_node->node_has_rules;
    dn->isLeaf = curr_node->isLeaf;
    dn->pointer_size = curr_node->ps.size();
    if(curr_node->node_has_rules)
        dn->rule_in_node = curr_node->rule_in_node->priority;
    else
        dn->rule_in_node = -1;

    
    if(dn->rules_inside != 0) {
        int i = 0;
        *ddt = (int *)malloc(dn->rules_inside * sizeof(**ddt));
        for(list<pc_rule*>::iterator it = curr_node->classifier.begin();
                it != curr_node->classifier.end();
                it ++ ) {
            (*ddt)[i] = (*it)->priority;
            i++;
        }
    }


    if(dn->pointer_size != 0) {
        *pcode = (int*)malloc(dn->pointer_size * sizeof(int));
        int i; 
        int j;
        int index = 0;

        for(i = 0;i<dn->pointer_size;i++) {
            (*pcode)[i] = -1;
            if(curr_node->ps[i] == NULL)
                continue;

            for (j=0;j<i;j++) {
                if(curr_node->ps[j] == curr_node->ps[i]) {
                    (*pcode)[i] = (*pcode)[j];
                    break;
                }
            }
            if((*pcode)[i] == -1) {
                (*pcode)[i] = index;
                index ++;
            }
        }
    }
}

//bool compare_pointer(node *first, node *second)
//{
//    if((unsigned int)first < (unsigned int)second) {
//        return true;
//    }
//    return false;
//}


void dump_dtree(node *root, FILE *fp)
{
    list<node*> dump_list;
    node *curr_node;
    dump_list.push_back(root);

    struct dump_node dn;
    int *ddt;
    int *pcode;
    vector<int> nn;
    //vector<node*> topush;

    fwrite(&node_count, sizeof(int), 1, fp);
    int lcnt = node_depth.size();
    fwrite(&lcnt, sizeof(int), 1, fp);
    for(map<int,int>::iterator it = node_depth.begin();
            it != node_depth.end();
            it ++) {
        int lncnt = it->second;
        fwrite(&lncnt, sizeof(int), 1, fp);
    }
    

    while(!dump_list.empty()) {
        curr_node = dump_list.front();
        dump_list.pop_front();

        node2dump(curr_node, &dn, &ddt, &pcode);
        fwrite(&dn, sizeof(dn), 1, fp);

        if(dn.rules_inside) {
            fwrite(ddt, sizeof(int), dn.rules_inside, fp);
            free(ddt);
        }

        if(dn.pointer_size) {
            fwrite(pcode, sizeof(int), dn.pointer_size, fp);
            for(int i = 0; i < dn.pointer_size; i++ ) {
                nn.push_back(pcode[i]);
            }
            sort(nn.begin(), nn.end());
            vector<int>::iterator it = unique(nn.begin(), nn.end());
            nn.erase(it, nn.end());

            int nnsize;
            if(nn[0] == -1) { 
                nn.erase(nn.begin());
            }

            nnsize = nn.size();
                
            fwrite(&nnsize, sizeof(int), 1, fp);
            nn.clear();

            int curr_index = 0;

            for(int i = 0; i< dn.pointer_size; i++ ){
                if(pcode[i] == -1) {
                    continue;
                }

                if(pcode[i] < curr_index) {
                    continue;
                }
                else {
                    dump_list.push_back(curr_node->ps[i]);
                    curr_index ++;
                }
            }
            free(pcode);
        }
    }
}

void load_dtree(node **root, FILE *fp)
{
    int node_cnt;
    fread(&node_cnt, sizeof(int), 1, fp);
    printf("need to load %d node\n", node_cnt);
    int level_cnt;
    fread(&level_cnt, sizeof(int), 1, fp);
    printf("in total %d levels\n", level_cnt);

    vector<node*> level_node;
    vector<int> level_node_cnt;


    for(int i = 0; i < level_cnt; i++) {
        int ncnt;
        fread(&ncnt, sizeof(int), 1, fp);
        printf("level %d has %d nodes\n", i+1, ncnt);
        level_node_cnt.push_back(ncnt);
        node * n = new node [ncnt];
        level_node.push_back(n);
    }



    dump_node dn;
    int *ddt = NULL;
    int *pcode = NULL;
    node *curr_node;

    node_cnt = 0;
    int child = 0;
    int curr_level = 0;
    int next_level_cnt = 0;




    while(!feof(fp) && curr_level < level_cnt) {
        fread(&dn, sizeof(dn), 1, fp);
        curr_node = level_node[curr_level] + node_cnt;
        node_cnt ++;

        

        curr_node->depth = dn.depth;
        curr_node->boundary = dn.boundary;
        if(dn.rules_inside) {
            ddt = (int*)malloc(dn.rules_inside * sizeof(*ddt));
        }

        memcpy(curr_node->cuts, dn.cuts, MAXDIMENSIONS * sizeof(int));
        memcpy(curr_node->ub_mask, dn.ub_mask, MAXDIMENSIONS * sizeof(int));
        memcpy(curr_node->fuse_array, dn.fuse_array, MAX_CUT_FIELD * NUM_INDEX * sizeof(int));
        memcpy(curr_node->fuse_array_num, dn.fuse_array_num, MAX_CUT_FIELD * sizeof(int)); 

        curr_node->no = dn.no;
        curr_node->node_has_rules = dn.node_has_rules;
        if(curr_node->node_has_rules){
            curr_node->rule_in_node = &classifier[dn.rule_in_node];
        }
        else
            curr_node->rule_in_node = NULL;
        curr_node->isLeaf = dn.isLeaf;
        
        if(dn.pointer_size) {
            pcode = (int*)malloc(dn.pointer_size * sizeof(int));
        }


        if(dn.rules_inside) {
            fread(ddt, sizeof(int), dn.rules_inside, fp);
            for(int i = 0; i< dn.rules_inside; i++) {
                curr_node->classifier.push_back(&(classifier[ddt[i]]));
            }
            free(ddt);
        }

        if(dn.pointer_size) {
            fread(pcode, sizeof(int), dn.pointer_size, fp);
            fread(&child, sizeof(int), 1, fp);
            //printf("child %d\n", child);

            curr_node->ps.resize(dn.pointer_size);
            for(int i = 0; i < dn.pointer_size;i++ ){ 
                if(pcode[i] != -1) {
                    curr_node->ps[i] = level_node[curr_level + 1] + pcode[i]
                        + next_level_cnt;
                }
                else
                    curr_node->ps[i] = NULL;
            }
            free(pcode);
            next_level_cnt += child;
        }



        if(node_cnt == level_node_cnt[curr_level]) {
            node_cnt = 0;
            curr_level ++;
            next_level_cnt = 0;
        }
        NodeStats(curr_node); 

    }
    *root = level_node[0];

}

#ifdef SEARCH_TEST 
void node_dim(node *n, int *chosen_dim, int *chosen)
{
    memset(chosen_dim, 0, MAXDIMENSIONS * sizeof(int));
    *chosen = 0;

    for(int i = 0; i <MAXDIMENSIONS; i++) {
        if(n->cuts[i] != 1) {
            chosen_dim[(*chosen)++] = i;
        }
    }
}

int fuse_array_find_index(unsigned long long v,
        int *fuse_array, int fuse_num) 
{
    for(int i = fuse_num - 1; i >=0 ; i--) {
        if ( v >= (unsigned int)fuse_array[i] ) 
            return i;
    }

    return 0;
}

int check_rule(pc_rule *r, field_type *ft)
{
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

int linear_search(list<pc_rule*> &p_classifier, field_type *ft)
{
    int ret = -1;
    for(list<pc_rule*>::iterator it = p_classifier.begin();
            it != p_classifier.end();
            it++) {
        ret = check_rule(*it, ft);
        if(ret == 1) {
            ret = (*it)->priority;
            break;
        }
    }
    return ret;
}
        


int search_rules(node *root, field_type *ft)
{
    node *curr_node = root;

    int chosen_dim[MAXDIMENSIONS];
    int chosen;
    unsigned long long s;
    unsigned long long v;
    int index = -1;
    int old_index;
    int this_table_num = 0;
    int ret = -1;

    while(curr_node && curr_node->classifier.size() == 0) {

        node_dim(curr_node, chosen_dim, &chosen);
        old_index = 0;
        this_table_num = 0;
        for(int i = chosen - 1; i >= 0; i--) {
            if(ft[chosen_dim[i]] > curr_node->boundary.field[chosen_dim[i]].high
                    || ft[chosen_dim[i]] < curr_node->boundary.field[chosen_dim[i]].low) 
            {
                return -1;
            }
            s = ceil((double)(curr_node->boundary.field[chosen_dim[i]].high - 
                        curr_node->boundary.field[chosen_dim[i]].low + 1) /
                    curr_node->cuts[chosen_dim[i]]);
            
            if(no_div) {
                if ( (1UL << (unsigned)log2(s)) != s) {
                    //printf("we need div %llu\n", s);
                    s = 1ULL<<((unsigned)ceil(log2(s)));
                }
            } 

            v = (ft[chosen_dim[i]] - 
                    curr_node->boundary.field[chosen_dim[i]].low )/ s ; 



            if(curr_node->ub_mask[chosen_dim[i]]) {
                index = fuse_array_find_index(v, curr_node->fuse_array[i], curr_node->fuse_array_num[i]);
                this_table_num = curr_node->fuse_array_num[i];
            }

            else {
                index = (int)v;
                //this_table_num = curr_node->cuts[chosen_dim[i]];
                this_table_num = check_s(curr_node, chosen_dim[i]);
            }

            index = old_index * this_table_num + index;
            old_index = index;

            //if(curr_node->ub_mask[chosen_dim[i]]) {
            //    last_table_num = curr_node->fuse_array_num[i]; 
            //}
            //else {
            //    last_table_num = curr_node->cuts[chosen_dim[i]];
            //}
        }
        curr_node = curr_node->ps[index]; 
    }

    if(!curr_node) {
        return -1;
    }

    ret = linear_search(curr_node->classifier, ft);

    return ret;
}
#endif

int load_ft(FILE *fpt, field_type *ft) 
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
int filter_out_flag = 0;
void filter_out(list<pc_rule*> &p_classifier)
{
    int w_m = 0;
    int m_w = 0;
    rule_boundary rb;

    rb.field[0].low = 0;
    rb.field[0].high = 0xFFFFFFFF;
    rb.field[1].low = 0;
    rb.field[1].high = 0xFFFFFFFF;
    rb.field[2].low = 0;
    rb.field[2].high = 0xFFFF;
    rb.field[3].low = 0;
    rb.field[3].high = 0xFFFF;
    rb.field[4].low = 0;
    rb.field[4].high = 0xFF;

    //map<range, int> ms;
    //map<range, int> md;


    filter_out_flag = 1;
    for(list<pc_rule*>::iterator it = p_classifier.begin();
            it != p_classifier.end();
            ) {
        if(check_range_size((*it)->field[0], rb, 0)
            && !check_range_size((*it)->field[1],rb, 1))
        {
            w_m ++;
            filter.push_back((*it));
            it = p_classifier.erase(it);
        }
        else if(check_range_size((*it)->field[1],rb, 1)
            && !check_range_size((*it)->field[0], rb, 0))
        {
            m_w ++;
            filter.push_back((*it));
            it = p_classifier.erase(it);
        }
        else if(check_range_size((*it)->field[1],rb, 1)
            && check_range_size((*it)->field[0], rb, 0))
        {
            filter.push_back((*it));
            it = p_classifier.erase(it);
        }
        else 
            it++;

    }
    printf("w_m %d\n", w_m);
    printf("m_w %d\n", m_w);

}



int main(int argc, char *argv[])
{
    parseargs(argc, argv);
    check_config();
    node *root;
    list<pc_rule*> p_classifier;

    loadrules(fpr);
    load_rule_ptr(classifier, p_classifier, 0, classifier.size() - 1);


    //filter_out(p_classifier);


//#if 0
    if(!ifp) {
        root = create_tree(p_classifier);
        show_stats();    
    }

    if(!filter.empty()) {
        dump_rules(filter, "filter1");
    }

    if(part_on_cut || filter_out_flag) {
        //node *froot;
        part_on_cut = 1;
        list<pc_rule*> incomming = filter;
        filter.clear();
        clear_stats();
        //froot = create_tree(incomming);
        create_tree(incomming);
        show_stats();    
    }
    if(ofp)
        dump_dtree(root, ofp);  
    if(ifp){
        load_dtree(&root, ifp);
        show_stats();
    }
    
    if(!filter.empty()) {
        dump_rules(filter, "filter2");
    }


//#endif

#ifdef SEARCH_TEST
    int ret;
    int ret2;
    int trace_no = 0;
    int flag = 0;
    field_type ft[MAXDIMENSIONS];
    while((trace_no = load_ft(fpt, ft)) != 0) {
        //if(trace_no == 4348)
        //    printf("here\n");
        ret = search_rules(root, ft);
        ret2 = linear_search(p_classifier, ft);
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




}



