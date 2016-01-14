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

using namespace std;

#include "hypc.h"
#include "sp_tree.h"
#include "rtrie.h"
#include <unistd.h>
#define THRESHOLD (12 * 1024 * 1024)


#define DOUT(s,a,b) printf("%s:mem = %d %dKB %.2fMB, acc = %d\n", (s), (a), (a)/1024, (a)/(1024.*1024),(b))
#define RSOUT(s,a) printf("%s: %u\n", (s), (a).size())
#define RULE_ENTRY_SIZE 18

vector<pc_rule> classifier;
double bin = 0.5;
double IPbin = 0.05;
int bucketSize = 16;
int no_div = 1;
int just_analyze = 0;
int always_skew_check = 1;
FILE *fpr;


int check_range_size(const range & check, const rule_boundary & rb, int index);
int sip_div = 2;
int dip_div = 2;
#define PTR_SIZE 4
string rs_name;
void parseargs(int argc, char *argv[])
{
    int	c;
    while ((c = getopt(argc, argv, "r:as:d:")) != -1) {
        switch (c) {
            case 'r':
                fpr = fopen(optarg, "r");
                rs_name = string(optarg);
                break;
            case 'a':
                just_analyze = 1;
                break;
            case 's':
                sip_div = atoi(optarg);
                break;
            case 'd':
                dip_div = atoi(optarg);
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

inline range range_in_boundary_1D(range r, range boundary);

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

void init_rnode(rnode *n, rule_boundary &b) 
{
    rg sip(b.field[0].low, b.field[0].high);
    rg dip(b.field[1].low, b.field[1].high);

    n[0].setb(sip);
    n[1].setb(dip);

}


void remove_redund_rt(list<pc_rule*> &pr, rule_boundary &b, bool large) 
{
    list<pc_rule*> rulelist;
    //vector<pc_rule*> vpc;
    //l2v(pr, vpc);
    
    range br;
    
    rnode rt[2];
    vector<pc_rule*> set[2];
    //map<int, int> sect;
    vector<pc_rule*> sect;

    init_rnode(rt, b);
    //int count = 0;

    for(auto rule = pr.begin();
            rule != pr.end(); ++ rule) {

        if(large) {
            if(!check_range_size((*rule)->field[0], b, 0) 
                    || !check_range_size((*rule)->field[1], b, 1)) {
                rulelist.push_back(*rule);
                continue;
            }
        }

        //if((*rule)->priority == 567) {
        //    cout<<"here"<<endl;
        //}

        sect.clear();
        for(int i = 0; i < 2; i++) {
            br = range_in_boundary_1D((*rule)->field[i], b.field[i]); 
            set[i].clear();
            rt_qry_insert(rt+i, rg(br.low, 
                    br.high), set[i], (*rule));
        }

        //if(set[0].size() > 5 * set[1].size()) {
        //    sect = set[1];
        //}
        //else if (set[1].size() > 5 * set[0].size()) {
        //    sect = set[0];

        //}
        //else{ 
        sort(set[0].begin(), set[0].end());
        sort(set[1].begin(), set[1].end());
        set_intersection(set[0].begin(), set[0].end(),
                set[1].begin(), set[1].end(),
                back_inserter(sect));
        //}

        if(sect.empty()) {
            rulelist.push_back(*rule);
        }
        else {
            bool found = false;
            for(auto check_rule = sect.begin();
                    check_rule != sect.end();
                    check_rule++) {
                if(is_equal(**check_rule, **rule, b)) {
                    found = true;
                    //count++;
                    //cout<<(*rule)->priority<<" was hide by " <<*check_rule<<endl;
                    break;
                }
            }

            if(!found) {
                rulelist.push_back(*rule);
            }
        }

    }
    pr = rulelist;
    //cout<<"remove "<<count<<endl;
}

void remove_redund(list<pc_rule*> &pr, rule_boundary &rb)
{
    list <pc_rule*> rulelist;
    //list <pc_rule*> dellist;
    //list <pc_rule*> cftlist;

    rulelist.clear();

    for (list<pc_rule*>::iterator rule = pr.begin();
            rule != pr.end();++rule)
    {
        int found = 0;
        for (list<pc_rule*>::iterator mule = rulelist.begin();
                mule != rulelist.end();++mule)
        {
            if (is_equal(**mule,**rule, rb) == true)
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
    pr.clear();
    pr = rulelist;
    //pr.unique(myequal);
}

int check_range_size(const range & check, const rule_boundary & rb, int index)
{
    double field;
    field = (((double)(check.high - check.low))/(rb.field[index].high - rb.field[index].low));
    if(index == 0 || index == 1) {
        if(field >= IPbin) {
            return 1;
        } 
        else
            return 0;
    }
    else {
        if(field >= bin) {
            return 1;
        }
        else
            return 0;
    }
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

int loadrules(FILE *fp) {
    int i = 0;
    int wild = 0;
    unsigned sip1, sip2, sip3, sip4, siplen;
    unsigned dip1, dip2, dip3, dip4, diplen;
    unsigned proto, protomask;
    unsigned junk, junkmask;

    pc_rule rule;

    while(1) {
        wild = 0;
        if(fscanf(fp,"@%u.%u.%u.%u/%u\t%u.%u.%u.%u/%u\t%llu : %llu\t%llu : %llu\t%x/%x\t%x/%x\n",
                    &sip1, &sip2, &sip3, &sip4, &siplen, &dip1, &dip2, &dip3, &dip4, &diplen, 
                    &rule.field[2].low, &rule.field[2].high, &rule.field[3].low, &rule.field[3].high,
                    &proto, &protomask, &junk, &junkmask) != 18) break;

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
        rule.priority = i;
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

bool less_range_func(const range & a, const range &b)
{
    if(a.low != b.low)
        return a.low < b.low;
    else 
        return a.high < b.high;
    //return a.low < b.low;
}
typedef bool (*less_range)(const range &a, const range &b);


void get_range_map(list<pc_rule*> &pc, vector< map<range,int, less_range> > & range_map, rule_boundary &rb)
{
    //vector< map<range, int, less_range> > range_map_to_check;

    range check;
    //map<range, int, less_range> * range_map = new map<range, int, less_range>[MAXDIMENSIONS](less_range_func);
    //vector< map<range, int, less_range> > range_map;

    map<range, int, less_range>::iterator rit;

    for (int i = 0; i < MAXDIMENSIONS; ++i)
    {
        range_map.push_back(map<range, int, less_range>(less_range_func));
        //range_map_to_check.push_back(map<range, int, less_range>(less_range_func));

        for (list<pc_rule*>::iterator rule = pc.begin();
                rule != pc.end();++rule)
        {

            //if (select_small_ranges) {
            //    if (check_range_size((*rule)->field[i], i)) {
            //        continue;
            //    }
            //}

            check = range_in_boundary_1D((*rule)->field[i], rb.field[i]);
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

        //printf("unique_elements %d \n", unique_elements[i]);
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


void init_boundary(rule_boundary & rb)
{
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
       fprintf(out, "0x%llx/0x%llx\t", (i)->field[4].low, 0xFFULL);

    fprintf(out, "0x%x/0x%x\t", 0,0);

    fprintf(out, "%d\n", (i)->priority);
}

void dump_rules(list<pc_rule*> ruleset, string str)
{
    FILE *fp = fopen(str.c_str(), "w");

    for (list<pc_rule *>::iterator i = ruleset.begin();
            i != ruleset.end();
            i++){
        dump_rule(*i, fp);
    }
    fclose(fp);
} 


void check_fields_uniformity(list<pc_rule*> &prl, 
        int i, vector<int>& cnt, rule_boundary &rb, int bits)
{
    int prefix;
    int length;

    //printf("%u\n", prl.size());
    for(list<pc_rule*>::iterator it = prl.begin();
            it != prl.end();
            ++it) {
        //printf("%llu %llu\n",(*it)->field[i].low, (*it)->field[i].high);
        //printf("%u\n",(*it)->priority);
       
        if(!check_range_size((*it)->field[i], rb, i)) {
            prefix = (*it)->field[i].low ^ (*it)->field[i].high;
            length = 32 - __builtin_popcount(prefix);
            prefix = (int)((*it)->field[i].low);
            int pos = ((unsigned int)prefix >> (32 - bits));
            //printf("length %d prefix %x\n", length, prefix);
            //printf("pos %u\n", pos);

#if 1 
            if(length >= bits) {
                cnt[pos] ++;
            }
            else {
                for(int d = 0; d < (1<<(bits - length)); d++) {
                    cnt[pos + d] ++;
                }
            }
#endif
        }
    }
}

void dump_vector(vector<int> &cnt, string str)
{
    FILE *fd = fopen(str.c_str(), "w");
    for(size_t i = 0; i< cnt.size(); i++) {
        fprintf(fd, "%d\t%d\n", i, cnt[i]);
    }
    fclose(fd);
}

void check_uniformity(list<pc_rule*> & prl, int bits)
{
    rule_boundary rb;
    init_boundary(rb);

    vector<int> cnt;

    printf("bits %d %d\n", bits, 1<<bits);
    cnt.resize((1<<bits));

    //check source IP
    //printf("here\n");
    //
    printf("sip\n");
    check_fields_uniformity(prl, 0, cnt, rb, bits);
    dump_vector(cnt, "sip");

    cnt.clear();
    cnt.resize((1<<bits));

    printf("dip\n");
    check_fields_uniformity(prl, 1, cnt, rb, bits);
    dump_vector(cnt, "dip");
    cnt.clear();
}

void analyze_port(
        rule_boundary &rb,
        vector< map<range, int, less_range> > & range_map,
        list<pc_rule*> &p_classifier,
        struct mem_info &mi
        )
{
    int pws, psw, pww, pss;
    pws = 0;
    pww = 0;
    pss = 0;
    psw = 0;
    for(list<pc_rule*>::iterator it = p_classifier.begin();
            it != p_classifier.end();
            it ++ ) {
        if(check_range_size((*it)->field[2], rb, 2) 
                && !check_range_size((*it)->field[3], rb, 3))
        {
            pws ++;
        }

        if(!check_range_size((*it)->field[2], rb, 2) 
                && check_range_size((*it)->field[3], rb, 3))
        {
            psw ++;
        }

        if(check_range_size((*it)->field[2], rb, 2) 
                && check_range_size((*it)->field[3], rb, 3))
        {
            pww ++;
        }
        
        if(!check_range_size((*it)->field[2], rb, 2) 
                && !check_range_size((*it)->field[3], rb, 3))
        {
            pss ++;
        }
    }

    printf("pss = %d psw = %d pws = %d pww = %d\n", pss, psw, pws, pww);
    int sum = 0;
    for(int i = 0; i<= 4; i++) {
        printf("dim %d unique ranges %d\n", i, range_map[i].size());
        sum += range_map[i].size();
    }

    printf("rules/ranges %.2f\n", p_classifier.size() /(double)sum);




#if 0
    unsigned cws;
    unsigned csw;
    unsigned cww;
    double fws = (double) range_map[2].size()/ (range_map[2].size() + range_map[3].size());
    double fsw = (double) range_map[3].size()/ (range_map[2].size() + range_map[3].size());
    cws = (unsigned)((log2((psw + pss * fws)/(bucketSize/2))));
    csw = (unsigned)((log2((pws + pss * fsw)/(bucketSize/2))));
    cww = (unsigned)(log2((psw + pss* fws )/(bucketSize/2)) + log2((pws + pss * fsw)/(bucketSize/2)));
    printf("fws %.2f fsw %.2f\n", fws, fsw); 

    printf("log2 :(sw + ss * fws) / ( bucketSize/2) %d\n", cws);
    printf("log2 :(ws + ss * fsw) / ( bucketSize/2) %d\n", csw);

    uint64_t mpwssw;
    uint64_t mpss;
    uint64_t mpww;

    mpwssw = pws *(1ULL<<cws) * PTR_SIZE + psw * (1ULL<<csw) * PTR_SIZE;
    mpww = pww * (1ULL<<cww) * PTR_SIZE;
    mpss = (pss) * PTR_SIZE;
    printf("memory_wssw %llu %lluKB %.2fMB\n", mpwssw, mpwssw/1024, (double)mpwssw/(1024*1024));
    printf("memory_ww %llu %lluKB %.2fMB\n", mpww, mpww/1024, (double)mpww/(1024*1024));
    printf("memory_ss %llu %lluKB %.2fMB\n", mpss, mpss/1024, (double)mpss/(1024*1024));
    printf("memory_total %llu %lluKB %.2fMB\n", (mpww + mpwssw + mpss),
            (mpww + mpwssw + mpss)/1024, (mpww + mpwssw + mpss)/(1024 * 1024.));


    mi.mpww =   mpww;
    mi.mpwssw = mpwssw;
    mi.mpss =   mpss;
#endif



}

void calc_fac(list<pc_rule*> &wwl, struct mem_info &mi) 
{
    double sf,df;
    double total = 0;
    rule_boundary rb;
    init_boundary(rb);
    remove_redund(wwl, rb);

    int w4 = 0;
    for(list<pc_rule*>::iterator it = wwl.begin();
            it != wwl.end();
            it ++ ) {
        sf = ((*it)->field[0].high - (*it)->field[0].low + 1) / (double)0x100000000ULL;
        df = ((*it)->field[1].high - (*it)->field[1].low + 1) / (double)0x100000000ULL;
        total += sf * df;

        if(check_range_size((*it)->field[0], rb, 0) && 
                check_range_size((*it)->field[1], rb, 1) && 
                check_range_size((*it)->field[2], rb, 2) && 
                check_range_size((*it)->field[3], rb, 3)) {
            w4++;
        }
    }
    printf("ww is actually %.2f\n", total);
    mi.aww = total;
    printf("w4 = %d\n", w4);
}

void calc_mem(list<pc_rule*> &pr, uint64_t & mww, uint64_t &mws, uint64_t &msw, uint64_t &mss, uint64_t &mstrict)
{
    vector< map<range, int, less_range> > range_map;

    int ws, sw, ww, ss;
    ws = 0;
    sw = 0;
    ww = 0;
    ss = 0;

    rule_boundary rb;
    init_boundary(rb);

    get_range_map(pr, range_map, rb);
    for(list<pc_rule*>::iterator it = pr.begin();
            it != pr.end();
            it ++ ) {
        if(check_range_size((*it)->field[0], rb, 0) 
                && !check_range_size((*it)->field[1], rb, 1))
        {
            ws ++;
        }

        if(!check_range_size((*it)->field[0], rb, 0) 
                && check_range_size((*it)->field[1], rb, 1))
        {
            sw ++;
        }

        if(check_range_size((*it)->field[0], rb, 0) 
                && check_range_size((*it)->field[1], rb, 1))
        {
            ww ++;
        }

        if(!check_range_size((*it)->field[0], rb, 0) 
                && !check_range_size((*it)->field[1], rb, 1))
        {
            ss ++;
        }

    }
    //printf("ww %d ws %d sw %d ss %d\n", ww, ws, sw, ss);

    
    double fws = (double)range_map[0].size() / (range_map[0].size() + range_map[1].size());
    double fsw = (double)range_map[1].size() / (range_map[0].size() + range_map[1].size());
    //printf("fws %.2f fsw %.2f\n", fws, fsw);

#if 0
    unsigned int cws;
    unsigned int csw;
    unsigned int cww;

    if(sw + ss * fws > bucketSize/2){
        cws = (unsigned)ceil((log2((sw + ss * fws)/(bucketSize/2))));
    }
    else {
        cws = 1;
    }
   
    if(ws + ss * fsw > bucketSize/2) {
        csw = (unsigned)ceil((log2((ws + ss * fsw)/(bucketSize/2))));
    }
    else {
        csw = 1;
    }
    if(sw + ss*fws >= bucketSize/2){
        cws = (unsigned)((log2((sw + ss*fws)/(bucketSize/2))));
    }
    else {
        cws = 0;
    }
    if(ws + ss * fsw >= bucketSize/2) { 
        csw = (unsigned)((log2((ws + ss*fsw)/(bucketSize/2))));
    }
    else {
        csw = 0;
    }
    if((log2((ws + ss*fsw)/(bucketSize/2)) + log2((sw + ss*fws)/(bucketSize/2)))> 0)
        cww = (unsigned)((log2((ws + ss*fsw)/(bucketSize/2)) + log2((sw + ss*fws)/(bucketSize/2))));
    else 
        cww = 0;

    //printf("\nin pw calc\n");
    //printf("fws %.2f fsw %.2f\n", fws, fsw); 
    //printf("log2 :(sw + ss * fws) / ( bucketSize/2) %d\n", cws);
    //printf("log2 :(ws + ss * fsw) / ( bucketSize/2) %d\n", csw);

    mws = ws *(1ULL<<cws) * PTR_SIZE ;
    msw = sw * (1ULL<<csw) * PTR_SIZE;
    mww = ww * (1ULL<<(cww)) * PTR_SIZE;
    mss = ss * PTR_SIZE;
#endif
    mws = ws * (sw + ss * fws)/(bucketSize/2) * PTR_SIZE;
    msw = sw * (ws + ss * fsw)/(bucketSize/2) * PTR_SIZE;
    mww = ww * (((sw + ss * fws)/(bucketSize/2)) * ((ws + ss * fsw)/(bucketSize/2))) * PTR_SIZE; 
    mss = (ss) * PTR_SIZE;
    mstrict = 2*ws*sw/(bucketSize/2) * PTR_SIZE;

}

void mem_skew_check(list<pc_rule*> &pr, int sip_btd, int dip_btd, struct mem_info &mi)
{
    printf("sip_btd %d\n", sip_btd);
    printf("dip_btd %d\n", dip_btd);

    uint32_t st_sip = (0xFFFFFFFFULL + 1) / (1<<sip_btd); 
    uint32_t st_dip = (0xFFFFFFFFULL + 1) / (1<<dip_btd); 
    range sip;
    range dip;

    vector<list<pc_rule*> > rules;
    vector<rule_boundary> rbv;

    rules.resize((1<<dip_btd) * (1<<sip_btd));
    rbv.resize((1<<dip_btd) * (1<<sip_btd));
    

    for(uint32_t i = 0; i < (1<<sip_btd); i++) {

        sip.low = i * st_sip;
        sip.high = (uint64_t)(i+1) * st_sip - 1;

        for(uint32_t j = 0; j< (1<<dip_btd); j++) {
            dip.low = j * st_dip;
            dip.high = (uint64_t)(j+1) * st_dip - 1; 

            init_boundary(rbv[i*(1<<dip_btd) +j]);
            for(list<pc_rule*>::iterator it = pr.begin();
                    it != pr.end();
                    it ++) {
                if((*it)->field[0].high < sip.low || (*it)->field[0].low > sip.high)
                    continue;
                if((*it)->field[1].high < dip.low || (*it)->field[1].low > dip.high)
                    continue;

                rules[i*(1<<dip_btd) + j].push_back(*it);
                rbv[i*(1<<dip_btd) +j].field[0].low = sip.low;
                rbv[i*(1<<dip_btd) +j].field[0].high = sip.high;
                rbv[i*(1<<dip_btd) +j].field[1].low = dip.low;
                rbv[i*(1<<dip_btd) +j].field[1].high = dip.high;
            }
        }
    }

    uint64_t mss, mws, msw, mww, mstrict;
    uint64_t mem = 0;

    for(uint32_t i = 0; i < rules.size(); i++) {
        //printf("rules[%d].size() %d\n", i, rules[i].size());
        remove_redund_rt(rules[i], rbv[i], true); 
        //remove_redund_rt(rules[i], rbv[i]); 
        calc_mem(rules[i], mww, mws, msw,mss, mstrict); 
        mem += (mww + mws + msw + mss);
        //if(mem > 1000000000ULL)
        //{
        //    printf("i %d\n", i);
        //}
        //printf("mww %llu mws %llu msw %llu mss %llu\n", mww, mws, msw, mss);

        mi.mww += mww;
        mi.mwssw += (mws + msw);
        mi.mss += mss;
        mi.mt += (mww + mws + msw + mss);
        mi.mstrict += mstrict;
    }
    printf("mstrict %llu %lluKB %.2fMB\n", mi.mstrict, mi.mstrict/1024, mi.mstrict/(1024*1024.));
    printf("mwssw %llu %lluKB %.2fMB\n", mi.mwssw, mi.mwssw/1024, mi.mwssw/(1024*1024.));
    printf("mem %llu %lluKB %.2fMB\n", mem, mem/1024, mem/(1024*1024.));

    //mi.mww = mww;
    //mi.mwssw = mws + msw;
    //mi.mss = mss;
    //mi.mt = (mww + mws + msw + mss);

}



//#define PORT_MEM
void calc_mem_with_data(
        rule_boundary &rb,
        vector< map<range, int, less_range> > &range_map,
        list<pc_rule*> &p_classifier, 
        struct mem_info &mi,
        int skew_sip,
        int skew_dip
        )
{
    int ws,sw,ww,ss;
    ws = 0;
    sw = 0;
    ww = 0;
    ss = 0;


#ifdef PORT_MEM
    list<pc_rule*> wwl;
    list<pc_rule*> ssl;
    list<pc_rule*> wsl;
    list<pc_rule*> swl;
#endif
   // if(skew_sip != 1 && skew_dip != 1)
   //     remove_redund(p_classifier, rb);

    for(list<pc_rule*>::iterator it = p_classifier.begin();
            it != p_classifier.end();
            it ++ ) {
        if(check_range_size((*it)->field[0], rb, 0) 
                && !check_range_size((*it)->field[1], rb, 1))
        {
            ws ++;
#ifdef PORT_MEM
            wsl.push_back(*it);
#endif
        }

        if(!check_range_size((*it)->field[0], rb, 0) 
                && check_range_size((*it)->field[1], rb, 1))
        {
            sw ++;
#ifdef PORT_MEM
            swl.push_back(*it);
#endif
        }

        if(check_range_size((*it)->field[0], rb, 0) 
                && check_range_size((*it)->field[1], rb, 1))
        {
            ww ++;
#ifdef PORT_MEM
            wwl.push_back(*it);
#endif
        }
        
        if(!check_range_size((*it)->field[0], rb, 0) 
                && !check_range_size((*it)->field[1], rb, 1))
        {
            ss ++;
#ifdef PORT_MEM
            ssl.push_back(*it);
#endif
        }
    }
    printf("ss = %d sw = %d ws = %d ww = %d\n", ss, sw, ws, ww);

    double fws = (double) range_map[0].size()/ (range_map[0].size() + range_map[1].size());
    double fsw = (double) range_map[1].size()/ (range_map[0].size() + range_map[1].size());

    uint64_t mwssw;
    uint64_t mstrict;
    uint64_t mss;
    uint64_t mww;
#if 0
    unsigned cws;
    unsigned csw;
    unsigned cww;
    cws = (unsigned)((log2((sw + ss * fws)/(bucketSize/2))));
    csw = (unsigned)((log2((ws + ss * fsw)/(bucketSize/2))));
    cww = (unsigned)(log2((sw + ss* fws )/(bucketSize/2)) + log2((ws + ss * fsw)/(bucketSize/2)));
    printf("fws %.2f fsw %.2f\n", fws, fsw); 

    printf("log2 :(sw + ss * fws) / ( bucketSize/2) %d\n", cws);
    printf("log2 :(ws + ss * fsw) / ( bucketSize/2) %d\n", csw);


    mwssw = ws *(1ULL<<cws) * PTR_SIZE + sw * (1ULL<<csw) * PTR_SIZE;
    mww = ww * (1ULL<<cww) * PTR_SIZE;
    mss = (ss) * PTR_SIZE;
#endif
    mwssw = (ws * (sw + ss * fws)/(bucketSize/2)) * PTR_SIZE + sw * ((ws + ss * fsw)/(bucketSize/2)) * PTR_SIZE;
    mstrict = (2*(ws * sw)/(bucketSize/2)) *PTR_SIZE;
    mww = ww * ((sw + ss * fws)/(bucketSize/2)) * ((ws + ss * fsw)/(bucketSize/2)) * PTR_SIZE; 
    mss = (ss) * PTR_SIZE;

    printf("memory_wssw %llu %lluKB %.2fMB\n", mwssw, mwssw/1024, (double)mwssw/(1024*1024));
    printf("memory_ww %llu %lluKB %.2fMB\n", mww, mww/1024, (double)mww/(1024*1024));
    printf("memory_ss %llu %lluKB %.2fMB\n", mss, mss/1024, (double)mss/(1024*1024));
    printf("memory_total %llu %lluKB %.2fMB\n", (mww + mwssw + mss),
            (mww + mwssw + mss)/1024, (mww + mwssw + mss)/(1024 * 1024.));


    mi.mww = mww;
    mi.mwssw = mwssw;
    mi.mss = mss;
    mi.mstrict = mstrict;
    mi.mt = (mww + mwssw + mss);

    mi.ss = ss;
    mi.ws = ws;
    mi.sw = sw;
    mi.ww = ww;

    //remove_redund(wwl, rb);
    //printf("\nww: %u\n", wwl.size());
    //printf("after redund %d\n", wwl.size());

    if(always_skew_check) {
        mi.mww = 0;
        mi.mwssw = 0;
        mi.mss = 0;
        mi.mt = 0;
        mi.mstrict = 0;
        mem_skew_check(p_classifier, sip_div, dip_div, mi);
    }

    //calc_fac(wwl, mi);
    //mi.amt = mi.mt - mi.mww + mi.aww * (1ULL<<cww) * PTR_SIZE;
    //printf("acutal memory_total %llu %lluKB %.2fMB\n", mi.amt, mi.amt/1024, mi.amt/(1024.*1024));

    //this code is just used for test value
    //it can not be enabled for the framework.
    //mi.mww = 0;
    //mi.mwssw = 0;
    //mi.mss = 0;
    //mi.mt = 0;
    //mem_skew_check(p_classifier, 2, 2, mi);
    //end 

    if((skew_sip || skew_dip) && (ww!=0 || sw!=0 || ws!=0) && !always_skew_check ) {
        mi.mww = 0;
        mi.mwssw = 0;
        mi.mss = 0;
        mi.mt = 0;

        printf("found it skew, I have to recheck\n");
        if(skew_sip == 1 && skew_dip == 1)
            mem_skew_check(p_classifier, 2, 2, mi);
        else if(skew_sip == 1 && skew_dip == 0)
            mem_skew_check(p_classifier, 2, 0, mi);
        else if(skew_sip == 0 && skew_dip == 1)
            mem_skew_check(p_classifier, 0, 2, mi);
        else
            printf("no skew at all\n");
            
        printf("\n");
    }


#ifdef PORT_MEM
    //remove_redund(wwl, rb);
    //printf("\nww: %u\n", wwl.size());
    //printf("after redund %d\n", wwl.size());
    vector< map<range, int, less_range> > ww_rm;
    get_range_map(wwl, ww_rm, rb);
    analyze_port(rb, ww_rm, wwl, mi);

    //printf("\nws: %u\n", wsl.size());
    //vector< map<range, int, less_range> > ws_rm;
    //get_range_map(wsl, ws_rm, rb);
    //analyze_port(rb, ws_rm, wsl, mi);

    //printf("\nsw: %u\n", swl.size());
    //vector< map<range, int, less_range> > sw_rm;
    //get_range_map(swl, sw_rm, rb);
    //analyze_port(rb, sw_rm, swl, mi);

    //printf("\nss: %u\n", ssl.size());
    //vector< map<range, int, less_range> > ss_rm;
    //get_range_map(ssl, ss_rm, rb);
    //analyze_port(rb, ss_rm, ssl, mi);


    //if(just_analyze) {
    //    //dump_rules(wwl, "ww");
    //    //dump_rules(wsl, "ws");
    //    //dump_rules(swl, "sw");
    //    //dump_rules(ssl, "ss");
    //}
    
#endif

}

#define DEFINITELY_NOT 1
#define MAYBE_NOT 2 
#define SPLIT 3

int split_or_not(struct mem_info mi, vector<int> &btdv, uint64_t threshold)
{
    vector<int>::iterator vit = max_element(btdv.begin() + 2, btdv.end());

    if(mi.mt < threshold) {
        return DEFINITELY_NOT;
    }
    else {
        if( mi.mwssw < threshold && mi.ww < (1<<(*vit)) * bucketSize/2){
            return MAYBE_NOT;
        }
        else
            return SPLIT;
    }
}

#define TEST_BUILD_HYPERCUTS  1
#define TEST_BUILD_HYPERSPLIT 2 


int choose_right_algorithms(int skew)
{
    if(skew == 1) {
        return TEST_BUILD_HYPERSPLIT;
    }
    else 
        return TEST_BUILD_HYPERCUTS;

}

#define HCPATH "../hc/"
#define HCCMD "hypc"
#define HSPATH "../hs_lookup/"
#define HSCMD "hs"

#define PREDICT_RIGHT 1
#define PREDICT_WRONG 2

void check_ret(int *mem, int *acc, string bn)
{
    char cmd_str[256];
    snprintf(cmd_str, 256, "./check.pl %s", bn.c_str());

    FILE *fp = popen(cmd_str, "r");
    fscanf(fp, "mem = %d, acc = %d", mem, acc); 
    fclose(fp);
    //if(*mem == -1 || *acc == -1) {
    //    printf("error\n");
    //    exit(-1);
    //}
}
#define H1 1
#define H2 2
#define BS 3 
#define H2PRE0 4
#define H2PRE1 5 


void run_building_program(int cmd, int t, string bn, string rs)
{
    char cmd_str[256];
    char type[32];
    char tn[16];
    char type_str[128];
    static int tnum = 0;


    switch(cmd) {
        case TEST_BUILD_HYPERCUTS:
            cout<<"RUNNING: HYPERCUTS"<<endl;
            if(t == H1) {
                sprintf(type, "%s", "-h1");
            }
            if(t == H2) {
                sprintf(type, "%s", "-h2");
            }
            if(t == H2PRE0) {
                sprintf(type, "%s", "-h2 -d 0");
            }
            if(t == H2PRE1) {
                sprintf(type, "%s", "-h2 -d 1");
            }
            sprintf(tn, "tree%d", tnum++);

            snprintf(cmd_str, 256, "%s%s -r %s %s -o %s", HCPATH, HCCMD, rs.c_str(), type, tn);
            snprintf(type_str,128, "touch %d%s", tnum, "hc");
            break;
        case TEST_BUILD_HYPERSPLIT:
            cout<<"RUNNING: HYPERSPLIT"<<endl;
            sprintf(tn, "tree%d", tnum++);

            if(t == BS) {
                sprintf(type, "%d", 8);
                snprintf(cmd_str, 256, "%s%s -r %s -b %s -o %s", HSPATH, HSCMD, rs.c_str(), type, tn);
            }
            else {
                snprintf(cmd_str, 256, "%s%s -r %s -o %s", HSPATH, HSCMD, rs.c_str(), tn);
            }
            snprintf(type_str,128, "touch %d%s", tnum, "hs");
            break;
    }
    printf("%s\n", cmd_str);
    
    system(cmd_str);
    system(type_str);

}

void get_sp_bd_v(list<pc_rule*> &pr, vector<int> &stepsv, vector<int> &btdv, rule_boundary &rb)
{
    vector< map<range, int, less_range> > range_map;
    get_range_map(pr, range_map, rb);
    for(int i = 0; i < MAXDIMENSIONS; i++) {
        split_tree tr(rb.field[i], bucketSize, 32);
        for(map<range, int, less_range>::iterator mit = range_map[i].begin();
                mit != range_map[i].end();
                ++mit) {
            //if(!check_range_size(mit->first, rb, i)) {
                tr.split_add_range(mit->first, mit->second);
            //}
        }
        tr.split_create();

        int btd;
        int steps = tr.measuring_steps(btd);

        btdv.push_back(btd);

        //if(i==0 || i == 1)
        stepsv.push_back(steps);

        //if(btd > depth/2 && steps > depth/2) {
        //    vector<vector<range> > vr;
        //    tr.get_xbtd_range(vr, btd, 2);

        //    for(size_t i = 0; i < vr.size(); i++){
        //        for(size_t j = 0; j < vr[i].size(); j++) {
        //            cout<<"range: "<<vr[i][j].low << " - " << vr[i][j].high << endl;

        //        }
        //    }
        //    
        //}

        printf("dim %d: ranges %d  steps %d, btd %d\n" , i, range_map[i].size(), steps, btd);
        
    }



}

void us_mix_split(list<pc_rule*> &pr, rule_boundary &rb) 
{
    //split into four?
    //is that ok we split them into four
    //we try to split them into four
    //and test the uniformity of three subsets
    //that is ss, sw, ws.
    list<pc_rule*> ssl;
    list<pc_rule*> swl;
    list<pc_rule*> wsl;
    list<pc_rule*> wwl;

    for(list<pc_rule*>::iterator it = pr.begin();
            it != pr.end();
            it ++ ) {
        if(check_range_size((*it)->field[0], rb, 0) &&
                check_range_size((*it)->field[1], rb, 1)) {
            wwl.push_back(*it);

        }

        if(!check_range_size((*it)->field[0], rb, 0) &&
                !check_range_size((*it)->field[1], rb, 1)) {
            ssl.push_back(*it);
        }

        if(check_range_size((*it)->field[0], rb, 0) &&
                !check_range_size((*it)->field[1], rb, 1)) {
            wsl.push_back(*it);

        }

        if(!check_range_size((*it)->field[0], rb, 0) &&
                check_range_size((*it)->field[1], rb, 1)) {
            swl.push_back(*it);
        }
    }

    vector<int>stepsv, btdv;
    get_sp_bd_v(ssl, stepsv, btdv, rb);
    get_sp_bd_v(swl, stepsv, btdv, rb);
    get_sp_bd_v(wsl, stepsv, btdv, rb);




}


void split_rules(list<pc_rule*> &pr, struct mem_info mi, vector<int> btdv, vector<int> stepsv, int skew)
{
    rule_boundary rb;
    init_boundary(rb);

    int mem, acc;
    int mem_total = 0;
    int acc_total = 0;
    int depth = (uint32_t)log2(pr.size());

    //fisrt consider only remove the ww part.
#if 0
    if(mi.mwssw < THRESHOLD) {
        list<pc_rule*> wsswl;
        list<pc_rule*> wwl;
        for(list<pc_rule*>::iterator it = pr.begin();
                it != pr.end();
                it ++) {
            if(check_range_size((*it)->field[0], rb, 0) 
                   &&  check_range_size((*it)->field[1], rb, 1)) {
                wwl.push_back(*it);
            }
            else {
                wsswl.push_back(*it);
            }
        }
        //remove_redund(wwl, rb);
        dump_rules(wwl, "ww");
        dump_rules(wsswl, "wssw");

        if(skew) {
            RSOUT("wssw", wsswl);
            run_building_program(TEST_BUILD_HYPERSPLIT, H1, "wssw_build", "wssw");
            RSOUT("ww", wwl);
            run_building_program(TEST_BUILD_HYPERSPLIT, BS, "ww_build", "ww");
        }
        else {
            RSOUT("wssw", wsswl);
            //vector<int>::iterator bit = min_element(btdv.begin(), btdv.begin() +2);
            //if(*bit <= depth/2){ 
            //    run_building_program(TEST_BUILD_HYPERCUTS, H1, "wssw_build", "wssw");
            //}
            //else {
            //    //if(*bit == btdv[0])
            //        //run_building_program(TEST_BUILD_HYPERCUTS, H2PRE0, "wssw_build", "wssw");
            //    //else
            //    run_building_program(TEST_BUILD_HYPERCUTS, H2, "wssw_build", "wssw");
            //}

            run_building_program(TEST_BUILD_HYPERCUTS, H1, "wssw_build", "wssw");
            RSOUT("ww", wwl);
            run_building_program(TEST_BUILD_HYPERSPLIT, BS, "ww_build", "ww");

        }

        check_ret(&mem, &acc, "wssw_build");
        //printf("wssw: mem = %d, acc = %d\n", mem, acc); 
        DOUT("wssw", mem, acc);
        mem_total += mem;
        acc_total += acc;
        
        check_ret(&mem, &acc, "ww_build");
        //printf("ww: mem = %d, acc = %d\n", mem, acc); 
        DOUT("ww", mem, acc);
        mem_total += mem;
        acc_total += acc;

        mem_total += pr.size() * RULE_ENTRY_SIZE;

        if(mem_total < THRESHOLD) {
            cout<<"PREDICT RIGHT\n"<<endl; 
            DOUT("total", mem_total, acc_total);
            return;
        }
        else {
            cout<<"PREDICT WRONG, need to split ws/sw\n"<<endl;
            mem = 0;
            acc = 0;
            mem_total = 0;
            acc_total = 0;
        }
    }
#endif

    //choose a smaller steps dimension to do the merge.
#if 0
    if(stepsv[0] > depth/2 && btdv[0] > depth/2 
            && stepsv[1] > depth/2 && btdv[1] > depth/2) {
        us_mix_split(pr, rb);
    }
#endif

    list<pc_rule*> wwl;

    if(stepsv[0] < stepsv[1]) {
        //merge ss with sw, split ws
        list<pc_rule*> sxl;
        list<pc_rule*> wsl;
        for(list<pc_rule*>::iterator it = pr.begin();
                it != pr.end();
                it ++) {
            if(!check_range_size((*it)->field[0], rb, 0)) { 
                sxl.push_back(*it);
            }
            else if(!check_range_size((*it)->field[1], rb, 1)) {
                wsl.push_back(*it);
            }
            else {
                wwl.push_back(*it);
            }
        }

        dump_rules(sxl, "sx");
        dump_rules(wsl, "ws");
        remove_redund(wwl, rb);
        dump_rules(wwl, "ww");

        RSOUT("sx", sxl);
        if(stepsv[0] > depth/2) {
            run_building_program(TEST_BUILD_HYPERSPLIT, H1, "sx_build", "sx");
        }
        else {
            //if(btdv[0] <= depth/2){ 
            //    run_building_program(TEST_BUILD_HYPERCUTS, H1, "sx_build", "sx");
            //}
            //else {
                run_building_program(TEST_BUILD_HYPERCUTS, H2PRE0, "sx_build", "sx");
            //}

            //run_building_program(TEST_BUILD_HYPERCUTS, H1, "sx_build", "sx");
        }

        //check_ret(&mem, &acc, "sx_build");
        //printf("sx: mem = %d, acc = %d\n", mem, acc); 
        //DOUT("sx", mem, acc);
        //mem_total += mem;
        //acc_total += acc;


        RSOUT("ws", wsl);
        if(stepsv[1] > depth/2) {
            run_building_program(TEST_BUILD_HYPERSPLIT, H1, "ws_build", "ws");
        }
        else {
            //if(btdv[1] <= depth/2){ 
            //    run_building_program(TEST_BUILD_HYPERCUTS, H1, "ws_build", "ws");
            //}
            //else {
                run_building_program(TEST_BUILD_HYPERCUTS, H2PRE1, "ws_build", "ws");
            //}

            //run_building_program(TEST_BUILD_HYPERCUTS, H1, "ws_build", "ws");
        }

        //check_ret(&mem, &acc, "ws_build");
        //printf("ws: mem = %d, acc = %d\n", mem, acc); 
        //DOUT("ws", mem, acc);
        //mem_total += mem;
        //acc_total += acc;

        //RSOUT("ww", wwl);
        run_building_program(TEST_BUILD_HYPERSPLIT, BS, "ww_build", "ww");

        //check_ret(&mem, &acc, "ww_build");
        //printf("ww: mem = %d, acc = %d\n", mem, acc); 
        //DOUT("ww", mem, acc);
        //mem_total += mem;
        //acc_total += acc;
        //printf("total: mem = %d, acc = %d\n", mem_total, acc_total); 
        //mem_total += pr.size() * RULE_ENTRY_SIZE; 
        //DOUT("total", mem_total, acc_total);

    }
    else {
        //merge ss with ws, split sw
        list<pc_rule*> xsl;
        list<pc_rule*> swl;
        for(list<pc_rule*>::iterator it = pr.begin();
                it != pr.end();
                it ++) {
            if(!check_range_size((*it)->field[1], rb, 1)) { 
                xsl.push_back(*it);
            }
            else if(!check_range_size((*it)->field[0], rb, 0)) {
                swl.push_back(*it);
            }
            else {
                wwl.push_back(*it);
            }
        }

        dump_rules(xsl, "xs");
        dump_rules(swl, "sw");
        remove_redund(wwl, rb);
        dump_rules(wwl, "ww");

        RSOUT("sw", swl);
        if(stepsv[0] > depth/2) {
            run_building_program(TEST_BUILD_HYPERSPLIT, H1, "sw_build", "sw");
        }
        else {
        //    if(btdv[0] <= depth/2)  
        //        run_building_program(TEST_BUILD_HYPERCUTS, H1, "sw_build", "sw");
        //    else
                run_building_program(TEST_BUILD_HYPERCUTS, H2PRE0, "sw_build", "sw");

        }

        //check_ret(&mem, &acc, "sw_build");
        //printf("sw: mem = %d, acc = %d\n", mem, acc); 
        //DOUT("sw", mem, acc);
        //mem_total += mem;
        //acc_total += acc;

        RSOUT("xs", xsl);
        if(stepsv[1] > depth/2) {
            run_building_program(TEST_BUILD_HYPERSPLIT, H1, "xs_build", "xs");
        }
        else {
        //    if(btdv[1] <= depth/2)
        //        run_building_program(TEST_BUILD_HYPERCUTS, H1, "xs_build", "xs");
         //   else
                run_building_program(TEST_BUILD_HYPERCUTS, H2PRE1, "xs_build", "xs");
        }

        //check_ret(&mem, &acc, "xs_build");
        //printf("ws: mem = %d, acc = %d\n", mem, acc); 
        //DOUT("ws", mem, acc);
        //mem_total += mem;
        //acc_total += acc;
        
        //RSOUT("ww", wwl);
        run_building_program(TEST_BUILD_HYPERSPLIT, BS, "ww_build", "ww");
        //run_building_program(TEST_BUILD_HYPERCUTS, H2, "ww_build", "ww");

        //check_ret(&mem, &acc, "ww_build");
        //printf("ww: mem = %d, acc = %d\n", mem, acc); 
        //DOUT("ww", mem, acc);
        //mem_total += mem;
        //acc_total += acc;
        //printf("total: mem = %d, acc = %d\n", mem_total, acc_total); 
        mem_total += pr.size() * RULE_ENTRY_SIZE; 
        DOUT("total", mem_total, acc_total);
    }

}

void ww_kick(list<pc_rule*> &pr, rule_boundary &rb)
{
    list<pc_rule*> wsswl;
    list<pc_rule*> wwl;
    list<pc_rule*> ssl;
    for(list<pc_rule*>::iterator it = pr.begin();
            it != pr.end();
            it ++) {
        if(check_range_size((*it)->field[0], rb, 0) 
                &&  check_range_size((*it)->field[1], rb, 1)) {
            wwl.push_back(*it);
        }
        else {
            wsswl.push_back(*it);
        }
        if(!check_range_size((*it)->field[0], rb, 0)
                && !check_range_size((*it)->field[1], rb, 0)) {
            ssl.push_back(*it);

        }
    }
    //remove_redund(wwl, rb);
    dump_rules(wwl, "ll");
    dump_rules(wsswl, "lssl");
    dump_rules(ssl, "ss");
}




int main(int argc, char *argv[])
{
    rule_boundary rb;
    list<pc_rule*> p_classifier;

    parseargs(argc, argv);
    loadrules(fpr);
    load_rule_ptr(classifier, p_classifier, 0, classifier.size() - 1);
    init_boundary(rb); 

    vector< map<range, int, less_range> > range_map;
    get_range_map(p_classifier, range_map, rb);

    vector<int> btdv;
    vector<int> stepsv;

    int depth = (unsigned)log2(p_classifier.size());
    printf("log2 N_rule %d\n", depth);
    //check_uniformity(p_classifier, 10);

#if 0 
    list<pc_rule*> wwl;
    for(list<pc_rule*>::iterator it = p_classifier.begin();
                it != p_classifier.end();
                it ++) {
        if(check_range_size((*it)->field[0], rb, 0) 
                &&  check_range_size((*it)->field[1], rb, 1)) {
            wwl.push_back(*it);
        }
    }
    //remove_redund(wwl, rb);
    dump_rules(wwl, "ww");
    return 0;
#endif
    //ww_kick(p_classifier, rb);


    for(int i = 0; i < MAXDIMENSIONS; i++) {
        split_tree tr(rb.field[i], bucketSize, 32);
        for(map<range, int, less_range>::iterator mit = range_map[i].begin();
                mit != range_map[i].end();
                ++mit) {
            if(!check_range_size(mit->first, rb, i)) {
                tr.split_add_range(mit->first, mit->second);
            }
        }
        tr.split_create();

        int btd;
        int steps = tr.measuring_steps(btd);

        //vector<double> hkv;
        //tr.measuring_hk(hkv);
        //for_each(hkv.begin(), hkv.end(), [](double hk){cout<<hk<<endl;});

        btdv.push_back(btd);
        //tr.dump_tree("a");
        //exit(-1);

        //if(i==0 || i == 1)
        stepsv.push_back(steps);

        //if(btd > depth/2 && steps > depth/2) {
        //    vector<vector<range> > vr;
        //    tr.get_xbtd_range(vr, btd, 2);

        //    for(size_t i = 0; i < vr.size(); i++){
        //        for(size_t j = 0; j < vr[i].size(); j++) {
        //            cout<<"range: "<<vr[i][j].low << " - " << vr[i][j].high << endl;

        //        }
        //    }
        //    
        //}

        printf("dim %d: ranges %d  steps %d, btd %d\n" , i, range_map[i].size(), steps, btd);
        
    }
    printf("IP fields %d, Other fields %d\n", range_map[0].size() + range_map[1].size(), 
            range_map[2].size() + range_map[3].size() +range_map[4].size());

    //currently we only compare sIP and dIP.
    vector<int>::iterator sit = max_element(stepsv.begin(), stepsv.begin()+2);
    //int dm;
    //if(range_map[0].size() >= range_map[1].size()) {
    //    dm = stepsv[0];
    //}
    //else
    //    dm = stepsv[1];

    int skew;
    if(*sit > depth/2) {
        skew = 1;
    }
    else {
        skew = 0;
    }
    printf("skew %d\n", skew);

    struct mem_info mi;
    mi.mwssw = 0;
    mi.mss = 0;
    mi.mww = 0;
    mi.mt = 0;

    int cmd;
    int split = 0;
    int mem, acc;

    int skew_sip, skew_dip;
    skew_sip = 0;
    skew_dip = 0;

    if(stepsv[0] >= depth/2)
        skew_sip  = 1;
    if(stepsv[1] >= depth/2)
        skew_dip = 1;

    calc_mem_with_data(rb, range_map, p_classifier, mi, skew_sip, skew_dip); 
    
    if(just_analyze)
        return 0;

    printf("\nANALYSIS FINISHED\n\nNOW CHOOSE ALGOS OR SPLIT RULESETS:\n");
    switch(split_or_not(mi, btdv, THRESHOLD)) 
    {
        case DEFINITELY_NOT:
            printf("SPLIT:only need to build a single tree\n");

            cmd = choose_right_algorithms(skew);
            RSOUT("single", p_classifier);
            //remove_redund(p_classifier, rb);
            run_building_program(cmd, H1, "test_building", rs_name);
            check_ret(&mem, &acc, "test_building");
            DOUT("single", mem, acc);
            break;
        case MAYBE_NOT:
            printf("SPLIT:let's try build a single tree\n");
            cmd = choose_right_algorithms(skew);
            RSOUT("single", p_classifier);
            run_building_program(cmd, H1, "test_building", rs_name);
            check_ret(&mem, &acc, "test_building");
            DOUT("single", mem, acc);
            break;
        case SPLIT:
            printf("SPLIT:let's split\n");
            split_rules(p_classifier, mi, btdv, stepsv, skew);
            split = 1;
            break;
        default:
            break;
    }


    if(split == 0) {
        mem += p_classifier.size() * RULE_ENTRY_SIZE;
        if(mem < THRESHOLD) {
            //printf("mem = %d %dKB %.fMB, acc = %d\n", mem, mem/1024, mem/(1024*1024.), acc);
            DOUT("single", mem, acc);
            printf("PREDICT RIGHT\n");
        }
        else {
            printf("PREDICT WRONG, need to split\n");
            split_rules(p_classifier, mi, btdv, stepsv, skew);
        }
    }
        

    return 0;
}

