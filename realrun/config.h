#ifndef __CONFIG_H__
#define __CONFIG_H__

#define DEFINE_ROOT1 struct table_entry *root1
#define LOAD_TREE1 root1 = load_hs(ifp1)
#define SEARCH_TREE1 (uint32_t)lookup_table(root1, ft+i*MAXDIMENSIONS)
#define DEFINE_ROOT2 struct table_entry *root2
#define LOAD_TREE2 root2 = load_hs(ifp2)
#define SEARCH_TREE2 (uint32_t)lookup_table(root2, ft+i*MAXDIMENSIONS)
#define DEFINE_ROOT3 struct table_entry *root3
#define LOAD_TREE3 root3 = load_hs(ifp3)
#define SEARCH_TREE3 (uint32_t)lookup_table(root3, ft+i*MAXDIMENSIONS)

#endif

