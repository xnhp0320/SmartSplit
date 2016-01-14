/*-----------------------------------------------------------------------------
 * Name:		hs.h
 * Version:		0.1
 * Description: hyper-split packet classification algorithm
 * Author:		Yaxuan Qi
 * Date:		07/15/2008
 *-----------------------------------------------------------------------------*/

#ifndef  _HS_H
#define  _HS_H


/*-----------------------------------------------------------------------------
 *  constant
 *-----------------------------------------------------------------------------*/

//#define	DEBUG
#define	LOOKUP

/* for 5-tuple classification */
#define DIM			5

/* for function return value */
#define SUCCESS		1
#define FAILURE		0
#define TRUE		1
#define FALSE		0

/* locate rule files */
#define RULEPATH "./rules/"

/* for bitmap */
#define MAXFILTERS	128*1024 /* support 128K rules */	
#define WORDLENGTH	32	/* for 32-bit system */	
#define BITMAPSIZE	256 /* MAXFILTERS/WORDLENGTH */				


/*-----------------------------------------------------------------------------
 *  structure
 *-----------------------------------------------------------------------------*/
struct FILTER						
{
	unsigned int cost;		
	unsigned int dim[DIM][2];
	unsigned char act;	
};

struct FILTSET
{
	unsigned int	numFilters;	
	struct FILTER	filtArr[MAXFILTERS];
};


struct TPOINT
{
	unsigned int value;
	unsigned char flag;	
};

struct FRAGNODE
{
	unsigned int start;
	unsigned int end;
	struct FRAGNODE *next;
};

struct FRAGLINKLIST
{
	unsigned int fragNum;
	struct FRAGNODE *head;
};

struct TFRAG
{
	unsigned int value;						// end point value
	unsigned int cbm[BITMAPSIZE];					// LENGTH * SIZE bits, CBM
};
struct TFRAG* ptrTfrag[2];						// released after tMT[2] is generated

struct FRAG
{
	unsigned int value;
};
struct FRAG* ptrfrag[2];
unsigned int fragNum[2];

struct CES
{
	unsigned short eqID;					// 2 byte, eqID;
	unsigned int  cbm[BITMAPSIZE];	
	struct	CES *next;								// next CES
};

struct LISTEqS
{
	unsigned short	nCES;					// number of CES
	struct			CES *head;								// head pointer of LISTEqS 
	struct			CES *rear;								// pointer to end node of LISTEqS
};
struct LISTEqS* listEqs[6];

struct PNODE
{
	unsigned short	cell[65536];			// each cell stores an eqID
	struct			LISTEqS listEqs;					// list of Eqs
};
struct PNODE portNodes[2];

/*for hyper-splitting tree*/
typedef	struct rule_s
{
	unsigned int	pri;
	unsigned int	range[DIM][2];
} rule_t;

typedef struct rule_set_s
{
	unsigned int	num; /* number of rules in the rule set */
	rule_t*			ruleList; /* rules in the set */
} rule_set_t;

typedef	struct seg_point_s
{
	unsigned int	num;	/* number of segment points */
	unsigned int*	pointList;	/* points stores here */
} seg_point_t;

typedef struct segments_s
{
	unsigned int	num;		/* number of segment */
	unsigned int	range[2];	/* segment */
} segments_t;

typedef	struct search_space_s
{
	unsigned int	range[DIM][2];
} search_space_t;

typedef struct hs_node_s
{
	unsigned char		d2s;		/* dimension to split, 2bit is enough */
	unsigned char		depth;		/* tree depth of the node, x bits supports 2^(2^x) segments */
	unsigned int		thresh;		/* thresh value to split the current segments */
	struct hs_node_s*	child[2];	/* pointer to child-node, 2 for binary split */
    rule_set_t *rule_set;

} hs_node_t;

/*-----------------------------------------------------------------------------
 *  function declaration
 *-----------------------------------------------------------------------------*/

/* read rules from file */
int		ReadFilterFile(rule_set_t* ruleset, char* filename); /* main */
void	LoadFilters(FILE* fp, struct FILTSET* filtset);
int		ReadFilter(FILE* fp, struct FILTSET* filtset,	unsigned int cost);
void	ReadIPRange(FILE* fp, unsigned int* IPrange);
void	ReadPort(FILE* fp, unsigned int* from, unsigned int* to);
void    ReadProtocol(FILE* fp, unsigned int* from, unsigned int* to);

/* build hyper-split-tree */
int		BuildHSTree(rule_set_t* ruleset, hs_node_t* node, unsigned int depth); /* main */
int		SegPointCompare(const void * a, const void * b);

/* lookup hyper-split-tree */
int		LookupHSTree(rule_set_t* ruleset, hs_node_t* root);

#endif   /* ----- #ifndef _HS_H ----- */
