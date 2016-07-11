#include "pdsp_defs.h"

void
pxgstrf_pruneL(
	       const int  jcol,      /* current column */
	       const int  *perm_r,   /* row pivotings */
	       const int  pivrow,    /* pivot row of column jcol */
	       const int  nseg,      /* number of U-segments */
	       const int  *segrep,   /* in */
	       const int  *repfnz,   /* in */
	       int        *xprune,   /* modified */
	       int        *ispruned, /* modified */
	       GlobalLU_t *Glu /* modified - global LU data structures */
	       )
{
/*
 * -- SuperLU MT routine (version 1.0) --
 * Univ. of California Berkeley, Xerox Palo Alto Research Center,
 * and Lawrence Berkeley National Lab.
 * August 15, 1997
 *
 * Purpose
 * =======
 *   Reduces the L-structure of those supernodes whose L-structure
 *   contains the current pivot row "pivrow".
 *
 */
    register int jsupno, irep, isupno, irep1, kmin, kmax, krow;
    register int i, ktemp;
    register int do_prune; /* logical variable */
    int        *xsup, *xsup_end, *supno;
    int        *lsub, *xlsub, *xlsub_end;

    xsup       = Glu->xsup;
    xsup_end   = Glu->xsup_end;
    supno      = Glu->supno;
    lsub       = Glu->lsub;
    xlsub      = Glu->xlsub;
    xlsub_end  = Glu->xlsub_end;
    
    /*
     * For each supernode-rep irep in U[*,j]
     */
    jsupno = supno[jcol];
    for (i = 0; i < nseg; i++) {

	irep = segrep[i];
	irep1 = irep + 1;

	/* Don't prune with a zero U-segment */
 	if ( repfnz[irep] == EMPTY ) continue;

     	/* If a supernode overlaps with the next panel, then the U-segment 
   	 * is fragmented into two parts - irep and irep1. We should let
	 * pruning occur at the rep-column in irep1's supernode. 
	 */
	isupno = supno[irep];
	if ( isupno == supno[irep1] ) continue;	/* Don't prune */

	/*
	 * If it is not pruned & it has a nonz in row L[pivrow,i]
	 */
	do_prune = FALSE;
	if ( isupno != jsupno ) {
	    if ( ! ispruned[irep] ) {
		kmin = SINGLETON( isupno ) ? xlsub_end[irep] : xlsub[irep];
		kmax = xprune[irep] - 1;
		for (krow = kmin; krow <= kmax; krow++) 
		    if ( lsub[krow] == pivrow ) {
			do_prune = TRUE;
			break;
		    }
	    }
	    
    	    if ( do_prune ) {

	     	/* Do a quicksort-type partition */
	        while ( kmin <= kmax ) {
	    	    if ( perm_r[lsub[kmax]] == EMPTY ) 
			kmax--;
		    else if ( perm_r[lsub[kmin]] != EMPTY )
			kmin++;
		    else { /* kmin below pivrow, and kmax above pivrow: 
		            * 	interchange the two subscripts
			    */
		        ktemp = lsub[kmin];
		        lsub[kmin] = lsub[kmax];
		        lsub[kmax] = ktemp;
		        kmin++;
		        kmax--;
		    }
	        } /* while */

	        xprune[irep] = kmin;	/* Pruning */
		ispruned[irep] = 1;

#ifdef CHK_PRUNE
if (irep >= LOCOL && irep >= HICOL && jcol >= LOCOL && jcol <= HICOL)	
    printf("pxgstrf_pruneL() for irep %d using col %d: xprune %d - %d\n", 
	   irep, jcol, xlsub[irep], kmin);
#endif
	    } /* if do_prune */

	} /* if */

    } /* for each U-segment... */
}
