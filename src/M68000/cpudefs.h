/*
     Definitions for the CPU-Modules
*/

#ifndef __m68000defs__
#define __m68000defs__


#include <stdlib.h>
#include "memory.h"

#ifndef __WATCOMC__
#ifdef WIN32
#define __inline__ __inline
#else
#define __inline__  inline
#endif
#endif
#ifdef __WATCOMC__
#define __inline__
#endif


#ifdef __MWERKS__
#pragma require_prototypes off
#endif



#define BYTE signed char
#define UBYTE unsigned char
#define UWORD unsigned short
#define WORD short
#define ULONG unsigned int
#define LONG int
#define CPTR unsigned int

extern void Exception(int nr, CPTR oldpc);


typedef void cpuop_func(LONG);
extern cpuop_func *cpufunctbl[65536];


typedef char flagtype;
#ifndef __WATCOMC__
#define READ_MEML(a,b) asm ("mov (%%esi),%%eax \n\t bswap %%eax \n\t" :"=a" (b) :"S" (a))
#define READ_MEMW(a,b) asm ("mov (%%esi),%%ax\n\t  xchg %%al,%%ah" :"=a" (b) : "S" (a))
#endif
#ifdef __WATCOMC__
LONG wat_readmeml(void *a);
#pragma aux wat_readmeml=\
       "mov eax,[esi]"\
       "bswap eax    "\
       parm [esi] \
       value [eax];
#define READ_MEML(a,b) b=wat_readmeml(a)
WORD wat_readmemw(void *a);
#pragma aux wat_readmemw=\
       "mov ax,[esi]"\
       "xchg al,ah    "\
       parm [esi] \
       value [ax];
#define READ_MEMW(a,b) b=wat_readmemw(a)
#endif

#define get_byte(a) cpu_readmem24((a)&0xffffff)
#define get_word(a) cpu_readmem24_word((a)&0xffffff)
#define get_long(a) cpu_readmem24_dword((a)&0xffffff)
#define put_byte(a,b) cpu_writemem24((a)&0xffffff,b)
#define put_word(a,b) cpu_writemem24_word((a)&0xffffff,b)
#define put_long(a,b) cpu_writemem24_dword((a)&0xffffff,b)


union flagu {
    struct {
        char v;
        char c;
        char n;
        char z;
    } flags;
    struct {
        unsigned short vc;
        unsigned short nz;
    } quickclear;
    ULONG longflags;
};

#define CLEARVC regflags.quickclear.vc=0;


extern int areg_byteinc[];
extern int movem_index1[256];
extern int movem_index2[256];
extern int movem_next[256];
extern int imm8_table[];
extern UBYTE *actadr;

typedef struct
{
            ULONG d[8];
            CPTR  a[8],usp,isp,msp;
            UWORD sr;
            flagtype t1;
            flagtype t0;
            flagtype s;
            flagtype m;
            flagtype x;
            flagtype stopped;
            int intmask;
            ULONG pc;

            ULONG vbr,sfc,dfc;
            double fp[8];
            ULONG fpcr,fpsr,fpiar;
} regstruct;

extern regstruct regs, lastint_regs;


extern union flagu intel_flag_lookup[256];
extern union flagu regflags;


#define ZFLG (regflags.flags.z)
#define NFLG (regflags.flags.n)
#define CFLG (regflags.flags.c)
#define VFLG (regflags.flags.v)

#ifdef ASM_MEMORY
static __inline__ UWORD nextiword_opcode(void)
{
        asm(" \
                movzwl  _regs+88,%ecx \
                movzbl  _regs+90,%ebx \
                addl    $2,_regs+88 \
                movl    _MRAM(,%ebx,4),%edx \
                movb    (%ecx, %edx),%bh \
                movb    1(%ecx, %edx),%bl \
        ");
}
#endif

static __inline__ UWORD nextiword(void)
{
    unsigned int i=regs.pc&0xffffff;
    regs.pc+=2;
    return (cpu_readop16(i));
}

static __inline__ ULONG nextilong(void)
{
    unsigned int i=regs.pc&0xffffff;
    regs.pc+=4;
    return ((cpu_readop16(i)<<16) | cpu_readop16(i+2));
}

static __inline__ void m68k_setpc(CPTR newpc)
{
    regs.pc = newpc;
    change_pc24(regs.pc&0xffffff);
}

static __inline__ CPTR m68k_getpc(void)
{
    return regs.pc;
}

static __inline__ ULONG get_disp_ea (ULONG base)
{
   UWORD dp = nextiword();
        int reg = (dp >> 12) & 7;
        LONG regd = dp & 0x8000 ? regs.a[reg] : regs.d[reg];
        if ((dp & 0x800) == 0)
            regd = (LONG)(WORD)regd;
        return base + (BYTE)(dp) + regd;
}

static __inline__ int cctrue(const int cc)
{
            switch(cc){
              case 0: return 1;                       /* T */
              case 1: return 0;                       /* F */
              case 2: return !CFLG && !ZFLG;          /* HI */
              case 3: return CFLG || ZFLG;            /* LS */
              case 4: return !CFLG;                   /* CC */
              case 5: return CFLG;                    /* CS */
              case 6: return !ZFLG;                   /* NE */
              case 7: return ZFLG;                    /* EQ */
              case 8: return !VFLG;                   /* VC */
              case 9: return VFLG;                    /* VS */
              case 10:return !NFLG;                   /* PL */
              case 11:return NFLG;                    /* MI */
              case 12:return NFLG == VFLG;            /* GE */
              case 13:return NFLG != VFLG;            /* LT */
              case 14:return !ZFLG && (NFLG == VFLG); /* GT */
              case 15:return ZFLG || (NFLG != VFLG);  /* LE */
             }
             abort();
             return 0;
}
static __inline__ void MakeSR(void)
{
    regs.sr = ((regs.t1 << 15) | (regs.t0 << 14)
      | (regs.s << 13) | (regs.m << 12) | (regs.intmask << 8)
      | (regs.x << 4) | (NFLG << 3) | (ZFLG << 2) | (VFLG << 1)
      |  CFLG);
}

static __inline__ void MakeFromSR(void)
{
  /*  int oldm = regs.m; */
    int olds = regs.s;

    regs.t1 = (regs.sr >> 15) & 1;
    regs.t0 = (regs.sr >> 14) & 1;
    regs.s = (regs.sr >> 13) & 1;
    regs.m = (regs.sr >> 12) & 1;
    regs.intmask = (regs.sr >> 8) & 7;
    regs.x = (regs.sr >> 4) & 1;
    NFLG = (regs.sr >> 3) & 1;
    ZFLG = (regs.sr >> 2) & 1;
    VFLG = (regs.sr >> 1) & 1;
    CFLG = regs.sr & 1;

    if (olds != regs.s) {
       if (olds) {
         regs.isp = regs.a[7];
         regs.a[7] = regs.usp;
        } else {
           regs.usp = regs.a[7];
           regs.a[7] = regs.isp;
        }
     }

}


#endif
