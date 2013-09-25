#include <uart_print.h>
#include "lpae.h"
#include "print.h"

/* Long-descriptor translation table format */
#define TTBL_L1_OUTADDR_MASK	0x000000FFEC000000ULL
#define TTBL_L2_OUTADDR_MASK	0x000000FFFFE00000ULL
#define TTBL_L3_OUTADDR_MASK	0x000000FFFFFFF000ULL

#define TTBL_L2_TABADDR_MASK	0x000000FFFFFFF000ULL

/* Level 2 Block, 2MB, entry in LPAE Descriptor format for the given physical address */
lpaed_t hvmm_mm_lpaed_l2_block( uint64_t pa, lpaed_stage2_memattr_t mattr )
{
    /* lpae.c */
	lpaed_t lpaed;

	// Valid Block Entry
	lpaed.pt.valid = 1;
	lpaed.pt.table = 0;

	lpaed.bits &= ~TTBL_L2_OUTADDR_MASK;
	lpaed.bits |= pa & TTBL_L2_OUTADDR_MASK;
	lpaed.p2m.sbz3 = 0;

	// Lower block attributes
	lpaed.p2m.mattr = mattr & 0x0F;	
	lpaed.p2m.read = 1;		// Read/Write
	lpaed.p2m.write = 1;		
	lpaed.p2m.sh = 0;	// Non-shareable
	lpaed.p2m.af = 1;	// Access Flag set to 1?
	lpaed.p2m.sbz4 = 0;

	// Upper block attributes
	lpaed.p2m.hint = 0;
	lpaed.p2m.sbz2 = 0;
	lpaed.p2m.xn = 0;	// eXecute Never = 0

	lpaed.p2m.sbz1 = 0;

	return lpaed;
}

/* Level 1 Block, 1GB, entry in LPAE Descriptor format for the given physical address */
lpaed_t hvmm_mm_lpaed_l1_block( uint64_t pa, uint8_t attr_idx )
{
    /* lpae.c */
	lpaed_t lpaed;

	printh( "[mm] hvmm_mm_lpaed_l1_block:\n\r" );
	printh( " pa:"); uart_print_hex64(pa); printh("\n\r");
	printh( " attr_idx:"); uart_print_hex32((uint32_t) attr_idx); printh("\n\r");

	// Valid Block Entry
	lpaed.pt.valid = 1;
	lpaed.pt.table = 0;

	lpaed.bits &= ~TTBL_L1_OUTADDR_MASK;
	lpaed.bits |= pa & TTBL_L1_OUTADDR_MASK;
	lpaed.pt.sbz = 0;

	// Lower block attributes
	lpaed.pt.ai = attr_idx;
	lpaed.pt.ns = 1;	// Allow Non-secure access
	lpaed.pt.user = 1;
	lpaed.pt.ro = 0;
	lpaed.pt.sh = 2;	// Outher Shareable
	lpaed.pt.af = 1;	// Access Flag set to 1?
	lpaed.pt.ng = 1;

	// Upper block attributes
	lpaed.pt.hint = 0;
	lpaed.pt.pxn = 0;
	lpaed.pt.xn = 0;	// eXecute Never = 0
	return lpaed;
}


void lpaed_stage2_conf_l2_table( lpaed_t *ttbl2, uint64_t baddr, uint8_t valid )
{
    ttbl2->pt.valid = valid ? 1 : 0;
    ttbl2->pt.table = valid ? 1 : 0;
    ttbl2->bits &= ~TTBL_L2_TABADDR_MASK;
    ttbl2->bits |= baddr & TTBL_L2_TABADDR_MASK;
}

void lpaed_stage2_enable_l2_table( lpaed_t *ttbl2 )
{
    ttbl2->pt.valid = 1;
    ttbl2->pt.table = 1;
}
void lpaed_stage2_disable_l2_table( lpaed_t *ttbl2 )
{
    ttbl2->pt.valid = 0;
}

void lpaed_stage2_map_page( lpaed_t *pte, uint64_t pa, lpaed_stage2_memattr_t mattr )
{

	pte->pt.valid = 1;
	pte->pt.table = 1;

	pte->bits &= ~TTBL_L3_OUTADDR_MASK;
	pte->bits |= pa & TTBL_L3_OUTADDR_MASK;
	pte->p2m.sbz3 = 0;

	// Lower block attributes
	pte->p2m.mattr = mattr & 0x0F;	
	pte->p2m.read = 1;		// Read/Write
	pte->p2m.write = 1;		
	pte->p2m.sh = 0;	// Non-shareable
	pte->p2m.af = 1;	// Access Flag set to 1?
	pte->p2m.sbz4 = 0;

	// Upper block attributes
	pte->p2m.hint = 0;
	pte->p2m.sbz2 = 0;
	pte->p2m.xn = 0;	// eXecute Never = 0

	pte->p2m.sbz1 = 0;
}