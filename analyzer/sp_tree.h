#ifndef __SP_TREE_H__
#define __SP_TREE_H__

#include "hypc.h"
#include <algorithm>
#include <list>
#include <utility>
#include <string>

using namespace std;

class split_node
{
    public:
        split_node(range rg, int d);
        ~split_node();

        split_node *left;
        split_node *right;
        struct range r;
        list<pair<range, int> > rl;
        int cnt;
        int r_cnt;
        int depth;
        int btd;
        int step;
        double p;
        double h;
};



class split_tree
{
    public:
        split_node root;
        int tsh;
        int depth_limit;

        int max_depth;
        int max_step;
        int max_btd;
        int min_btd;
        int leaf_up;
        double depth_sum;
        int leaf_cnt;

        list<split_node*> nl;
        
        split_tree(range r, int th, int dl);
        ~split_tree();
        int height();
        double ave_height();
        void split_ranges(split_node*);
        void node_update(split_node*);
        void compact_range(split_node &);
        void split_create_breadth();

        split_node *choose_a_suitable_leaf(list<split_node*> &snl);
        void split_create_evenness();

        void split_create_cost();
        int need_delete_node(split_node *node);
        int small_parts(split_node *n, unsigned long long s);
        void balance_leaves(list<split_node *> &leaves);
        int large_parts(split_node *n, unsigned long long s);
        int calc_index(split_node *n, unsigned long long s);
        int duplicate_cut(split_node * curr_node);
        int unbalance_tree_depth(int *fuse_array, int *fuse_array_num);
        void split_add_range(const range &r, int cnt);
        void split_create();
        void dump_tree(string file);
        //int balance_tree_depth(list<split_node*> &);

        void merge_leaves_v(list<split_node*> &leaves, vector<int> &v,unsigned long long s);

        void interval_leaves_v(list<split_node*> & leaves,vector<int> &v, unsigned long long s);

        void dump_tree_with_level(string file, int level);
        void twice_balance_tree_depth(list<split_node*> & leaf, vector<int> &btd_l, int &skip);

        void measure_steps(split_node* n, int step, int label);
        int measuring_steps(int &btd);
        int get_bd();
        void get_xbtd_range(vector<vector<range> > &vr, int btd, int allow_step);
        void find_tail(split_node *n, vector<range> &ht, int allow_step);

        void get_max_skew_range(range &r);
        int leaf_num();
        
        double measure_h(split_node *n);
        void measuring_hk(vector<double> &hk);
        void update_p(split_node *n, double );
};




#endif
