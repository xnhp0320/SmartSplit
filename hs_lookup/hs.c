/*-----------------------------------------------------------------------------
 *  
 *  Name:			hs.c
 *  Description:	hyper-split packet classification algorithm
 *  Version:		1.0 (release)
 *  Author:			Yaxuan Qi (yaxuan.qi@gmail.com)
 *  Date:			07/15/2008 ~ 07/28/2008
 *
 *  comments:		
 *					1) refine the code from UCSD
 *					2) add quad and oct search
 *
 *-----------------------------------------------------------------------------*/

#ifndef  _HS_C
#define  _HS_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include "hs.h"
#include "list.h"




/*-----------------------------------------------------------------------------
 *  globals
 *-----------------------------------------------------------------------------*/
unsigned int	gChildCount = 0;

unsigned int	gNumTreeNode = 0;
unsigned int	gNumLeafNode = 0;

unsigned int	gWstDepth = 0;
unsigned int	gAvgDepth = 0;
unsigned int    gMaxLeafNum = 0;
unsigned int    gMaxL_D = 0;
unsigned int    gMaxL_N = 0;
unsigned int    gMaxRulesinLeaf = 0;
unsigned int    gRulePointers = 0;

unsigned int	gNumNonOverlappings[DIM];
unsigned long long	gNumTotalNonOverlappings = 1;


char filename[128];
char tracename[128];
char fibname[128];
int bucketSize = 16;

struct timeval	gStartTime,gEndTime; 

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  ReadIPRange
 *  Description:  
 * =====================================================================================
 */
void ReadIPRange(FILE* fp, unsigned int* IPrange)
{
	/*asindmemacces IPv4 prefixes*/
	/*temporary variables to store IP range */
	unsigned int trange[4];	
	unsigned int mask;
	char validslash;
	int masklit1;
	unsigned int masklit2,masklit3;
	unsigned int ptrange[4];
	int i;
	/*read IP range described by IP/mask*/
	/*fscanf(fp, "%d.%d.%d.%d/%d", &trange[0],&trange[1],&trange[2],&trange[3],&mask);*/
	if (4 != fscanf(fp, "%d.%d.%d.%d", &trange[0],&trange[1],&trange[2],&trange[3])) {
		printf ("\n>> [err] ill-format IP rule-file\n");
		exit (-1);
	}
	if (1 != fscanf(fp, "%c", &validslash)) {
		printf ("\n>> [err] ill-format IP slash rule-file\n");
		exit (-1);
	}
	/*deal with default mask*/
	if(validslash != '/')
		mask = 32;
	else {
		if (1 != fscanf(fp,"%d", &mask)) {
			printf ("\n>> [err] ill-format mask rule-file\n");
			exit (-1);
		}
	}
	mask = 32 - mask;
	masklit1 = mask / 8;
	masklit2 = mask % 8;
	
	for(i=0;i<4;i++)
		ptrange[i] = trange[i];

	/*count the start IP */
	for(i=3;i>3-masklit1;i--)
		ptrange[i] = 0;
	if(masklit2 != 0){
		masklit3 = 1;
		masklit3 <<= masklit2;
		masklit3 -= 1;
		masklit3 = ~masklit3;
		ptrange[3-masklit1] &= masklit3;
	}
	/*store start IP */
	IPrange[0] = ptrange[0];
	IPrange[0] <<= 8;
	IPrange[0] += ptrange[1];
	IPrange[0] <<= 8;
	IPrange[0] += ptrange[2];
	IPrange[0] <<= 8;
	IPrange[0] += ptrange[3];
	
	/*count the end IP*/
	for(i=3;i>3-masklit1;i--)
		ptrange[i] = 255;
	if(masklit2 != 0){
		masklit3 = 1;
		masklit3 <<= masklit2;
		masklit3 -= 1;
		ptrange[3-masklit1] |= masklit3;
	}
	/*store end IP*/
	IPrange[1] = ptrange[0];
	IPrange[1] <<= 8;
	IPrange[1] += ptrange[1];
	IPrange[1] <<= 8;
	IPrange[1] += ptrange[2];
	IPrange[1] <<= 8;
	IPrange[1] += ptrange[3];
}

void ReadPort(FILE* fp, unsigned int* from, unsigned int* to)
{
	unsigned int tfrom;
	unsigned int tto;
	if ( 2 !=  fscanf(fp,"%d : %d",&tfrom, &tto)) {
		printf ("\n>> [err] ill-format port range rule-file\n");
		exit (-1);
	}
	*from = tfrom;
	*to = tto;
}

void ReadProtocol(FILE* fp, unsigned int* from, unsigned int* to)
{
    //TODO: currently, only support single protocol, or wildcard
    char dump=0;    
    unsigned int proto=0,len=0;
    if ( 7 != fscanf(fp, " %c%c%x%c%c%c%x",&dump,&dump,&proto,&dump,&dump,&dump,&len)) {
		printf ("\n>> [err] ill-format protocol rule-file\n");
		exit (-1);
	}
    if (len==0xff) {
        *from = proto;
        *to = *from;    
    } else {
        *from = 0x0;
        *to = 0xff;
    }
}


int ReadFilter(FILE* fp, struct FILTSET* filtset,	unsigned int cost)
{
	/*allocate a few more bytes just to be on the safe side to avoid overflow etc*/
	char validfilter;	/* validfilter means an '@'*/
	struct FILTER *tempfilt,tempfilt1;
	
	while (!feof(fp))
	{
		
		if ( 0 != fscanf(fp,"%c",&validfilter)) {
			/*printf ("\n>> [err] ill-format @ rule-file\n");*/
			/*exit (-1);*/
		}
		if (validfilter != '@') continue;	/* each rule should begin with an '@' */

		tempfilt = &tempfilt1;
		ReadIPRange(fp,tempfilt->dim[0]);					/* reading SIP range */
		ReadIPRange(fp,tempfilt->dim[1]);					/* reading DIP range */

		ReadPort(fp,&(tempfilt->dim[2][0]),&(tempfilt->dim[2][1]));
		ReadPort(fp,&(tempfilt->dim[3][0]),&(tempfilt->dim[3][1]));
		
		ReadProtocol(fp,&(tempfilt->dim[4][0]),&(tempfilt->dim[4][1]));
		
		/*read action taken by this rule
				fscanf(fp, "%d", &tact);		
				tempfilt->act = (unsigned char) tact;

		read the cost (position) , which is specified by the last parameter of this function*/
        int junk,junkmask;
        if(2 != fscanf(fp, "%x/%x", &junk, &junkmask)) {
            exit(-1);
        }
		//tempfilt->cost = cost;
        if(1 != fscanf(fp, "%d", &tempfilt->cost)) {
            printf("wrong ruleset format, missing priority\n"); 
            exit(-1);
        }
		
		// copy the temp filter to the global one
		memcpy(&(filtset->filtArr[filtset->numFilters]),tempfilt,sizeof(struct FILTER));
		
		filtset->numFilters++;	   
		return SUCCESS;
	}
	return FALSE;
}


void LoadFilters(FILE *fp, struct FILTSET *filtset)
{
	int line = 0;	
	filtset->numFilters = 0;
	while(!feof(fp)) 
	{
		ReadFilter(fp,filtset,line);
		line++;
	}
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:	ReadFilterFile
 *  Description:	Read rules from file. 
 *					Rules are stored in 'filterset' for range matching
 * =====================================================================================
 */
int ReadFilterFile(rule_set_t*	ruleset, char* filename)
{
	int		i, j;
	FILE*	fp;
	struct FILTSET	filtset;		/* filter set for range match */


	fp = fopen (filename, "r");
	if (fp == NULL) 
	{
		printf("Couldnt open filter set file \n");
		return	FAILURE;
	}

	LoadFilters(fp, &filtset);	
	fclose(fp);

    /*
	 *yaxuan: copy rules to dynamic structrue, and from now on, everything is new:-)
     */
	ruleset->num = filtset.numFilters;
	ruleset->ruleList = (rule_t*) malloc(ruleset->num * sizeof(rule_t));
	for (i = 0; i < ruleset->num; i++) {
		ruleset->ruleList[i].pri = filtset.filtArr[i].cost;
		for (j = 0; j < DIM; j++) {
			ruleset->ruleList[i].range[j][0] = filtset.filtArr[i].dim[j][0];
			ruleset->ruleList[i].range[j][1] = filtset.filtArr[i].dim[j][1];
		}
	}
	/*printf("\n>>number of rules loaded from file: %d", ruleset->num);*/

	return	SUCCESS;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  Compare
 *  Description:  for qsort
 *     Comments:  who can make it better?
 * =====================================================================================
 */
int SegPointCompare (const void * a, const void * b)
{
	if ( *(unsigned int*)a < *(unsigned int*)b )
		return -1;
	else if ( *(unsigned int*)a == *(unsigned int*)b )
		return 0;
	else 
		return 1;
}


int is_equal(rule_t *a, rule_t *b)
{
    int i;
    int count = 0;
    for (i = 0; i < DIM; i++) {
        if(a->range[i][0] <= b->range[i][0]
                 && a->range[i][1]>= b->range[i][1])
            count ++;
    }
    if(count == DIM) 
        return 1;
    else 
        return 0;
}

void remove_redund(rule_set_t **ruleset)
{
    int i,j;
    int pos = 0;
    int found = 0;

    rule_set_t * newlist = (rule_set_t*) malloc(sizeof(rule_set_t));
    newlist->num = 0;
    newlist->ruleList = (rule_t*) malloc( (*ruleset)->num * 
            sizeof(rule_t) );
    
    for(i=0; i < (*ruleset)->num; i++) {
        found = 0;
        for(j=0; j < newlist->num; j++) {
            if(is_equal(&(newlist->ruleList[j]), 
                        &((*ruleset)->ruleList[i]))) 
            {
                found = 1;
                break;
            }
        }
        if (found != 1) {
            newlist->ruleList[pos] = (*ruleset)->ruleList[i];
            newlist->num ++;
            pos ++;
        }
    }
    free((*ruleset)->ruleList);
    free((*ruleset));
    *ruleset = newlist;
}


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  BuildHSTree
 *  Description:  building hyper-splitting tree via recursion
 * =====================================================================================
 */
int	BuildHSTree (rule_set_t* ruleset, hs_node_t* currNode, unsigned int depth)
{
	/* generate segments for input filtset */
	unsigned int	dim, num, pos;
	unsigned int	maxDiffSegPts = 1;	/* maximum different segment points */
	unsigned int	d2s = 0;		/* dimension to split (with max diffseg) */
	unsigned int	thresh;
	unsigned int	range[2][2];	/* sub-space ranges for child-nodes */
	unsigned int	*segPoints[DIM];
	unsigned int	*segPointsInfo[DIM];
	unsigned int	*tempSegPoints;
	unsigned int	*tempRuleNumList;
	float			hightAvg, hightAll;
	rule_set_t		*childRuleSet;

#ifdef	DEBUG
	/*if (depth > 10)	exit(0);*/
	printf("\n\n>>BuildHSTree at depth=%d", depth);
	printf("\n>>Current Rules:");
	for (num = 0; num < ruleset->num; num++) {
		printf ("\n>>%5dth Rule:", ruleset->ruleList[num].pri);
		for (dim = 0; dim < DIM; dim++) {
			printf (" [%-8x, %-8x]", ruleset->ruleList[num].range[dim][0], ruleset->ruleList[num].range[dim][1]);
		}
	}
#endif /* DEBUG */
	
	/*Generate Segment Points from Rules*/
	for (dim = 0; dim < DIM; dim ++) {
		/* N rules have 2*N segPoints */
		segPoints[dim] = (unsigned int*) malloc ( 2 * ruleset->num * sizeof(unsigned int));
		segPointsInfo[dim] = (unsigned int*) malloc ( 2 * ruleset->num * sizeof(unsigned int));
		for (num = 0; num < ruleset->num; num ++) {
			segPoints[dim][2*num] = ruleset->ruleList[num].range[dim][0];
			segPoints[dim][2*num + 1] = ruleset->ruleList[num].range[dim][1];
		}
	}
	/*Sort the Segment Points*/
	for(dim = 0; dim < DIM; dim ++) {
		qsort(segPoints[dim], 2*ruleset->num, sizeof(unsigned int), SegPointCompare);
	}

	/*Compress the Segment Points, and select the dimension to split (d2s)*/
	tempSegPoints  = (unsigned int*) malloc(2 * ruleset->num * sizeof(unsigned int)); 
	hightAvg = 2*ruleset->num + 1;
	for (dim = 0; dim < DIM; dim ++) {
		unsigned int	i, j;
		unsigned int	*hightList;
		unsigned int	diffSegPts = 1; /* at least there are one differnt segment point */
		tempSegPoints[0] = segPoints[dim][0];
		for (num = 1; num < 2*ruleset->num; num ++) {
			if (segPoints[dim][num] != tempSegPoints[diffSegPts-1]) {
				tempSegPoints[diffSegPts] = segPoints[dim][num];
				diffSegPts ++;
			}
		}
		/*Span the segment points which is both start and end of some rules*/
		pos = 0;
		for (num = 0; num < diffSegPts; num ++) {
			int	i;
			int ifStart = 0;
			int	ifEnd	= 0;
			segPoints[dim][pos] = tempSegPoints[num];
			for (i = 0; i < ruleset->num; i ++) {
				if (ruleset->ruleList[i].range[dim][0] == tempSegPoints[num]) {
					/*printf ("\n>>rule[%d] range[0]=%x", i, ruleset->ruleList[i].range[dim][0]);*/
					/*this segment point is a start point*/
					ifStart = 1;
					break;
				}
			}
			for (i = 0; i < ruleset->num; i ++) {
				if (ruleset->ruleList[i].range[dim][1] == tempSegPoints[num]) {
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

		/* now pos is the total number of points in the spanned segment point list */

		if (depth == 0) {
			gNumNonOverlappings[dim] = pos;
			gNumTotalNonOverlappings *= (unsigned long long) pos;
		}

#ifdef	DEBUG
		printf("\n>>dim[%d] segs: ", dim);
		for (num = 0; num < pos; num++) {
			/*if (!(num % 10))	printf("\n");*/
			printf ("%x(%u)	", segPoints[dim][num], segPointsInfo[dim][num]);
		}
#endif /* DEBUG */
		
		if (pos >= 3) {
			hightAll = 0;
			hightList = (unsigned int *) malloc(pos * sizeof(unsigned int));
			for (i = 0; i < pos-1; i++) {
				hightList[i] = 0;
				for (j = 0; j < ruleset->num; j++) {
					if (ruleset->ruleList[j].range[dim][0] <= segPoints[dim][i] \
							&& ruleset->ruleList[j].range[dim][1] >= segPoints[dim][i+1]) {
						hightList[i]++;
						hightAll++;
					}
				}
			}
			if (hightAvg > hightAll/(pos-1)) {	/* possible choice for d2s, pos-1 is the number of segs */
				float hightSum = 0;
				
				/* select current dimension */
				d2s = dim;
				hightAvg = hightAll/(pos-1);
				
				/* the first segment MUST belong to the leff child */
				hightSum += hightList[0];
				for (num = 1; num < pos-1; num++) {  /* pos-1 >= 2; seg# = num */
					if (segPointsInfo[d2s][num] == 0) 
						thresh = segPoints[d2s][num] - 1;
					else
						thresh = segPoints[d2s][num];
		
					if (hightSum > hightAll/2) {
						break;
					}
					hightSum += hightList[num];
				}
				/*printf("\n>>d2s=%u thresh=%x\n", d2s, thresh);*/
				range[0][0] = segPoints[d2s][0];
				range[0][1] = thresh;
				range[1][0] = thresh + 1;
				range[1][1] = segPoints[d2s][pos-1];
			}
			/* print segment list of each dim */
#ifdef	DEBUG
			printf("\n>>hightAvg=%f, hightAll=%f, segs=%d", hightAll/(pos-1), hightAll, pos-1);
			for (num = 0; num < pos-1; num++) {
				printf ("\nseg%5d[%8x, %8x](%u)	", 
						num, segPoints[dim][num], segPoints[dim][num+1], hightList[num]);
			}
#endif /* DEBUG */
			free(hightList);
		} /* pos >=3 */

		if (maxDiffSegPts < pos) {
			maxDiffSegPts = pos;
		}
	}
	free(tempSegPoints);

	/*Update Leaf node*/
	if (maxDiffSegPts <= 2 || ruleset->num < bucketSize) {
		currNode->d2s = 0;
		currNode->depth = depth;
		currNode->thresh = (unsigned int) ruleset->ruleList[0].pri;
		currNode->child[0] = NULL;
		currNode->child[1] = NULL;
        currNode->rule_set = ruleset;
		
		for (dim = 0; dim < DIM; dim ++) {
			free(segPoints[dim]);
			free(segPointsInfo[dim]);
		}
#ifdef DEBUG
		printf("\n>>LEAF-NODE: matching rule %d", ruleset->ruleList[0].pri);
#endif /* DEBUG */
		
		gChildCount ++;
		gNumLeafNode ++;
        if(ruleset->num > 1) 
            gRulePointers += ruleset->num;

		if (gNumLeafNode % 1000000 == 0)
			printf(".");
			/*printf("\n>>#%8dM leaf-node generated", gNumLeafNode/1000000);*/
		if (gWstDepth < depth)
			gWstDepth = depth;

        if (ruleset->num + depth > gMaxLeafNum) {
            gMaxLeafNum = ruleset->num + depth;
            gMaxL_D = depth;
            gMaxL_N = ruleset->num;
        }

        if (ruleset->num > gMaxRulesinLeaf) {
            gMaxRulesinLeaf = ruleset->num;
        }

		gAvgDepth += depth;
		return	SUCCESS;
	}

	/*Update currNode*/	
	/*Binary split along d2s*/


#ifdef DEBUG
	/* split info */
	printf("\n>>d2s=%u; thresh=0x%8x, range0=[%8x, %8x], range1=[%8x, %8x]",
			d2s, thresh, range[0][0], range[0][1], range[1][0], range[1][1]);
#endif /* DEBUG */

	if (range[1][0] > range[1][1]) {
		printf("\n>>maxDiffSegPts=%d  range[1][0]=%x  range[1][1]=%x", 
				maxDiffSegPts, range[1][0], range[1][1]);
		printf("\n>>fuck\n"); exit(0);
	}


	for (dim = 0; dim < DIM; dim ++) {
		free(segPoints[dim]);
		free(segPointsInfo[dim]);
	}
	
	gNumTreeNode ++;
	currNode->d2s = (unsigned char) d2s;
	currNode->depth = (unsigned char) depth;
	currNode->thresh = thresh;
	currNode->child[0] = (hs_node_t *) malloc(sizeof(hs_node_t));

	/*Generate left child rule list*/
	tempRuleNumList = (unsigned int*) malloc(ruleset->num * sizeof(unsigned int)); /* need to be freed */
	pos = 0;
	for (num = 0; num < ruleset->num; num++) {
		if (ruleset->ruleList[num].range[d2s][0] <= range[0][1]
		&&	ruleset->ruleList[num].range[d2s][1] >= range[0][0]) {
			tempRuleNumList[pos] = num;
			pos++;
		}
	}
	childRuleSet = (rule_set_t*) malloc(sizeof(rule_set_t));
	childRuleSet->num = pos;
	childRuleSet->ruleList = (rule_t*) malloc( childRuleSet->num * sizeof(rule_t) );
	for (num = 0; num < childRuleSet->num; num++) {
		childRuleSet->ruleList[num] = ruleset->ruleList[tempRuleNumList[num]];
		/* in d2s dim, the search space needs to be trimmed off */
		if (childRuleSet->ruleList[num].range[d2s][0] < range[0][0])
			childRuleSet->ruleList[num].range[d2s][0] = range[0][0];
		if (childRuleSet->ruleList[num].range[d2s][1] > range[0][1])
			childRuleSet->ruleList[num].range[d2s][1] = range[0][1];
	}
	free(tempRuleNumList);
    remove_redund(&childRuleSet);
	BuildHSTree(childRuleSet, currNode->child[0], depth+1);
#ifndef	LOOKUP
	free(currNode->child[0]);
	free(childRuleSet->ruleList);
	free(childRuleSet);
#endif

	/*Generate right child rule list*/
	currNode->child[1] = (hs_node_t *) malloc(sizeof(hs_node_t));
	tempRuleNumList = (unsigned int*) malloc(ruleset->num * sizeof(unsigned int)); /* need to be free */
	pos = 0;
	for (num = 0; num < ruleset->num; num++) {
		if (ruleset->ruleList[num].range[d2s][0] <= range[1][1]
		&&	ruleset->ruleList[num].range[d2s][1] >= range[1][0]) {
			tempRuleNumList[pos] = num;
			pos++;
		}
	}

	childRuleSet = (rule_set_t*) malloc(sizeof(rule_set_t));
	childRuleSet->num = pos;
	childRuleSet->ruleList = (rule_t*) malloc( childRuleSet->num * sizeof(rule_t) );
	for (num = 0; num < childRuleSet->num; num++) {
		childRuleSet->ruleList[num] = ruleset->ruleList[tempRuleNumList[num]];
		/* in d2s dim, the search space needs to be trimmed off */
		if (childRuleSet->ruleList[num].range[d2s][0] < range[1][0])
			childRuleSet->ruleList[num].range[d2s][0] = range[1][0];
		if (childRuleSet->ruleList[num].range[d2s][1] > range[1][1])
			childRuleSet->ruleList[num].range[d2s][1] = range[1][1];
	}
	
	free(tempRuleNumList);
    remove_redund(&childRuleSet);
	BuildHSTree(childRuleSet, currNode->child[1], depth+1);
#ifndef	LOOKUP
	free(currNode->child[1]);
	free(childRuleSet->ruleList);
	free(childRuleSet);
#endif
		
	return	SUCCESS;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  LookupHSTtree
 *  Description:  test the hyper-split-tree with give 4-tuple packet
 * =====================================================================================
 */
int	LookupHSTree(rule_set_t* ruleset, hs_node_t* root)
{
	unsigned int	ruleNum;

	/*for (ruleNum = ruleset->num-1; ruleNum < ruleset->num; ruleNum ++) {*/
	for (ruleNum = 0; ruleNum < ruleset->num; ruleNum ++) {
		hs_node_t*	node = root;
		unsigned int	packet[DIM];
		packet[0] = ruleset->ruleList[ruleNum].range[0][0];
		packet[1] = ruleset->ruleList[ruleNum].range[1][0];
		packet[2] = ruleset->ruleList[ruleNum].range[2][0];
		packet[3] = ruleset->ruleList[ruleNum].range[3][0];
		packet[4] = ruleset->ruleList[ruleNum].range[4][0];
		while (node->child[0] != NULL) {
			if (packet[node->d2s] <= node->thresh)
				node = node->child[0];
			else
				node = node->child[1];
		}
		printf("\n>>LOOKUP RESULT");
		printf("\n>>packet:		[%8x %8x], [%8x %8x], [%5u %5u], [%5u %5u], [%2x %2x]",
				packet[0], packet[0],
				packet[1], packet[1],
				packet[2], packet[2],
				packet[3], packet[3],
				packet[4], packet[4]);
		printf("\n>>Expect Rule%d:	[%8x %8x], [%8x %8x], [%5u %5u], [%5u %5u], [%2x %2x]", ruleNum+1,
				ruleset->ruleList[ruleNum].range[0][0], ruleset->ruleList[ruleNum].range[0][1],
				ruleset->ruleList[ruleNum].range[1][0], ruleset->ruleList[ruleNum].range[1][1],
				ruleset->ruleList[ruleNum].range[2][0], ruleset->ruleList[ruleNum].range[2][1],
				ruleset->ruleList[ruleNum].range[3][0], ruleset->ruleList[ruleNum].range[3][1],
				ruleset->ruleList[ruleNum].range[4][0], ruleset->ruleList[ruleNum].range[4][1]);
		printf("\n>>Matched Rule%d:	[%8x %8x], [%8x %8x], [%5u %5u], [%5u %5u], [%2x %2x]", node->thresh+1,
				ruleset->ruleList[node->thresh].range[0][0], ruleset->ruleList[node->thresh].range[0][1],
				ruleset->ruleList[node->thresh].range[1][0], ruleset->ruleList[node->thresh].range[1][1],
				ruleset->ruleList[node->thresh].range[2][0], ruleset->ruleList[node->thresh].range[2][1],
				ruleset->ruleList[node->thresh].range[3][0], ruleset->ruleList[node->thresh].range[3][1],
				ruleset->ruleList[node->thresh].range[4][0], ruleset->ruleList[node->thresh].range[4][1]);

	}

	return	SUCCESS;
}
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  main
 *  Description:  yes, this is where we start.
 * =====================================================================================
 */

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




struct table_entry{
    uint32_t type;
    uint32_t dim;
    uint32_t mp;
    void* index; 
};

struct qn{
    hs_node_t *n;
    struct list_head list;
};

void push_back_node(hs_node_t *n, struct list_head *head)
{
    struct qn * qnew = (struct qn*)calloc(1, sizeof(*qnew));
    if(!qnew) {
        printf("no memory\n");
        exit(-1);
    }
    qnew->n = n;
    INIT_LIST_HEAD(&qnew->list);
    list_add_tail(&qnew->list, head);
}

hs_node_t *pop_node(struct list_head *head)
{
    struct list_head *h = head->next;
    struct qn * hqn = list_entry(h, struct qn, list);
    list_del(h);
    hs_node_t *r = hqn->n;
    free(hqn);
    return r; 
}

inline int g_linear_search(uint32_t *p, rule_set_t *ruleset)
{
    int i;
    int j;
    for(i=0;i<ruleset->num;i++) {
        if(p[0] >= ruleset->ruleList[i].range[0][0] 
                && p[0] <= ruleset->ruleList[i].range[0][1]
                && p[1] >= ruleset->ruleList[i].range[1][0]
                && p[1] <= ruleset->ruleList[i].range[1][1]
                && p[2] >= ruleset->ruleList[i].range[2][0]
                && p[2] <= ruleset->ruleList[i].range[2][1]
                && p[3] >= ruleset->ruleList[i].range[3][0]
                && p[3] <= ruleset->ruleList[i].range[3][1]
                && p[4] >= ruleset->ruleList[i].range[4][0]
                && p[4] <= ruleset->ruleList[i].range[4][1]
          ){
            return i;
        }
    }

    return -1;
}

inline int linear_search(struct table_entry *n, uint32_t *p, rule_set_t *ruleset)
{
    uint32_t *in = n->index;
    int i;
    int j;
    for(i=0;i<n->dim;i++) {
        if(p[0] >= ruleset->ruleList[in[i]].range[0][0] 
                && p[0] <= ruleset->ruleList[in[i]].range[0][1]
                && p[1] >= ruleset->ruleList[in[i]].range[1][0]
                && p[1] <= ruleset->ruleList[in[i]].range[1][1]
                && p[2] >= ruleset->ruleList[in[i]].range[2][0]
                && p[2] <= ruleset->ruleList[in[i]].range[2][1]
                && p[3] >= ruleset->ruleList[in[i]].range[3][0]
                && p[3] <= ruleset->ruleList[in[i]].range[3][1]
                && p[4] >= ruleset->ruleList[in[i]].range[4][0]
                && p[4] <= ruleset->ruleList[in[i]].range[4][1]
          ){
            return in[i];
        }
    }

    return -1;
}


struct dump_entry{
    uint32_t type;
    uint32_t dim;
    uint32_t mp;
    uint32_t next;
};

void dump_compact(struct table_entry* root, char *name) {
    FILE *fp = fopen(name, "w");
    if(!fp) {
        printf("fail to open a file\n");
        exit(-1);
    }
    unsigned int i = 0;
    struct dump_entry de;
    //fprintf(fp, "%u", gNumTreeNode + gNumLeafNode);
    uint32_t num = gNumTreeNode + gNumLeafNode;
    fwrite(&num, sizeof(uint32_t), 1 , fp);
    for(;i< gNumTreeNode + gNumLeafNode ;i++) {
        if((root+i)->type==0) {
            de.type = (root+i)->type;
            de.dim = (root+i)->dim;
            de.mp = (root+i)->mp;
            de.next = (struct table_entry*)((root+i)->index) - root;
        }
        else {
            de.type = 1;
            de.dim = (root+i)->dim;
            de.mp = 0;
            de.next = 0;
        }
        fwrite(&de, sizeof(de), 1, fp);

        if( de.type == 1 )
            fwrite((root+i)->index, (root+i)->dim, sizeof(uint32_t), fp);
    }
}

struct table_entry* build_compact(hs_node_t *rootnode)
{
    int cnt = 0;
    int fill_pos = 0;
    struct table_entry *root = (struct table_entry*)calloc(1, (gNumTreeNode+gNumLeafNode) *
            sizeof(*root));

    struct list_head hs_q;
    INIT_LIST_HEAD(&hs_q);

    push_back_node(rootnode, &hs_q);
    hs_node_t *curr_hn;

    while(!list_empty(&hs_q)) {
        curr_hn = pop_node(&hs_q);
        if(curr_hn->child[0] != NULL) {
            root[fill_pos].dim = curr_hn->d2s;
            root[fill_pos].index = root+cnt+1;
            root[fill_pos].mp = curr_hn->thresh; 
            push_back_node(curr_hn->child[0], &hs_q);
            push_back_node(curr_hn->child[1], &hs_q);
            cnt += 2;
            fill_pos += 1;
        }
        else {
            root[fill_pos].type = 1;
            root[fill_pos].dim = curr_hn->rule_set->num;
            root[fill_pos].index = 
            (uint32_t*)malloc(curr_hn->rule_set->num*
                    sizeof(uint32_t));
            uint32_t *p = root[fill_pos].index;
            int i;
            for(i=0;i<curr_hn->rule_set->num; i++) {
                p[i] = curr_hn->rule_set->ruleList[i].pri;
            }
            fill_pos ++;
        }
    }
    return root;

}


void lookup_table(struct table_entry *rootnode, rule_set_t *ruleset, FILE * fpt)
{
    uint32_t *ft = (uint32_t *)malloc(4000000 * DIM * sizeof(uint32_t));
    int i = 0;
#if 1 
    int ret1 = 0;
    int ret2 = 0;
#endif

    while(load_ft(fpt, ft+i*DIM)){
        i++;
    }
    printf("\n load rules %d\n", i);
    uint32_t trace_cnt = i;
   
    struct table_entry * node;

    struct timespec tp_a, tp_b;
    clock_gettime(CLOCK_MONOTONIC, &tp_b);
    for(i=i-1;i>=0;i--) {
        node = rootnode;
        while (node->type != 1) {
			if ((ft+i*DIM)[node->dim] <= node->mp)
				node = node->index;
			else
				node = (struct table_entry*)node->index + 1;
		}
#if 1
        ret1 = linear_search(node, ft + i * DIM, ruleset);
        ret2 = g_linear_search(ft + i * DIM,  ruleset);
        if( ret1 != ret2)
            printf("ret1 %d ret2 %d\n", ret1, ret2); 
#endif
#if 0
        linear_search(node, ft + i * DIM, ruleset);
#endif
    }
    clock_gettime(CLOCK_MONOTONIC, &tp_a);

    long total_time = 0;
    printf("\n%lu %lu\n", tp_a.tv_sec, tp_a.tv_nsec);
    printf("\n%lu %lu\n", tp_b.tv_sec, tp_b.tv_nsec);
    total_time = ((long)tp_a.tv_sec - (long)tp_b.tv_sec) * 1000000000ULL + 
            (tp_a.tv_nsec - tp_b.tv_nsec);

    printf("time: %ld\n", total_time); 
    double ave_time = (double)total_time / trace_cnt;
    printf("ave time: %.1fns\n", ave_time);
    double gbps = (1/ave_time) * 1e9 / 1.48e6;
    printf("gbps: %.1fGbps\n", gbps);  

}

void parseargs(int argc, char* argv[])
{
    int	c;
    int ok = 0;
    while ((c = getopt(argc, argv, "r:b:t:o:")) != -1) {
        switch (c) {
            case 'r':
                strncpy(filename, optarg, 128);
                ok = 1;
                break;
            case 'b':
                bucketSize = atoi(optarg);
                break;
            case 't':
                strncpy(tracename, optarg, 128);
                break;
            case 'o':
                strcpy(fibname, optarg);
            default:
                break;
        }
    }

    if(ok == 0) {
        printf("wrong args\n");
        exit(-1);
    }
    printf("filename :%s\n", filename);

}



int main(int argc, char* argv[])
{
	rule_set_t		ruleset;
	hs_node_t		rootnode;
	//char			filename[50] = "../hybrid/rules/fw2_2_0.5_-0.1_10K";	/* filter file name */
	//char			filename[50] = "../hybrid/rules/sw";	/* filter file name */
	//char			filename[50] = "../hybrid/filter2";	/* filter file name */
	//char			filename[50] = "sw";	/* filter file name */
	//char			filename[40] = "../supcutting/dr";
	//char			filename[40] = "../hybrid/filter2";	/* filter file name */

    parseargs(argc, argv);
	gettimeofday(&gStartTime,NULL);
	/* load rules from file */
	ReadFilterFile(&ruleset, filename);
	/* build hyper-split tree */
	printf("\n\n>>Building HyperSplit tree (%u rules, 5-tuple)", ruleset.num);
	BuildHSTree(&ruleset, &rootnode, 0);
#ifdef	LOOKUP
	//LookupHSTree(&ruleset, &rootnode);
    struct table_entry * root = build_compact(&rootnode);
    FILE *fpt;
    fpt = fopen("../hybrid/trace/fw2_2_0.5_-0.1_10K_1M","r");
    fpt = fopen(tracename,"r");
    //fpt = fopen("../hybrid/pc_trace","r");
    lookup_table(root, &ruleset, fpt); 
    dump_compact(root, fibname);
#endif
	gettimeofday(&gEndTime,NULL); 

	printf("\n\n>>RESULTS:");
	printf("\n>>number of children:		%d", gChildCount);
	printf("\n>>worst case tree depth:	%d", gWstDepth);
	printf("\n>>average tree depth:		%f", (float) gAvgDepth/gChildCount);
	printf("\n>>number of tree nodes:%d", gNumTreeNode);
	printf("\n>>number of leaf nodes:%d", gNumLeafNode);
	printf("\n>>number of leaf pointers:%d", gRulePointers);
	printf("\n>>total memory: %d(KB)", ((gNumTreeNode*8)>>10) + ((gNumLeafNode*4)>>10));
	printf("\n>>total leaf pointer memory: %d(KB)", (gRulePointers * 4 ) >> 10);
	printf("\n>>total HP memory: %d(KB)", ((gRulePointers*4)>>10) + ((gNumTreeNode*8)>>10) + ((gNumLeafNode*4)>>10));
	printf("\n>>preprocessing time: %ld(ms)", 1000*(gEndTime.tv_sec - gStartTime.tv_sec)
			+ (gEndTime.tv_usec - gStartTime.tv_usec)/1000);
	printf("\n\n>>SUCCESS in building HyperSplit tree :-)\n\n");
    printf("\n\n max memory access %d\n", gMaxLeafNum);
    printf("\n\n max memory access depth %d\n", gMaxL_D);
    printf("\n\n max memory access num %d\n", gMaxL_N);
    printf("\n\n max rules in leaf %d\n", gMaxRulesinLeaf);
	
	return SUCCESS;
}
#endif   /* ----- #ifndef _HS_C ----- */
