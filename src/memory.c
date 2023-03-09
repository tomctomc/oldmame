/***************************************************************************

  memory.c

  Functions which handle the CPU memory and I/O port access.

***************************************************************************/

#include "driver.h"

/*#define MEM_DUMP*/

/* Convenience macros - not in cpuintrf.h because they shouldn't be used by everyone */
#define MEMORY_READ(index,offset)       ((*cpuintf[Machine->drv->cpu[index].cpu_type & ~CPU_FLAGS_MASK].memory_read)(offset))
#define MEMORY_WRITE(index,offset,data) ((*cpuintf[Machine->drv->cpu[index].cpu_type & ~CPU_FLAGS_MASK].memory_write)(offset,data))
#define SET_OP_BASE(index,pc)           ((*cpuintf[Machine->drv->cpu[index].cpu_type & ~CPU_FLAGS_MASK].set_op_base)(pc))
#define ADDRESS_BITS(index)             (cpuintf[Machine->drv->cpu[index].cpu_type & ~CPU_FLAGS_MASK].address_bits)
#define ABITS1(index)                   (cpuintf[Machine->drv->cpu[index].cpu_type & ~CPU_FLAGS_MASK].abits1)
#define ABITS2(index)                   (cpuintf[Machine->drv->cpu[index].cpu_type & ~CPU_FLAGS_MASK].abits2)
#define ABITS3(index)                   (0)
#define ABITSMIN(index)                 (cpuintf[Machine->drv->cpu[index].cpu_type & ~CPU_FLAGS_MASK].abitsmin)

#if LSB_FIRST
	#define BYTE_XOR(a) ((a) ^ 1)
	#define BIG_DWORD(x) (((x) >> 16) + ((x) << 16))
#else
	#define BYTE_XOR(a) (a)
	#define BIG_DWORD(x) (x)
#endif

/* GSL 980224 Shift values for bytes within a word, used by the misaligned word load/store code */
#define SHIFT0 16
#define SHIFT1 24
#define SHIFT2 0
#define SHIFT3 8

unsigned char *OP_RAM;
unsigned char *OP_ROM;
MHELE ophw;				/* op-code hardware number */

struct ExtMemory ext_memory[MAX_EXT_MEMORY];

static unsigned char *ramptr[MAX_CPU], *romptr[MAX_CPU];

/* element shift bits, mask bits */
int mhshift[MAX_CPU][3], mhmask[MAX_CPU][3];

/* current hardware element map */
static MHELE *cur_mr_element[MAX_CPU];
static MHELE *cur_mw_element[MAX_CPU];

/* sub memory/port hardware element map */
static MHELE readhardware[MH_ELEMAX << MH_SBITS];  /* mem/port read  */
static MHELE writehardware[MH_ELEMAX << MH_SBITS]; /* mem/port write */

/* memory hardware element map */
/* value:                      */
#define HT_RAM    0		/* RAM direct        */
#define HT_BANK1  1		/* bank memory #1    */
#define HT_BANK2  2		/* bank memory #2    */
#define HT_BANK3  3		/* bank memory #3    */
#define HT_BANK4  4		/* bank memory #4    */
#define HT_BANK5  5		/* bank memory #5    */
#define HT_BANK6  6		/* bank memory #6    */
#define HT_BANK7  7		/* bank memory #7    */
#define HT_BANK8  8		/* bank memory #8    */
#define HT_NON    9		/* non mapped memory */
#define HT_NOP    10		/* NOP memory        */
#define HT_RAMROM 11		/* RAM ROM memory    */
#define HT_ROM    12		/* ROM memory        */

#define HT_USER   13		/* user functions    */
/* [MH_HARDMAX]-0xff	   link to sub memory element  */
/*                         (value-MH_HARDMAX)<<MH_SBITS -> element bank */

#define HT_BANKMAX (HT_BANK1 + MAX_BANKS - 1)

/* memory hardware handler */
static int (*memoryreadhandler[MH_HARDMAX])(int address);
static int memoryreadoffset[MH_HARDMAX];
static void (*memorywritehandler[MH_HARDMAX])(int address,int data);
static int memorywriteoffset[MH_HARDMAX];

/* bank ram base address */
unsigned char *cpu_bankbase[HT_BANKMAX+1];
static int bankreadoffset[HT_BANKMAX+1];
static int bankwriteoffset[HT_BANKMAX+1];

/* override OP base handler */
static int (*setOPbasefunc)(int);

/* current cpu current hardware element map point */
MHELE *cur_mrhard;
MHELE *cur_mwhard;


#ifdef macintosh
#include "macmemory.c"
#endif


/***************************************************************************

  Memory handling

***************************************************************************/
int mrh_ram(int address){return RAM[address];}
int mrh_bank1(int address){return cpu_bankbase[1][address];}
int mrh_bank2(int address){return cpu_bankbase[2][address];}
int mrh_bank3(int address){return cpu_bankbase[3][address];}
int mrh_bank4(int address){return cpu_bankbase[4][address];}
int mrh_bank5(int address){return cpu_bankbase[5][address];}
int mrh_bank6(int address){return cpu_bankbase[6][address];}
int mrh_bank7(int address){return cpu_bankbase[7][address];}
int mrh_bank8(int address){return cpu_bankbase[8][address];}

int mrh_error(int address)
{
	if (errorlog) fprintf(errorlog,"CPU #%d PC %04x: warning - read %02x from unmapped memory address %04x\n",cpu_getactivecpu(),cpu_getpc(),RAM[address],address);
	return RAM[address];
}
/* 24-bit address spaces are sparse, so we can't just return RAM[address] */
int mrh_error_sparse(int address)
{
	if (errorlog) fprintf(errorlog,"CPU #%d PC %06x: warning - read unmapped memory address %06x\n",cpu_getactivecpu(),cpu_getpc(),address);
	return 0;
}
int mrh_nop(int address)
{
	return 0;
}

void mwh_ram(int address,int data){RAM[address] = data;}
void mwh_bank1(int address,int data){cpu_bankbase[1][address] = data;}
void mwh_bank2(int address,int data){cpu_bankbase[2][address] = data;}
void mwh_bank3(int address,int data){cpu_bankbase[3][address] = data;}
void mwh_bank4(int address,int data){cpu_bankbase[4][address] = data;}
void mwh_bank5(int address,int data){cpu_bankbase[5][address] = data;}
void mwh_bank6(int address,int data){cpu_bankbase[6][address] = data;}
void mwh_bank7(int address,int data){cpu_bankbase[7][address] = data;}
void mwh_bank8(int address,int data){cpu_bankbase[8][address] = data;}

void mwh_error(int address,int data)
{
	if (errorlog) fprintf(errorlog,"CPU #%d PC %04x: warning - write %02x to unmapped memory address %04x\n",cpu_getactivecpu(),cpu_getpc(),data,address);
	RAM[address] = data;
}
/* 24-bit address spaces are sparse, so we can't just write to RAM[address] */
void mwh_error_sparse(int address,int data)
{
	if (errorlog) fprintf(errorlog,"CPU #%d PC %06x: warning - write %02x to unmapped memory address %06x\n",cpu_getactivecpu(),cpu_getpc(),data,address);
}
void mwh_rom(int address,int data)
{
	if (errorlog) fprintf(errorlog,"CPU #%d PC %04x: warning - write %02x to ROM address %04x\n",cpu_getactivecpu(),cpu_getpc(),data,address);
}
void mwh_ramrom(int address,int data)
{
	RAM[address] = ROM[address] = data;
}
void mwh_nop(int address,int data)
{
}


/* return element offset */
static MHELE *get_element( MHELE *element , int ad , int elemask ,
                        MHELE *subelement , int *ele_max )
{
	MHELE hw = element[ad];
	int i,ele;
	int banks = ( elemask / (1<<MH_SBITS) ) + 1;

	if( hw >= MH_HARDMAX ) return &subelement[(hw-MH_HARDMAX)<<MH_SBITS];

	/* create new element block */
	if( (*ele_max)+banks > MH_ELEMAX )
	{
		if (errorlog) fprintf(errorlog,"memory element size over \n");
		return 0;
	}
	/* get new element nunber */
	ele = *ele_max;
	(*ele_max)+=banks;
#ifdef MEM_DUMP
	if (errorlog) fprintf(errorlog,"create element %2d(%2d)\n",ele,banks);
#endif
	/* set link mark to current element */
	element[ad] = ele + MH_HARDMAX;
	/* get next subelement top */
	subelement  = &subelement[ele<<MH_SBITS];
	/* initialize new block */
	for( i = 0 ; i < (1<<MH_SBITS) ; i++ )
		subelement[i] = hw;

	return subelement;
}

static void set_element( int cpu , MHELE *celement , int sp , int ep , MHELE type , MHELE *subelement , int *ele_max )
{
	int i;
	int edepth = 0;
	int shift,mask;
	MHELE *eele = celement;
	MHELE *sele = celement;
	MHELE *ele;
	int ss,sb,eb,ee;

#ifdef MEM_DUMP
	if (errorlog) fprintf(errorlog,"set_element %6X-%6X = %2X\n",sp,ep,type);
#endif
	if( sp > ep ) return;
	do{
		mask  = mhmask[cpu][edepth];
		shift = mhshift[cpu][edepth];

		/* center element */
		ss = sp >> shift;
		sb = sp ? ((sp-1) >> shift) + 1 : 0;
		eb = ((ep+1) >> shift) - 1;
		ee = ep >> shift;

		if( sb <= eb )
		{
			if( (sb|mask)==(eb|mask) )
			{
				/* same reasion */
				ele = (sele ? sele : eele);
				for( i = sb ; i <= eb ; i++ ){
				 	ele[i & mask] = type;
				}
			}
			else
			{
				if( sele ) for( i = sb ; i <= (sb|mask) ; i++ )
				 	sele[i & mask] = type;
				if( eele ) for( i = eb&(~mask) ; i <= eb ; i++ )
				 	eele[i & mask] = type;
			}
		}

		edepth++;

		if( ss == sb ) sele = 0;
		else sele = get_element( sele , ss & mask , mhmask[cpu][edepth] ,
									subelement , ele_max );
		if( ee == eb ) eele = 0;
		else eele = get_element( eele , ee & mask , mhmask[cpu][edepth] ,
									subelement , ele_max );

	}while( sele || eele );
}


/* ASG 980121 -- allocate all the external memory */
static int memory_allocate_ext (void)
{
	struct ExtMemory *ext = ext_memory;
	int cpu;

	/* loop over all CPUs */
	for (cpu = 0; cpu < cpu_gettotalcpu (); cpu++)
	{
		const struct RomModule *romp = Machine->gamedrv->rom;
		const struct MemoryReadAddress *mra;
		const struct MemoryWriteAddress *mwa;

		int region = Machine->drv->cpu[cpu].memory_region;
		int curr = 0, size = 0;

		/* skip through the ROM regions to the matching one */
		while (romp->name || romp->offset || romp->length)
		{
			/* headers are all zeros except from the offset */
			if (!romp->name && !romp->length && !romp->checksum)
			{
				/* got a header; break if this is the match */
				if (curr++ == region)
				{
					size = romp->offset & ~ROMFLAG_MASK;
					break;
				}
			}
			romp++;
		}

		/* now it's time to loop */
		while (1)
		{
			int lowest = 0x7fffffff, end, lastend;

			/* find the base of the lowest memory region that extends past the end */
			for (mra = Machine->drv->cpu[cpu].memory_read; mra->start != -1; mra++)
				if (mra->end >= size && mra->start < lowest) lowest = mra->start;
			for (mwa = Machine->drv->cpu[cpu].memory_write; mwa->start != -1; mwa++)
				if (mwa->end >= size && mwa->start < lowest) lowest = mwa->start;

			/* done if nothing found */
			if (lowest == 0x7fffffff)
				break;

			/* now loop until we find the end of this contiguous block of memory */
			lastend = -1;
			end = lowest;
			while (end != lastend)
			{
				lastend = end;

				/* find the base of the lowest memory region that extends past the end */
				for (mra = Machine->drv->cpu[cpu].memory_read; mra->start != -1; mra++)
					if (mra->start <= end && mra->end > end) end = mra->end + 1;
				for (mwa = Machine->drv->cpu[cpu].memory_write; mwa->start != -1; mwa++)
					if (mwa->start <= end && mwa->end > end) end = mwa->end + 1;
			}

			/* time to allocate */
			ext->start = lowest;
			ext->end = end - 1;
			ext->region = region;
			ext->data = malloc (end - lowest);

			/* if that fails, we're through */
			if (!ext->data)
				return 0;

			/* reset the memory */
			memset (ext->data, 0, end - lowest);
			size = ext->end + 1;
			ext++;
		}
	}

	return 1;
}


unsigned char *memory_find_base (int cpu, int offset)
{
	int region = Machine->drv->cpu[cpu].memory_region;
	struct ExtMemory *ext;

	/* look in external memory first */
	for (ext = ext_memory; ext->data; ext++)
		if (ext->region == region && ext->start <= offset && ext->end >= offset)
			return ext->data + (offset - ext->start);

	return ramptr[cpu] + offset;
}


/* return = FALSE:can't allocate element memory */
int initmemoryhandlers(void)
{
	int i, cpu;
	const struct MemoryReadAddress *memoryread;
	const struct MemoryWriteAddress *memorywrite;
	const struct MemoryReadAddress *mra;
	const struct MemoryWriteAddress *mwa;
	int rdelement_max = 0;
	int wrelement_max = 0;
	int rdhard_max = HT_USER;
	int wrhard_max = HT_USER;
	MHELE hardware;
	int abits1,abits2,abits3,abitsmin;

	for( cpu = 0 ; cpu < MAX_CPU ; cpu++ )
		cur_mr_element[cpu] = cur_mw_element[cpu] = 0;

	/* ASG 980121 -- allocate external memory */
	if (!memory_allocate_ext ())
		return 0;
	setOPbasefunc = NULL;

	for( cpu = 0 ; cpu < cpu_gettotalcpu() ; cpu++ )
	{
		const struct MemoryReadAddress *mra;
		const struct MemoryWriteAddress *mwa;


		ramptr[cpu] = Machine->memory_region[Machine->drv->cpu[cpu].memory_region];

		/* opcode decryption is currently supported only for the first memory region */
		if (cpu == 0) romptr[cpu] = ROM;
		else romptr[cpu] = ramptr[cpu];


		/* initialize the memory base pointers for memory hooks */
		mra = Machine->drv->cpu[cpu].memory_read;
		while (mra->start != -1)
		{
			if (mra->base) *mra->base = memory_find_base (cpu, mra->start);
			if (mra->size) *mra->size = mra->end - mra->start + 1;
			mra++;
		}
		mwa = Machine->drv->cpu[cpu].memory_write;
		while (mwa->start != -1)
		{
			if (mwa->base) *mwa->base = memory_find_base (cpu, mwa->start);
			if (mwa->size) *mwa->size = mwa->end - mwa->start + 1;
			mwa++;
		}
	}

	/* initialize grobal handler */
	for( i = 0 ; i < MH_HARDMAX ; i++ ){
		memoryreadoffset[i] = 0;
		memorywriteoffset[i] = 0;
	}
	/* bank1 memory */
	memoryreadhandler[HT_BANK1] = mrh_bank1;
	memorywritehandler[HT_BANK1] = mwh_bank1;
	/* bank2 memory */
	memoryreadhandler[HT_BANK2] = mrh_bank2;
	memorywritehandler[HT_BANK2] = mwh_bank2;
	/* bank3 memory */
	memoryreadhandler[HT_BANK3] = mrh_bank3;
	memorywritehandler[HT_BANK3] = mwh_bank3;
	/* bank4 memory */
	memoryreadhandler[HT_BANK4] = mrh_bank4;
	memorywritehandler[HT_BANK4] = mwh_bank4;
	/* bank5 memory */
	memoryreadhandler[HT_BANK5] = mrh_bank5;
	memorywritehandler[HT_BANK5] = mwh_bank5;
	/* bank6 memory */
	memoryreadhandler[HT_BANK6] = mrh_bank6;
	memorywritehandler[HT_BANK6] = mwh_bank6;
	/* bank7 memory */
	memoryreadhandler[HT_BANK7] = mrh_bank7;
	memorywritehandler[HT_BANK7] = mwh_bank7;
	/* bank8 memory */
	memoryreadhandler[HT_BANK8] = mrh_bank8;
	memorywritehandler[HT_BANK8] = mwh_bank8;
	/* non map memory */
	memoryreadhandler[HT_NON] = mrh_error;
	memorywritehandler[HT_NON] = mwh_error;
	/* NOP memory */
	memoryreadhandler[HT_NOP] = mrh_nop;
	memorywritehandler[HT_NOP] = mwh_nop;
	/* RAMROM memory */
	memorywritehandler[HT_RAMROM] = mwh_ramrom;
	/* ROM memory */
	memorywritehandler[HT_ROM] = mwh_rom;

	/* if any CPU is 24-bit or more, we change the error handlers to be more benign */
	for (cpu = 0; cpu < cpu_gettotalcpu(); cpu++)
		if (ADDRESS_BITS (cpu) >= 24)
		{
			memoryreadhandler[HT_NON] = mrh_error_sparse;
			memorywritehandler[HT_NON] = mwh_error_sparse;
		}

	for( cpu = 0 ; cpu < cpu_gettotalcpu() ; cpu++ )
	{
		/* cpu selection */
		abits1 = ABITS1 (cpu);
		abits2 = ABITS2 (cpu);
		abits3 = ABITS3 (cpu);
		abitsmin = ABITSMIN (cpu);

		/* element shifter , mask set */
		mhshift[cpu][0] = (abits2+abits3);
		mhshift[cpu][1] = abits3;			/* 2nd */
		mhshift[cpu][2] = 0;				/* 3rd (used by set_element)*/
		mhmask[cpu][0]  = MHMASK(abits1);		/*1st(used by set_element)*/
		mhmask[cpu][1]  = MHMASK(abits2);		/*2nd*/
		mhmask[cpu][2]  = MHMASK(abits3);		/*3rd*/

		/* allocate current element */
		if( (cur_mr_element[cpu] = malloc(sizeof(MHELE)<<abits1)) == 0 )
		{
			shutdownmemoryhandler();
			return 0;
		}
		if( (cur_mw_element[cpu] = malloc(sizeof(MHELE)<<abits1)) == 0 )
		{
			shutdownmemoryhandler();
			return 0;
		}

		/* initialize curent element table */
		for( i = 0 ; i < (1<<abits1) ; i++ )
		{
			cur_mr_element[cpu][i] = HT_NON;	/* no map memory */
			cur_mw_element[cpu][i] = HT_NON;	/* no map memory */
		}

		memoryread = Machine->drv->cpu[cpu].memory_read;
		memorywrite = Machine->drv->cpu[cpu].memory_write;

		/* memory read handler build */
		mra = memoryread;
		while (mra->start != -1) mra++;
		mra--;

		while (mra >= memoryread)
		{
			int (*handler)(int) = mra->handler;

			if( handler == MRA_RAM ||
			    handler == MRA_ROM ) {
				hardware = HT_RAM;	/* sprcial case ram read */
			}
			else if( handler == MRA_BANK1 ) {
				hardware = HT_BANK1;
				memoryreadoffset[1] = bankreadoffset[1] = mra->start;
				cpu_bankbase[1] = memory_find_base (cpu, mra->start);
			}
			else if( handler == MRA_BANK2 ) {
				hardware = HT_BANK2;
				memoryreadoffset[2] = bankreadoffset[2] = mra->start;
				cpu_bankbase[2] = memory_find_base (cpu, mra->start);
			}
			else if( handler == MRA_BANK3 ) {
				hardware = HT_BANK3;
				memoryreadoffset[3] = bankreadoffset[3] = mra->start;
				cpu_bankbase[3] = memory_find_base (cpu, mra->start);
			}
			else if( handler == MRA_BANK4 ) {
				hardware = HT_BANK4;
				memoryreadoffset[4] = bankreadoffset[4] = mra->start;
				cpu_bankbase[4] = memory_find_base (cpu, mra->start);
			}
			else if( handler == MRA_BANK5 ) {
				hardware = HT_BANK5;
				memoryreadoffset[5] = bankreadoffset[5] = mra->start;
				cpu_bankbase[5] = memory_find_base (cpu, mra->start);
			}
			else if( handler == MRA_BANK6 ) {
				hardware = HT_BANK6;
				memoryreadoffset[6] = bankreadoffset[6] = mra->start;
				cpu_bankbase[6] = memory_find_base (cpu, mra->start);
			}
			else if( handler == MRA_BANK7 ) {
				hardware = HT_BANK7;
				memoryreadoffset[7] = bankreadoffset[7] = mra->start;
				cpu_bankbase[7] = memory_find_base (cpu, mra->start);
			}
			else if( handler == MRA_BANK8 ) {
				hardware = HT_BANK8;
				memoryreadoffset[8] = bankreadoffset[8] = mra->start;
				cpu_bankbase[8] = memory_find_base (cpu, mra->start);
			}
			else if( handler == MRA_NOP ) {
				hardware = HT_NOP;
			}
			else {
				/* create newer haraware handler */
				if( rdhard_max == MH_HARDMAX )
				{
					if (errorlog)
					 fprintf(errorlog,"read memory hardware pattern over !\n");
					hardware = 0;
				}
				else
				{
					/* regist hardware function */
					hardware = rdhard_max++;
					memoryreadhandler[hardware] = handler;
					memoryreadoffset[hardware] = mra->start;
				}
			}
			/* hardware element table make */
			set_element( cpu , cur_mr_element[cpu] ,
				mra->start >> abitsmin , mra->end >> abitsmin ,
				hardware , readhardware , &rdelement_max );
			mra--;
		}

		/* memory write handler build */
		mwa = memorywrite;
		while (mwa->start != -1) mwa++;
		mwa--;

		while (mwa >= memorywrite)
		{
			void (*handler)(int,int) = mwa->handler;
			if( handler == MWA_RAM ) {
				hardware = HT_RAM;	/* sprcial case ram write */
			}
			else if( handler == MWA_BANK1 ) {
				hardware = HT_BANK1;
				memorywriteoffset[1] = bankwriteoffset[1] = mwa->start;
				cpu_bankbase[1] = memory_find_base (cpu, mwa->start);
			}
			else if( handler == MWA_BANK2 ) {
				hardware = HT_BANK2;
				memorywriteoffset[2] = bankwriteoffset[2] = mwa->start;
				cpu_bankbase[2] = memory_find_base (cpu, mwa->start);
			}
			else if( handler == MWA_BANK3 ) {
				hardware = HT_BANK3;
				memorywriteoffset[3] = bankwriteoffset[3] = mwa->start;
				cpu_bankbase[3] = memory_find_base (cpu, mwa->start);
			}
			else if( handler == MWA_BANK4 ) {
				hardware = HT_BANK4;
				memorywriteoffset[4] = bankwriteoffset[4] = mwa->start;
				cpu_bankbase[4] = memory_find_base (cpu, mwa->start);
			}
			else if( handler == MWA_BANK5 ) {
				hardware = HT_BANK5;
				memorywriteoffset[5] = bankwriteoffset[5] = mwa->start;
				cpu_bankbase[5] = memory_find_base (cpu, mwa->start);
			}
			else if( handler == MWA_BANK6 ) {
				hardware = HT_BANK6;
				memorywriteoffset[6] = bankwriteoffset[6] = mwa->start;
				cpu_bankbase[6] = memory_find_base (cpu, mwa->start);
			}
			else if( handler == MWA_BANK7 ) {
				hardware = HT_BANK7;
				memorywriteoffset[7] = bankwriteoffset[7] = mwa->start;
				cpu_bankbase[7] = memory_find_base (cpu, mwa->start);
			}
			else if( handler == MWA_BANK8 ) {
				hardware = HT_BANK8;
				memorywriteoffset[8] = bankwriteoffset[8] = mwa->start;
				cpu_bankbase[8] = memory_find_base (cpu, mwa->start);
			}
			else if( handler == MWA_NOP ) {
				hardware = HT_NOP;
			}
			else if( handler == MWA_RAMROM ) {
				hardware = HT_RAMROM;
			}
			else if( handler == MWA_ROM ) {
				hardware = HT_ROM;
			}
			else {
				/* create newer haraware handler */
				if( wrhard_max == MH_HARDMAX ){
					if (errorlog)
					 fprintf(errorlog,"write memory hardware pattern over !\n");
					hardware = 0;
				}else{
					/* regist hardware function */
					hardware = wrhard_max++;
					memorywritehandler[hardware] = handler;
					memorywriteoffset[hardware]  = mwa->start;
				}
			}
			/* hardware element table make */
			set_element( cpu , cur_mw_element[cpu] ,
				mwa->start >> abitsmin , mwa->end >> abitsmin ,
				hardware , (MHELE *)writehardware , &wrelement_max );
			mwa--;
		}
	}

	if (errorlog){
		fprintf(errorlog,"used read  elements %d/%d , functions %d/%d\n"
		    ,rdelement_max,MH_ELEMAX , rdhard_max,MH_HARDMAX );
		fprintf(errorlog,"used write elements %d/%d , functions %d/%d\n"
		    ,wrelement_max,MH_ELEMAX , wrhard_max,MH_HARDMAX );
	}
#ifdef MEM_DUMP
	mem_dump();
#endif
	return 1;	/* ok */
}

void memorycontextswap(int activecpu)
{
	RAM = cpu_bankbase[0] = ramptr[activecpu];
	ROM = romptr[activecpu];

	cur_mrhard = cur_mr_element[activecpu];
	cur_mwhard = cur_mw_element[activecpu];

	/* op code memory pointer */
	ophw = HT_RAM;
	OP_RAM = RAM;
	OP_ROM = ROM;
}

void shutdownmemoryhandler(void)
{
	struct ExtMemory *ext;
	int cpu;

	for( cpu = 0 ; cpu < MAX_CPU ; cpu++ )
	{
		if( cur_mr_element[cpu] != 0 )
		{
			free( cur_mr_element[cpu] );
			cur_mr_element[cpu] = 0;
		}
		if( cur_mw_element[cpu] != 0 )
		{
			free( cur_mw_element[cpu] );
			cur_mw_element[cpu] = 0;
		}
	}

	/* ASG 980121 -- free all the external memory */
	for (ext = ext_memory; ext->data; ext++)
		free (ext->data);
	memset (ext_memory, 0, sizeof (ext_memory));
}

void updatememorybase(int activecpu)
{
	/* keep track of changes to RAM and ROM pointers (bank switching) */
	ramptr[activecpu] = RAM;
	romptr[activecpu] = ROM;
}



/***************************************************************************

  Perform a memory read. This function is called by the CPU emulation.

***************************************************************************/

int cpu_readmem16 (int address)
{
	MHELE hw;

	/* 1st element link */
	hw = cur_mrhard[address >> (ABITS2_16 + ABITS_MIN_16)];
	if (!hw) return RAM[address];
	if (hw >= MH_HARDMAX)
	{
		/* 2nd element link */
		hw = readhardware[((hw - MH_HARDMAX) << MH_SBITS) + ((address >> ABITS_MIN_16) & MHMASK(ABITS2_16))];
		if (!hw) return RAM[address];
	}

	/* fallback to handler */
	return memoryreadhandler[hw](address - memoryreadoffset[hw]);
}


int cpu_readmem20 (int address)
{
	MHELE hw;

	/* 1st element link */
	hw = cur_mrhard[address >> (ABITS2_20 + ABITS_MIN_20)];
	if (!hw) return RAM[address];
	if (hw >= MH_HARDMAX)
	{
		/* 2nd element link */
		hw = readhardware[((hw - MH_HARDMAX) << MH_SBITS) + ((address >> ABITS_MIN_20) & MHMASK(ABITS2_20))];
		if (!hw) return RAM[address];
	}

	/* fallback to handler */
	return memoryreadhandler[hw](address - memoryreadoffset[hw]);
}


int cpu_readmem24 (int address)
{
	int shift, word;
	MHELE hw;

	/* 1st element link */
	hw = cur_mrhard[address >> (ABITS2_24 + ABITS_MIN_24)];
	if (hw <= HT_BANKMAX) return cpu_bankbase[hw][BYTE_XOR (address - memoryreadoffset[hw])];
	if (hw >= MH_HARDMAX)
	{
		/* 2nd element link */
		hw = readhardware[((hw - MH_HARDMAX) << MH_SBITS) + ((address >> ABITS_MIN_24) & MHMASK(ABITS2_24))];
		if (hw <= HT_BANKMAX) return cpu_bankbase[hw][BYTE_XOR (address - memoryreadoffset[hw])];
	}

	/* fallback to handler */
	shift = ((address ^ 1) & 1) << 3;
	address &= ~1;
	word = memoryreadhandler[hw](address - memoryreadoffset[hw]);
	return (word >> shift) & 0xff;
}


int cpu_readmem24_word (int address)
{
	MHELE hw;

	/* 1st element link */
	hw = cur_mrhard[address >> (ABITS2_24 + ABITS_MIN_24)];
	if (hw <= HT_BANKMAX) return READ_WORD (&cpu_bankbase[hw][address - memoryreadoffset[hw]]);
	if (hw >= MH_HARDMAX)
	{
		/* 2nd element link */
		hw = readhardware[((hw - MH_HARDMAX) << MH_SBITS) + ((address >> ABITS_MIN_24) & MHMASK(ABITS2_24))];
		if (hw <= HT_BANKMAX) return READ_WORD (&cpu_bankbase[hw][address - memoryreadoffset[hw]]);
	}

	/* fallback to handler */
	return memoryreadhandler[hw](address - memoryreadoffset[hw]);
}


int cpu_readmem24_dword (int address)
{
	unsigned long val;
	MHELE hw;

	/* 1st element link */
	hw = cur_mrhard[address >> (ABITS2_24 + ABITS_MIN_24)];
	if (hw <= HT_BANKMAX)
	{
	  	#ifdef ACORN /* GSL 980224 misaligned dword load case */
		if (address & 3)
		{
			char *addressbase = (char *)&cpu_bankbase[hw][address - memoryreadoffset[hw]];
			return ((*addressbase)<<SHIFT0) + (*(addressbase+1)<<SHIFT1) + (*(addressbase+2)<<SHIFT2) + (*(addressbase+3)<<SHIFT3);
		}
		else
		{
			val = *(unsigned long *)&cpu_bankbase[hw][address - memoryreadoffset[hw]];
			return BIG_DWORD(val);
		}

		#else

		val = *(unsigned long *)&cpu_bankbase[hw][address - memoryreadoffset[hw]];
		return BIG_DWORD(val);
		#endif

	}
	if (hw >= MH_HARDMAX)
	{
		/* 2nd element link */
		hw = readhardware[((hw - MH_HARDMAX) << MH_SBITS) + ((address >> ABITS_MIN_24) & MHMASK(ABITS2_24))];
		if (hw <= HT_BANKMAX)
		{
		  	#ifdef ACORN
			if (address & 3)
			{
				char *addressbase = (char *)&cpu_bankbase[hw][address - memoryreadoffset[hw]];
				return ((*addressbase)<<SHIFT0) + (*(addressbase+1)<<SHIFT1) + (*(addressbase+2)<<SHIFT2) + (*(addressbase+3)<<SHIFT3);
			}
			else
			{
				val = *(unsigned long *)&cpu_bankbase[hw][address - memoryreadoffset[hw]];
				return BIG_DWORD(val);
			}

			#else

			val = *(unsigned long *)&cpu_bankbase[hw][address - memoryreadoffset[hw]];
			return BIG_DWORD(val);
			#endif

		}
	}

	/* fallback to handler */
	address -= memoryreadoffset[hw];
	return (memoryreadhandler[hw](address) << 16) + (memoryreadhandler[hw](address + 2) & 0xffff);
}


/***************************************************************************

  Perform a memory write. This function is called by the CPU emulation.

***************************************************************************/

void cpu_writemem16 (int address, int data)
{
	MHELE hw;

	/* 1st element link */
	hw = cur_mwhard[address >> (ABITS2_16 + ABITS_MIN_16)];
	if (!hw)
	{
		RAM[address] = data;
		return;
	}
	if (hw >= MH_HARDMAX)
	{
		/* 2nd element link */
		hw = writehardware[((hw - MH_HARDMAX) << MH_SBITS) + ((address >> ABITS_MIN_16) & MHMASK(ABITS2_16))];
		if (!hw)
		{
			RAM[address] = data;
			return;
		}
	}

	/* fallback to handler */
	memorywritehandler[hw](address - memorywriteoffset[hw], data);
}


void cpu_writemem20 (int address, int data)
{
	MHELE hw;

	/* 1st element link */
	hw = cur_mwhard[address >> (ABITS2_20 + ABITS_MIN_20)];
	if (!hw)
	{
		RAM[address] = data;
		return;
	}
	if ( hw >= MH_HARDMAX)
	{
		/* 2nd element link */
		hw = writehardware[((hw - MH_HARDMAX) << MH_SBITS) + ((address >> ABITS_MIN_20) & MHMASK(ABITS2_20))];
		if (!hw)
		{
			RAM[address] = data;
			return;
		}
	}

	/* fallback to handler */
	memorywritehandler[hw](address - memorywriteoffset[hw],data);
}


void cpu_writemem24 (int address, int data)
{
	int shift;
	MHELE hw;

	/* 1st element link */
	hw = cur_mwhard[address >> (ABITS2_24 + ABITS_MIN_24)];
	if (hw <= HT_BANKMAX)
	{
		cpu_bankbase[hw][BYTE_XOR (address - memorywriteoffset[hw])] = data;
		return;
	}
	if (hw >= MH_HARDMAX)
	{
		/* 2nd element link */
		hw = writehardware[((hw - MH_HARDMAX) << MH_SBITS) + ((address >> ABITS_MIN_24) & MHMASK(ABITS2_24))];
		if (hw <= HT_BANKMAX)
		{
			cpu_bankbase[hw][BYTE_XOR (address - memorywriteoffset[hw])] = data;
			return;
		}
	}

	/* fallback to handler */
	shift = ((address ^ 1) & 1) << 3;
	address &= ~1;
	memorywritehandler[hw](address - memorywriteoffset[hw], (0xff000000 >> shift) | ((data & 0xff) << shift));
}


void cpu_writemem24_word (int address, int data)
{
	MHELE hw;

	/* 1st element link */
	hw = cur_mwhard[address >> (ABITS2_24 + ABITS_MIN_24)];
	if (hw <= HT_BANKMAX)
	{
		WRITE_WORD (&cpu_bankbase[hw][address - memorywriteoffset[hw]], data);
		return;
	}
	if (hw >= MH_HARDMAX)
	{
		/* 2nd element link */
		hw = writehardware[((hw - MH_HARDMAX) << MH_SBITS) + ((address >> ABITS_MIN_24) & MHMASK(ABITS2_24))];
		if (hw <= HT_BANKMAX)
		{
			WRITE_WORD (&cpu_bankbase[hw][address - memorywriteoffset[hw]], data);
			return;
		}
	}

	/* fallback to handler */
	memorywritehandler[hw](address - memorywriteoffset[hw], data & 0xffff);
}


void cpu_writemem24_dword (int address, int data)
{
	MHELE hw;

	/* 1st element link */
	hw = cur_mwhard[address >> (ABITS2_24 + ABITS_MIN_24)];
	if (hw <= HT_BANKMAX)
	{
	  	#ifdef ACORN /* GSL 980224 misaligned dword store case */
		if (address & 3)
		{
			char *addressbase = (char *)&cpu_bankbase[hw][address - memorywriteoffset[hw]];
			*addressbase = data >> SHIFT0;
			*(addressbase+1) = data >> SHIFT1;
			*(addressbase+2) = data >> SHIFT2;
			*(addressbase+3) = data >> SHIFT3;
			return;
		}
		else
		{
			data = BIG_DWORD ((unsigned long)data);
			*(unsigned long *)&cpu_bankbase[hw][address - memorywriteoffset[hw]] = data;
			return;
		}

		#else

		data = BIG_DWORD ((unsigned long)data);
		*(unsigned long *)&cpu_bankbase[hw][address - memorywriteoffset[hw]] = data;
		return;
		#endif

	}
	if (hw >= MH_HARDMAX)
	{
		/* 2nd element link */
		hw = writehardware[((hw - MH_HARDMAX) << MH_SBITS) + ((address >> ABITS_MIN_24) & MHMASK(ABITS2_24))];
		if (hw <= HT_BANKMAX)
		{
		  	#ifdef ACORN
			if (address & 3)
			{
				char *addressbase = (char *)&cpu_bankbase[hw][address - memorywriteoffset[hw]];
				*addressbase = data >> SHIFT0;
				*(addressbase+1) = data >> SHIFT1;
				*(addressbase+2) = data >> SHIFT2;
				*(addressbase+3) = data >> SHIFT3;
				return;
			}
			else
			{
				data = BIG_DWORD ((unsigned long)data);
				*(unsigned long *)&cpu_bankbase[hw][address - memorywriteoffset[hw]] = data;
				return;
			}

			#else

			data = BIG_DWORD ((unsigned long)data);
			*(unsigned long *)&cpu_bankbase[hw][address - memorywriteoffset[hw]] = data;
			return;
			#endif

		}
	}

	/* fallback to handler */
	address -= memorywriteoffset[hw];
	memorywritehandler[hw](address, (data >> 16) & 0xffff);
	memorywritehandler[hw](address + 2, data & 0xffff);
}


/***************************************************************************

  Perform an I/O port read. This function is called by the CPU emulation.

***************************************************************************/
int cpu_readport16(int Port)
{
	const struct IOReadPort *iorp;


	iorp = Machine->drv->cpu[cpu_getactivecpu()].port_read;
	if (iorp)
	{
		while (iorp->start != -1)
		{
			if (Port >= iorp->start && Port <= iorp->end)
			{
				int (*handler)(int) = iorp->handler;


				if (handler == IORP_NOP) return 0;
				else return (*handler)(Port - iorp->start);
			}

			iorp++;
		}
	}

	if (errorlog) fprintf(errorlog,"CPU #%d PC %04x: warning - read unmapped I/O port %02x\n",cpu_getactivecpu(),cpu_getpc(),Port);
	return 0;
}

int cpu_readport(int Port)
{
	/* if Z80 and it doesn't support 16-bit addressing, clip to the low 8 bits */
	if ((Machine->drv->cpu[cpu_getactivecpu()].cpu_type & ~CPU_FLAGS_MASK) == CPU_Z80 &&
			(Machine->drv->cpu[cpu_getactivecpu()].cpu_type & CPU_16BIT_PORT) == 0)
		return cpu_readport16(Port & 0xff);
	else return cpu_readport16(Port);
}


/***************************************************************************

  Perform an I/O port write. This function is called by the CPU emulation.

***************************************************************************/
void cpu_writeport16(int Port,int Value)
{
	const struct IOWritePort *iowp;


	iowp = Machine->drv->cpu[cpu_getactivecpu()].port_write;
	if (iowp)
	{
		while (iowp->start != -1)
		{
			if (Port >= iowp->start && Port <= iowp->end)
			{
				void (*handler)(int,int) = iowp->handler;


				if (handler == IOWP_NOP) return;
				else (*handler)(Port - iowp->start,Value);

				return;
			}

			iowp++;
		}
	}

	if (errorlog) fprintf(errorlog,"CPU #%d PC %04x: warning - write %02x to unmapped I/O port %02x\n",cpu_getactivecpu(),cpu_getpc(),Value,Port);
}

void cpu_writeport(int Port,int Value)
{
	/* if Z80 and it doesn't support 16-bit addressing, clip to the low 8 bits */
	if ((Machine->drv->cpu[cpu_getactivecpu()].cpu_type & ~CPU_FLAGS_MASK) == CPU_Z80 &&
			(Machine->drv->cpu[cpu_getactivecpu()].cpu_type & CPU_16BIT_PORT) == 0)
		cpu_writeport16(Port & 0xff,Value);
	else cpu_writeport16(Port,Value);
}



/* set readmemory handler for bank memory  */
void cpu_setbankhandler_r(int bank,int (*handler)(int) )
{
	int offset = 0;

	if( handler == MRA_RAM ||
		handler == MRA_ROM ) {
		handler = mrh_ram;
	}
	else if( handler == MRA_BANK1 ) {
		handler = mrh_bank1;
		offset = bankreadoffset[1];
	}
	else if( handler == MRA_BANK2 ) {
		handler = mrh_bank2;
		offset = bankreadoffset[2];
	}
	else if( handler == MRA_BANK3 ) {
		handler = mrh_bank3;
		offset = bankreadoffset[3];
	}
	else if( handler == MRA_BANK4 ) {
		handler = mrh_bank4;
		offset = bankreadoffset[4];
	}
	else if( handler == MRA_BANK5 ) {
		handler = mrh_bank5;
		offset = bankreadoffset[5];
	}
	else if( handler == MRA_BANK6 ) {
		handler = mrh_bank6;
		offset = bankreadoffset[6];
	}
	else if( handler == MRA_BANK7 ) {
		handler = mrh_bank7;
		offset = bankreadoffset[7];
	}
	else if( handler == MRA_BANK8 ) {
		handler = mrh_bank8;
		offset = bankreadoffset[8];
	}
	else if( handler == MRA_NOP ) {
		handler = mrh_nop;
	}
	else {
		offset = bankreadoffset[bank];
	}
	memoryreadoffset[bank] = offset;
	memoryreadhandler[bank] = handler;
}

/* set writememory handler for bank memory  */
void cpu_setbankhandler_w(int bank,void (*handler)(int,int) )
{
	int offset = 0;

	if( handler == MWA_RAM ) {
		handler = mwh_ram;
	}
	else if( handler == MWA_BANK1 ) {
		handler = mwh_bank1;
		offset = bankwriteoffset[1];
	}
	else if( handler == MWA_BANK2 ) {
		handler = mwh_bank2;
		offset = bankwriteoffset[2];
	}
	else if( handler == MWA_BANK3 ) {
		handler = mwh_bank3;
		offset = bankwriteoffset[3];
	}
	else if( handler == MWA_BANK4 ) {
		handler = mwh_bank4;
		offset = bankwriteoffset[4];
	}
	else if( handler == MWA_BANK5 ) {
		handler = mwh_bank5;
		offset = bankwriteoffset[5];
	}
	else if( handler == MWA_BANK6 ) {
		handler = mwh_bank6;
		offset = bankwriteoffset[6];
	}
	else if( handler == MWA_BANK7 ) {
		handler = mwh_bank7;
		offset = bankwriteoffset[7];
	}
	else if( handler == MWA_BANK8 ) {
		handler = mwh_bank8;
		offset = bankwriteoffset[8];
	}
	else if( handler == MWA_NOP ) {
		handler = mwh_nop;
	}
	else if( handler == MWA_RAMROM ) {
		handler = mwh_ramrom;
	}
	else if( handler == MWA_ROM ) {
		handler = mwh_rom;
	}
	else {
		offset = bankwriteoffset[bank];
	}
	memorywriteoffset[bank] = offset;
	memorywritehandler[bank] = handler;
}

/* cpu change op-code memory base */
void cpu_setOPbaseoverride (int (*f)(int))
{
	setOPbasefunc = f;
}


/* Need to called after CPU or PC changed (JP,JR,BRA,CALL,RET) */
void cpu_setOPbase16 (int pc)
{
	MHELE hw;

	/* ASG 970206 -- allow overrides */
	if (setOPbasefunc)
	{
		pc = setOPbasefunc (pc);
		if (pc == -1)
			return;
	}

	/* 1st element link */
	hw = cur_mrhard[pc >> (ABITS2_16 + ABITS_MIN_16)];
	if (hw >= MH_HARDMAX)
	{
		/* 2nd element link */
		hw = readhardware[((hw - MH_HARDMAX) << MH_SBITS) + ((pc >> ABITS_MIN_16) & MHMASK(ABITS2_16))];
	}
	ophw = hw;

	if (!hw)
	{
	 /* memory direct */
		OP_RAM = RAM;
		OP_ROM = ROM;
		return;
	}

	if (hw <= HT_BANKMAX)
	{
		/* banked memory select */
		OP_RAM = cpu_bankbase[hw] - memoryreadoffset[hw];
		if (RAM == ROM) OP_ROM = OP_RAM;
		return;
	}

	/* do not support on callbank memory reasion */
	if (errorlog) fprintf(errorlog,"CPU #%d PC %04x: warning - op-code execute on mapped i/o\n",cpu_getactivecpu(),cpu_getpc());
}


void cpu_setOPbase20 (int pc)
{
	MHELE hw;

	/* ASG 970206 -- allow overrides */
	if (setOPbasefunc)
	{
		pc = setOPbasefunc (pc);
		if (pc == -1)
			return;
	}

	/* 1st element link */
	hw = cur_mrhard[pc >> (ABITS2_20 + ABITS_MIN_20)];
	if (hw >= MH_HARDMAX)
	{
		/* 2nd element link */
		hw = readhardware[((hw - MH_HARDMAX) << MH_SBITS) + ((pc >> ABITS_MIN_20) & MHMASK (ABITS2_20))];
	}
	ophw = hw;

	if (!hw)
	{
		/* memory direct */
		OP_RAM = RAM;
		OP_ROM = ROM;
		return;
	}

	if (hw <= HT_BANKMAX)
	{
		/* banked memory select */
		OP_RAM = cpu_bankbase[hw] - memoryreadoffset[hw];
		if (RAM == ROM) OP_ROM = OP_RAM;
		return;
	}

	/* do not support on callbank memory reasion */
	if (errorlog) fprintf(errorlog,"CPU #%d PC %04x: warning - op-code execute on mapped i/o\n",cpu_getactivecpu(),cpu_getpc());
}


void cpu_setOPbase24 (int pc)
{
	MHELE hw;

	/* ASG 970206 -- allow overrides */
	if (setOPbasefunc)
	{
		pc = setOPbasefunc (pc);
		if (pc == -1)
			return;
	}

	/* 1st element link */
	hw = cur_mrhard[pc >> (ABITS2_24 + ABITS_MIN_24)];
	if (hw >= MH_HARDMAX)
	{
		/* 2nd element link */
		hw = readhardware[((hw - MH_HARDMAX) << MH_SBITS) + ((pc >> ABITS_MIN_24) & MHMASK(ABITS2_24))];
	}
	ophw = hw;

	if (!hw)
	{
		/* memory direct */
		OP_RAM = RAM;
		OP_ROM = ROM;
		return;
	}

	if (hw <= HT_BANKMAX)
	{
		/* banked memory select */
		OP_RAM = cpu_bankbase[hw] - memoryreadoffset[hw];
		if (RAM == ROM) OP_ROM = OP_RAM;
		return;
	}

	/* do not support on callbank memory reasion */
	if (errorlog) fprintf(errorlog,"CPU #%d PC %04x: warning - op-code execute on mapped i/o\n",cpu_getactivecpu(),cpu_getpc());
}



#ifdef MEM_DUMP
static void mem_dump( void )
{
	extern int totalcpu;
	int cpu;
	int naddr,addr;
	MHELE nhw,hw;

	FILE *temp = fopen ("memdump.log", "w");

	if (!temp) return;

	for( cpu = 0 ; cpu < 1 ; cpu++ )
	{
		fprintf(temp,"cpu %d read memory \n",cpu);
		addr = 0;
		naddr = 0;
		nhw = 0xff;
		while( (addr >> mhshift[cpu][0]) <= mhmask[cpu][0] ){
			hw = cur_mr_element[cpu][addr >> mhshift[cpu][0]];
			if( hw >= MH_HARDMAX )
			{	/* 2nd element link */
				hw = readhardware[((hw-MH_HARDMAX)<<MH_SBITS) + ((addr>>mhshift[cpu][1]) & mhmask[cpu][1])];
				if( hw >= MH_HARDMAX )
					hw = readhardware[((hw-MH_HARDMAX)<<MH_SBITS) + (addr & mhmask[cpu][2])];
			}
			if( nhw != hw )
			{
				if( addr )
	fprintf(temp,"  %06x(%06x) - %06x = %02x\n",naddr,memoryreadoffset[nhw],addr-1,nhw);
				nhw = hw;
				naddr = addr;
			}
			addr++;
		}
		fprintf(temp,"  %06x(%06x) - %06x = %02x\n",naddr,memoryreadoffset[nhw],addr-1,nhw);

		fprintf(temp,"cpu %d write memory \n",cpu);
		naddr = 0;
		addr = 0;
		nhw = 0xff;
		while( (addr >> mhshift[cpu][0]) <= mhmask[cpu][0] ){
			hw = cur_mw_element[cpu][addr >> mhshift[cpu][0]];
			if( hw >= MH_HARDMAX )
			{	/* 2nd element link */
				hw = writehardware[((hw-MH_HARDMAX)<<MH_SBITS) + ((addr>>mhshift[cpu][1]) & mhmask[cpu][1])];
				if( hw >= MH_HARDMAX )
					hw = writehardware[((hw-MH_HARDMAX)<<MH_SBITS) + (addr & mhmask[cpu][2])];
			}
			if( nhw != hw )
			{
				if( addr )
	fprintf(temp,"  %06x(%06x) - %06x = %02x\n",naddr,memorywriteoffset[nhw],addr-1,nhw);
				nhw = hw;
				naddr = addr;
			}
			addr++;
		}
	fprintf(temp,"  %06x(%06x) - %06x = %02x\n",naddr,memorywriteoffset[nhw],addr-1,nhw);
	}
	fclose(temp);
}
#endif

