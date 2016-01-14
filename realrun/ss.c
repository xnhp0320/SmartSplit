#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include "pc.h"
#include "config.h"

FILE *fpr;
FILE *fpt;
FILE *ifp;
FILE *ifp1;
FILE *ifp2;
FILE *ifp3;

void parseargs(int argc, char *argv[]) {
    int	c;
    while ((c = getopt(argc, argv, "r:t:l:m:n:")) != -1) {
        switch (c) {
            case 'r':
                fpr = fopen(optarg, "r");
                break;
            case 't':
                fpt = fopen(optarg, "r");
                break;
            case 'l':
                ifp1 = fopen(optarg, "r");
                break;
            case 'm':
                ifp2 = fopen(optarg, "r");
                break;
            case 'n':
                ifp3 = fopen(optarg, "r");
                break;
            default:
                break;
        }
    }

    
    if(fpr == NULL){
        printf("can't open ruleset\n");
        exit(-1);
    }

    if(ifp1 == NULL || ifp2 == NULL || ifp3 == NULL) {
        printf("can't open tree file\n");
        exit(-1);
    }

}



#define MIN(a,b)   ((a)<(b) ? (a): (b))
int main(int argc, char *argv[])
{
    printf("node size %d\n", sizeof(struct node));
    parseargs(argc, argv);
    rule_set_cnt = loadrules(fpr);
    printf("load rules %d\n", rule_set_cnt);

    //struct node * root1, *root2;
    //struct table_entry *root3;
    DEFINE_ROOT1;
    DEFINE_ROOT2;
    DEFINE_ROOT3;

    struct tree_info info;

    //root1 = load_tree(ifp1, &info);
    //root2 = load_tree(ifp2, &info);
    //root3 = load_hs(ifp3);
    LOAD_TREE1; 
    LOAD_TREE2;
    LOAD_TREE3;

#if 0 
    uint32_t r1,r2,r3;
    int ret;
    int ret2;
    int trace_no = 0;
    int flag = 0;
    uint32_t ft[MAXDIMENSIONS];
    while((trace_no = load_ft(fpt, ft)) != 0) {
         //if(trace_no == 30)
         //   printf("here\n");
        //r1 = (uint32_t)search_rules(root1, ft);
        r1 = (uint32_t)lookup_table(root1, ft);
        //r2 = (uint32_t)search_rules(root2, ft);
        r2 = (uint32_t)lookup_table(root2, ft);
        r3 = (uint32_t)lookup_table(root3, ft);
        //r1 = SEARCH_TREE1;
        //r2 = SEARCH_TREE2;
        //r3 = SEARCH_TREE3;

        ret = MIN(r1, MIN(r3,r2));
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
    uint32_t r1,r2,r3;
    clock_gettime(CLOCK_MONOTONIC, &tp_b);
    for(i=i-1;i>=0;i--) {
        r1 = SEARCH_TREE1;
        r2 = SEARCH_TREE2;
        r3 = SEARCH_TREE3;
        //r1 = (uint32_t)search_rules(root1, ft);
        //r2 = (uint32_t)search_rules(root2, ft);
        //r3 = (uint32_t)lookup_table(root3, ft);
        ret = MIN(r1, MIN(r3,r2));
#ifdef STAT
        if(acc > max_acc)
            max_acc = acc;
        sum_acc+=acc;
        acc=0;
#endif
    }
    clock_gettime(CLOCK_MONOTONIC, &tp_a);

    printf("%lu %lu\n", tp_a.tv_sec, tp_a.tv_nsec);
    printf("%lu %lu\n", tp_b.tv_sec, tp_b.tv_nsec);
    total_time = ((long)tp_a.tv_sec - (long)tp_b.tv_sec) * 1000000000ULL + 
            (tp_a.tv_nsec - tp_b.tv_nsec);
#ifdef STAT
    printf("mem %lu %.2fKB %.2fMB\n", total_size, (double)total_size/1024, (double)total_size/(1024*1024));
    printf("max_acc %u avg_acc %.2f\n", max_acc, (double)sum_acc/trace_cnt);
#endif

    printf("time: %ld\n", total_time); 
    double ave_time = (double)total_time / trace_cnt;
    printf("ave time: %.1fns\n", ave_time);
    double gbps = (1/ave_time) * 1e9 / 1.48e6;
    printf("gbps: %.1fGbps\n", gbps);  

#endif
    
    return 0;

}


