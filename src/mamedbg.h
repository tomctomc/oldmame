#ifndef _MAMEDBG_H
#define _MAMEDBG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>


#if defined (UNIX) | defined (macintosh)	/* JB 980208 */
#define uclock_t clock_t
#define	uclock clock
#define UCLOCKS_PER_SEC CLOCKS_PER_SEC
#else
#if defined(WIN32)
#include "uclock.h"
#endif
#endif

#ifndef macintosh
#ifndef WIN32
	#include <conio.h>
	#include <allegro.h>
#endif
#endif

#include "driver.h"
#include "osdepend.h"
#include "osd_dbg.h"
#include "cpuintrf.h"
#include "Z80/Z80.h"
#include "Z80/Z80Dasm.h"
#include "M6502/M6502.h"
#include "I86/i86intrf.h"
#include "I8039/i8039.h"
#include "M6808/m6808.h"
#include "M6805/m6805.h"
#include "M6809/m6809.h"
#include "M68000/M68000.h"

extern int Dasm6502 (char *buf, int pc);
extern int Dasm6808 (unsigned char *base, char *buf, int pc);
extern int Dasm6805 (unsigned char *base, char *buf, int pc);	/* JB 980214 */
extern int Dasm6809 (char *buffer, int pc);
extern int Dasm68000 (unsigned char *pBase, char *buffer, int pc);

/* JB 980214 */
extern void asg_TraceInit(int count, char *filename);
extern void asg_TraceKill(void);
extern void asg_TraceSelect(int index);
extern void asg_Z80Trace(unsigned char *RAM, int PC);
extern void asg_6809Trace(unsigned char *RAM, int PC);
extern void asg_6808Trace(unsigned char *RAM, int PC);
extern void asg_6805Trace(unsigned char *RAM, int PC);
extern void asg_6502Trace(unsigned char *RAM, int PC);
extern void asg_68000Trace(unsigned char *RAM, int PC);
extern int traceon;

extern int CurrentVolume;

#define	MEM1DEFAULT             0xc000
#define	MEM2DEFAULT             0xc200
#define	HEADING_COLOUR          LIGHTGREEN
#define	LINE_COLOUR             LIGHTCYAN
#define	REGISTER_COLOUR         WHITE
#define	FLAG_COLOUR             WHITE
#define	BREAKPOINT_COLOUR       YELLOW
#define	PC_COLOUR               (WHITE+BLUE*16)	/* MB 980103 */
#define	CURSOR_COLOUR           (WHITE+RED*16)	/* MB 980103 */
#define COMMANDINFO_COLOUR		(BLACK+7*16)	/* MB 980201 */
#define	CODE_COLOUR             WHITE
#define	MEM1_COLOUR             WHITE
#define	CHANGES_COLOUR          LIGHTCYAN
#define	MEM2_COLOUR             WHITE
#define	ERROR_COLOUR            RED
#define	PROMPT_COLOUR           CYAN
#define	INSTRUCTION_COLOUR      WHITE
#define	INPUT_COLOUR            WHITE

static unsigned char MemWindowBackup[0x100*2];	/* Enough to backup 2 windows */

static void DrawDebugScreen8 (int TextCol, int LineCol);
static void DrawDebugScreen16 (int TextCol, int LineCol);

int DummyDasm(char *S, int PC){	return (1); }
int DummyCC = 0;

void DummyTrace(int PC){;}	/* JB 980214 */

/* Temporary wrappers around these functions at the moment */
int TempDasm6808 (char *buffer, int pc) { return (Dasm6808 (&ROM[pc], buffer, pc)); }
int TempDasm6805 (char *buffer, int pc) { return (Dasm6805 (&ROM[pc], buffer, pc)); }
int TempDasm6809 (char *buffer, int pc) { return (Dasm6809 (buffer, pc)); }
int TempDasm68000 (char *buffer, int pc){ return (Dasm68000 (&ROM[pc], buffer, pc));}
int TempDasmZ80 (char *buffer, int pc)  { return (DasmZ80 (buffer, pc)); }

/* JB 980214 */
void TempZ80Trace (int PC) { asg_Z80Trace (ROM, PC); }
void Temp6809Trace (int PC) { asg_6809Trace (ROM, PC); }
void Temp6808Trace (int PC) { asg_6808Trace (ROM, PC); }
void Temp6805Trace (int PC) { asg_6805Trace (ROM, PC); }
void Temp6502Trace (int PC) { asg_6502Trace (ROM, PC); }
void Temp68000Trace (int PC) { asg_68000Trace (ROM, PC); }

/* Commands functions */
static int ModifyRegisters(char *param);
static int SetBreakPoint(char *param);
static int ClearBreakPoint(char *param);
static int Here(char *param);
static int Go(char *param);
static int DumpToFile(char *param);
static int DasmToFile(char *param);
static int DisplayMemory(char *param);
static int DisplayCode(char *param);
static int EditMemory(char *param);
static int TraceToFile(char *param);	/* JB 980214 */

/* private functions */
static void DrawMemWindow (int Base, int Col, int Offset, int DisplayASCII);	/* MB 980103 */
static int  GetNumber (int X, int Y, int Col);
static int  ScreenEdit (int XMin, int XMax, int YMin, int YMax, int Col, int BaseAdd, int *DisplayASCII);	/* MB 980103 */
static int  debug_draw_disasm (int pc);
static int  debug_dasm_line (int, char *);
static void debug_draw_flags (void);
static void debug_draw_registers (void);

static void EditRegisters (int);
static int  debug_key_pressed (void);
static int	IsRegister(int cputype, char *);
static unsigned long GetAddress(int cputype, char *src);

/* globals */
static unsigned char    rgs[CPU_CONTEXT_SIZE];	/* ASG 971105 */
static unsigned char    bckrgs[CPU_CONTEXT_SIZE];	/* MB 980221 */


typedef struct
{
	char 	*Name;
	int		*Val;
	int		Size;
	int		XPos;
	int		YPos;
} tRegdef;

typedef struct
{
	char	*Name;
	int		NamePos;
	void	(*DrawDebugScreen)(int, int);
	int		(*Dasm)(char *, int);
	void	(*Trace)(int);	/* JB 980214 */
	int		DasmLineLen;
	int		DasmStartX;
	char	*Flags;
	int		*CC;
	int		FlagSize;
	char	*AddPrint;
	int		AddMask;
	int		MemWindowAddX;
	int		MemWindowDataX;
	int		MemWindowDataXEnd;
	int		MemWindowNumBytes;
	int		MaxInstLen;
	int		*SPReg;
	int		SPSize;
	tRegdef	RegList[32];
} tDebugCpuInfo;

typedef struct
{
	tRegdef RegList[32];
} tBackupReg;



/* LEAVE cmNOMORE as last Command! */
enum { cmBPX, cmBC, cmD, cmDASM, cmDUMP, cmE, cmG, cmHERE, cmJ, cmR, cmTRACE, cmNOMORE };

typedef struct
{
	int 	Cmd;
	char	*Name;
	char	*Description;
	int		(*ExecuteCommand)(char *);
} tCommands;


static tCommands CommandInfo[] =
{
	{	cmBPX,	"BPX ",		"bpx [address]", SetBreakPoint },
	{	cmBC,	"BC ",		"Breakpoint clear", ClearBreakPoint },
	{	cmD,	"D ",		"Display <Address> [1|2]", DisplayMemory },
	{	cmDASM,	"DASM ",	"Dasm <FileName> <StartAddr> <EndAddr>", DasmToFile },
	{	cmDUMP,	"DUMP ",	"Dump <FileName> <StartAddr> <EndAddr>", DumpToFile },
	{   cmE,	"E ",		"Edit <address> [1|2]", EditMemory },
	{	cmG,	"G ", 		"Go <address>", Go },
	{	cmHERE,	"HERE ",	"Run to cursor", Here },
	{	cmJ,	"J ",		"Jump <Address>", DisplayCode },
	{	cmR,	"R ",		"r [register] = [register|value]", ModifyRegisters },
	{	cmTRACE,"TRACE ",	"Trace <FileName>|OFF", TraceToFile },	/* JB 980214 */
	{   cmNOMORE },
};

static tBackupReg BackupRegisters[] =
{
	/* Dummy CPU -- placeholder for type 0 */
	{
		{
			{ "", (int *)-1, -1, -1, -1 }
		},
	},
	/* #define CPU_Z80    1 */
	{
		{
			{ "AF", (int *)&((Z80_Regs *)bckrgs)->AF.W.l, 2, 2, 1 },
			{ "HL", (int *)&((Z80_Regs *)bckrgs)->HL.W.l, 2, 10, 1 },
			{ "DE", (int *)&((Z80_Regs *)bckrgs)->DE.W.l, 2, 18, 1 },
			{ "BC", (int *)&((Z80_Regs *)bckrgs)->BC.W.l, 2, 26, 1 },
			{ "PC", (int *)&((Z80_Regs *)bckrgs)->PC.W.l, 2, 34, 1 },
			{ "SP", (int *)&((Z80_Regs *)bckrgs)->SP.W.l, 2, 42, 1 },
			{ "IX", (int *)&((Z80_Regs *)bckrgs)->IX.W.l, 2, 50, 1 },
			{ "IY", (int *)&((Z80_Regs *)bckrgs)->IY.W.l, 2, 58, 1 },
			{ "I", (int *)&((Z80_Regs *)bckrgs)->I, 1, 66, 1 },
			{ "R", (int *)&((Z80_Regs *)bckrgs)->R, 1, 71, 1 },
			{ "", (int *)-1, -1, -1, -1 }
		},
	},
	/* #define CPU_M6502  2 */
	{
		{
			{ "A", (int *)&((M6502 *)bckrgs)->A, 1, 2, 1 },	/* JB 980103 */
			{ "X", (int *)&((M6502 *)bckrgs)->X, 1, 7, 1 },
			{ "Y", (int *)&((M6502 *)bckrgs)->Y, 1, 12, 1 },
			{ "S", (int *)&((M6502 *)bckrgs)->S, 1, 17, 1 },
			{ "PC", (int *)&((M6502 *)bckrgs)->PC.W, 2, 22, 1 },
			{ "", (int *)-1, -1, -1, -1 }
		},
	},
	/* #define CPU_I86    3 */
	{
		{
			{ "IP", (int *)&((i86_Regs *)bckrgs)->ip, 4, 2, 1 },
			{ "ES", (int *)&((i86_Regs *)bckrgs)->sregs[0], 2, 14, 1 },
			{ "CS", (int *)&((i86_Regs *)bckrgs)->sregs[1], 2, 22, 1 },
			{ "SS", (int *)&((i86_Regs *)bckrgs)->sregs[2], 2, 30, 1 },
			{ "DS", (int *)&((i86_Regs *)bckrgs)->sregs[3], 2, 38, 1 },
			{ "AX", (int *)&((i86_Regs *)bckrgs)->regs.w[0], 2, 67, 3 },
			{ "CX", (int *)&((i86_Regs *)bckrgs)->regs.w[1], 2, 67, 4 },
			{ "DX", (int *)&((i86_Regs *)bckrgs)->regs.w[2], 2, 67, 5 },
			{ "BX", (int *)&((i86_Regs *)bckrgs)->regs.w[3], 2, 67, 6 },
			{ "SP", (int *)&((i86_Regs *)bckrgs)->regs.w[4], 2, 67, 7 },
			{ "BP", (int *)&((i86_Regs *)bckrgs)->regs.w[5], 2, 67, 8 },
			{ "SI", (int *)&((i86_Regs *)bckrgs)->regs.w[6], 2, 67, 9 },
			{ "DI", (int *)&((i86_Regs *)bckrgs)->regs.w[7], 2, 67, 10 },
			{ "", (int *)-1, -1, -1, -1 }
		},
	},
	/* #define CPU_I8039  4 */
	{
		{
			{ "PC", (int *)&((I8039_Regs *)bckrgs)->PC, 2, 2, 1 },
			{ "A", (int *)&((I8039_Regs *)bckrgs)->A, 1, 10, 1 },
			{ "SP", (int *)&((I8039_Regs *)bckrgs)->SP, 1, 15, 1 },
			{ "", (int *)-1, -1, -1, -1 }
		},
	},
	/* #define CPU_M6803  5 */ /* CPU_6802, CPU_6808 */
	{
		{
			{ "A", (int *)&((m6808_Regs *)bckrgs)->a, 1, 2, 1 },
			{ "B", (int *)&((m6808_Regs *)bckrgs)->b, 1, 7, 1 },
			{ "PC", (int *)&((m6808_Regs *)bckrgs)->pc, 2, 12, 1 },
			{ "S", (int *)&((m6808_Regs *)bckrgs)->s, 2, 20, 1 },
			{ "X", (int *)&((m6808_Regs *)bckrgs)->x, 2, 27, 1 },
			{ "", (int *)-1, -1, -1, -1 }
		},
	},
	/* #define CPU_6805 6 */	/* JB 980214 */
	{
		{
			{ "A", (int *)&((m6805_Regs *)bckrgs)->a, 1, 2, 1 },
			{ "PC", (int *)&((m6805_Regs *)bckrgs)->pc, 2, 7, 1 },
			{ "S", (int *)&((m6805_Regs *)bckrgs)->s, 2, 15, 1 },
			{ "X", (int *)&((m6805_Regs *)bckrgs)->x, 2, 22, 1 },
			{ "", (int *)-1, -1, -1, -1 }
		},
	},
	/* #define CPU_M6809  7 */
	{
		{
			{ "A", (int *)&((m6809_Regs *)bckrgs)->a, 1, 2, 1 },
			{ "B", (int *)&((m6809_Regs *)bckrgs)->b, 1, 8, 1 },
			{ "PC", (int *)&((m6809_Regs *)bckrgs)->pc, 2, 14, 1 },
			{ "S", (int *)&((m6809_Regs *)bckrgs)->s, 2, 23, 1 },
			{ "U", (int *)&((m6809_Regs *)bckrgs)->u, 2, 31, 1 },
			{ "X", (int *)&((m6809_Regs *)bckrgs)->x, 2, 39, 1 },
			{ "Y", (int *)&((m6809_Regs *)bckrgs)->y, 2, 47, 1 },
			{ "DP", (int *)&((m6809_Regs *)bckrgs)->dp, 1, 55, 1 },
			{ "", (int *)-1, -1, -1, -1 }
		},
	},
	/* #define CPU_M68000 8 */
	{
		{
			{ "PC", (int *)&((MC68000_Regs *)bckrgs)->regs.pc, 4, 2, 1 },
			{ "VBR", (int *)&((MC68000_Regs *)bckrgs)->regs.vbr, 4, 14, 1 },
			{ "ISP", (int *)&((MC68000_Regs *)bckrgs)->regs.isp, 4, 27, 1 },
			{ "USP", (int *)&((MC68000_Regs *)bckrgs)->regs.usp, 4, 40, 1 },
			{ "SFC", (int *)&((MC68000_Regs *)bckrgs)->regs.sfc, 4, 53, 1 },
			{ "DFC", (int *)&((MC68000_Regs *)bckrgs)->regs.dfc, 4, 66, 1 },
			{ "D0", (int *)&((MC68000_Regs *)bckrgs)->regs.d[0], 4, 67, 3 },
			{ "D1", (int *)&((MC68000_Regs *)bckrgs)->regs.d[1], 4, 67, 4 },
			{ "D2", (int *)&((MC68000_Regs *)bckrgs)->regs.d[2], 4, 67, 5 },
			{ "D3", (int *)&((MC68000_Regs *)bckrgs)->regs.d[3], 4, 67, 6 },
			{ "D4", (int *)&((MC68000_Regs *)bckrgs)->regs.d[4], 4, 67, 7 },
			{ "D5", (int *)&((MC68000_Regs *)bckrgs)->regs.d[5], 4, 67, 8 },
			{ "D6", (int *)&((MC68000_Regs *)bckrgs)->regs.d[6], 4, 67, 9 },
			{ "D7", (int *)&((MC68000_Regs *)bckrgs)->regs.d[7], 4, 67, 10 },
			{ "A0", (int *)&((MC68000_Regs *)bckrgs)->regs.a[0], 4, 67, 12 },
			{ "A1", (int *)&((MC68000_Regs *)bckrgs)->regs.a[1], 4, 67, 13 },
			{ "A2", (int *)&((MC68000_Regs *)bckrgs)->regs.a[2], 4, 67, 14 },
			{ "A3", (int *)&((MC68000_Regs *)bckrgs)->regs.a[3], 4, 67, 15 },
			{ "A4", (int *)&((MC68000_Regs *)bckrgs)->regs.a[4], 4, 67, 16 },
			{ "A5", (int *)&((MC68000_Regs *)bckrgs)->regs.a[5], 4, 67, 17 },
			{ "A6", (int *)&((MC68000_Regs *)bckrgs)->regs.a[6], 4, 67, 18 },
			{ "A7", (int *)&((MC68000_Regs *)bckrgs)->regs.a[7], 4, 67, 19 },
			{ "", (int *)-1, -1, -1, -1 }
		},
	}
};

static tDebugCpuInfo DebugInfo[] =
{
	/* Dummy CPU -- placeholder for type 0 */
	{
		"Dummy", 14,
		DrawDebugScreen8,
		DummyDasm, DummyTrace, 0, 8,
		"........", (int *)&DummyCC, 8,
		"%04X:", 0xffff,
		25, 31, 77, 16,
		1,
		(int *)-1, -1,
		{
			{ "", (int *)-1, -1, -1, -1 }
		},
	},
	/* #define CPU_Z80    1 */
	{
		"Z80", 14,
		DrawDebugScreen8,
		TempDasmZ80, TempZ80Trace, 15, 8,	/* JB 980103 */
		"SZ.H.PNC", (int *)&((Z80_Regs *)rgs)->AF.B.l, 8,
		"%04X:", 0xffff,
		25, 31, 77, 16,
		4,
		(int *)&((Z80_Regs *)rgs)->SP.W.l, 2,
		{
			{ "AF", (int *)&((Z80_Regs *)rgs)->AF.W.l, 2, 2, 1 },
			{ "HL", (int *)&((Z80_Regs *)rgs)->HL.W.l, 2, 10, 1 },
			{ "DE", (int *)&((Z80_Regs *)rgs)->DE.W.l, 2, 18, 1 },
			{ "BC", (int *)&((Z80_Regs *)rgs)->BC.W.l, 2, 26, 1 },
			{ "PC", (int *)&((Z80_Regs *)rgs)->PC.W.l, 2, 34, 1 },
			{ "SP", (int *)&((Z80_Regs *)rgs)->SP.W.l, 2, 42, 1 },
			{ "IX", (int *)&((Z80_Regs *)rgs)->IX.W.l, 2, 50, 1 },
			{ "IY", (int *)&((Z80_Regs *)rgs)->IY.W.l, 2, 58, 1 },
			{ "I", (int *)&((Z80_Regs *)rgs)->I, 1, 66, 1 },
			{ "R", (int *)&((Z80_Regs *)rgs)->R, 1, 71, 1 },
			{ "", (int *)-1, -1, -1, -1 }
		},
	},
	/* #define CPU_M6502  2 */
	{
		"6502", 14,
		DrawDebugScreen8,
		Dasm6502, Temp6502Trace, 15, 8,
		"NVRBDIZC", (int *)&((M6502 *)rgs)->P, 8,
		"%04X:", 0xffff,
		25, 31, 77, 16,
		3,
		(int *)&((M6502 *)rgs)->S, 1,
		{
			{ "A", (int *)&((M6502 *)rgs)->A, 1, 2, 1 },	/* JB 980103 */
			{ "X", (int *)&((M6502 *)rgs)->X, 1, 7, 1 },
			{ "Y", (int *)&((M6502 *)rgs)->Y, 1, 12, 1 },
			{ "S", (int *)&((M6502 *)rgs)->S, 1, 17, 1 },
			{ "PC", (int *)&((M6502 *)rgs)->PC.W, 2, 22, 1 },
			{ "", (int *)-1, -1, -1, -1 }
		},
	},
	/* #define CPU_I86    3 */
	{
		"I86", 21,
		DrawDebugScreen16,
		DummyDasm, DummyTrace, 20, 10,
		"................", (int *)&((i86_Regs *)rgs)->flags, 16,
		"%06X:     ", 0xffffff,
		32, 40, 62, 8,
		5,
		(int *)&((i86_Regs *)rgs)->regs.w[4], 2,
		{
			{ "IP", (int *)&((i86_Regs *)rgs)->ip, 4, 2, 1 },
			{ "ES", (int *)&((i86_Regs *)rgs)->sregs[0], 2, 14, 1 },
			{ "CS", (int *)&((i86_Regs *)rgs)->sregs[1], 2, 22, 1 },
			{ "SS", (int *)&((i86_Regs *)rgs)->sregs[2], 2, 30, 1 },
			{ "DS", (int *)&((i86_Regs *)rgs)->sregs[3], 2, 38, 1 },
			{ "AX", (int *)&((i86_Regs *)rgs)->regs.w[0], 2, 67, 3 },
			{ "CX", (int *)&((i86_Regs *)rgs)->regs.w[1], 2, 67, 4 },
			{ "DX", (int *)&((i86_Regs *)rgs)->regs.w[2], 2, 67, 5 },
			{ "BX", (int *)&((i86_Regs *)rgs)->regs.w[3], 2, 67, 6 },
			{ "SP", (int *)&((i86_Regs *)rgs)->regs.w[4], 2, 67, 7 },
			{ "BP", (int *)&((i86_Regs *)rgs)->regs.w[5], 2, 67, 8 },
			{ "SI", (int *)&((i86_Regs *)rgs)->regs.w[6], 2, 67, 9 },
			{ "DI", (int *)&((i86_Regs *)rgs)->regs.w[7], 2, 67, 10 },
			{ "", (int *)-1, -1, -1, -1 }
		},
	},
	/* #define CPU_I8039  4 */
	{
		"I8039", 14,
		DrawDebugScreen8,
		DummyDasm, DummyTrace, 0, 8,
		"........", (int *)&((I8039_Regs *)rgs)->PSW, 8,
		"%04X:", 0xffff,
		25, 31, 77, 16,
		1,
		(int *)&((I8039_Regs *)rgs)->SP, 1,
		{
			{ "PC", (int *)&((I8039_Regs *)rgs)->PC, 2, 2, 1 },
			{ "A", (int *)&((I8039_Regs *)rgs)->A, 1, 10, 1 },
			{ "SP", (int *)&((I8039_Regs *)rgs)->SP, 1, 15, 1 },
			{ "", (int *)-1, -1, -1, -1 }
		},
	},
	/* #define CPU_M6803  5 */ /* CPU_6802, CPU_6808 */
	{
		"6808", 14,
		DrawDebugScreen8,
		TempDasm6808, Temp6808Trace, 15, 8,
		"..HINZVC", (int *)&((m6808_Regs *)rgs)->cc, 8,
		"%04X:", 0xffff,
		25, 31, 77, 16,
		4,
		(int *)&((m6808_Regs *)rgs)->s, 2,
		{
			{ "A", (int *)&((m6808_Regs *)rgs)->a, 1, 2, 1 },
			{ "B", (int *)&((m6808_Regs *)rgs)->b, 1, 7, 1 },
			{ "PC", (int *)&((m6808_Regs *)rgs)->pc, 2, 12, 1 },
			{ "S", (int *)&((m6808_Regs *)rgs)->s, 2, 20, 1 },
			{ "X", (int *)&((m6808_Regs *)rgs)->x, 2, 27, 1 },
			{ "", (int *)-1, -1, -1, -1 }
		},
	},
	/* #define CPU_6805 6 */	/* JB 980214 */
	{
		"6805", 14,
		DrawDebugScreen8,
		TempDasm6805, Temp6805Trace, 15, 8,
		"...HINZC",  (int *)&((m6805_Regs *)rgs)->cc, 8,
		"%04X:", 0xffff,
		25, 31, 77, 16,
		4,
		(int *)&((m6805_Regs *)rgs)->s, 2,
		{
			{ "A", (int *)&((m6805_Regs *)rgs)->a, 1, 2, 1 },
			{ "PC", (int *)&((m6805_Regs *)rgs)->pc, 2, 7, 1 },
			{ "S", (int *)&((m6805_Regs *)rgs)->s, 2, 15, 1 },
			{ "X", (int *)&((m6805_Regs *)rgs)->x, 2, 22, 1 },
			{ "", (int *)-1, -1, -1, -1 }
		},
	},
	/* #define CPU_M6809  7 */
	{
		"6809", 14,
		DrawDebugScreen8,
		TempDasm6809, Temp6809Trace, 15, 8,
		"..H.NZVC", (int *)&((m6809_Regs *)rgs)->cc, 8,
		"%04X:", 0xffff,
		25, 31, 77, 16,
		5,
		(int *)&((m6809_Regs *)rgs)->s, 2,
		{
			{ "A", (int *)&((m6809_Regs *)rgs)->a, 1, 2, 1 },
			{ "B", (int *)&((m6809_Regs *)rgs)->b, 1, 8, 1 },
			{ "PC", (int *)&((m6809_Regs *)rgs)->pc, 2, 14, 1 },
			{ "S", (int *)&((m6809_Regs *)rgs)->s, 2, 23, 1 },
			{ "U", (int *)&((m6809_Regs *)rgs)->u, 2, 31, 1 },
			{ "X", (int *)&((m6809_Regs *)rgs)->x, 2, 39, 1 },
			{ "Y", (int *)&((m6809_Regs *)rgs)->y, 2, 47, 1 },
			{ "DP", (int *)&((m6809_Regs *)rgs)->dp, 1, 55, 1 },
			{ "", (int *)-1, -1, -1, -1 }
		},
	},
	/* #define CPU_M68000 8 */
	{
		"68K", 21,
		DrawDebugScreen16,
		TempDasm68000, Temp68000Trace, 21, 10,
		"T.S..III...XNZVC", (int *)&((MC68000_Regs *)rgs)->regs.sr, 16,
		"%06.6X:     ", 0xffffff,
		33, 41, 63, 8,
		8,
		(int *)&((MC68000_Regs *)rgs)->regs.isp, 4,
		{
			{ "PC", (int *)&((MC68000_Regs *)rgs)->regs.pc, 4, 2, 1 },
			{ "VBR", (int *)&((MC68000_Regs *)rgs)->regs.vbr, 4, 14, 1 },
			{ "ISP", (int *)&((MC68000_Regs *)rgs)->regs.isp, 4, 27, 1 },
			{ "USP", (int *)&((MC68000_Regs *)rgs)->regs.usp, 4, 40, 1 },
			{ "SFC", (int *)&((MC68000_Regs *)rgs)->regs.sfc, 4, 53, 1 },
			{ "DFC", (int *)&((MC68000_Regs *)rgs)->regs.dfc, 4, 66, 1 },
			{ "D0", (int *)&((MC68000_Regs *)rgs)->regs.d[0], 4, 67, 3 },
			{ "D1", (int *)&((MC68000_Regs *)rgs)->regs.d[1], 4, 67, 4 },
			{ "D2", (int *)&((MC68000_Regs *)rgs)->regs.d[2], 4, 67, 5 },
			{ "D3", (int *)&((MC68000_Regs *)rgs)->regs.d[3], 4, 67, 6 },
			{ "D4", (int *)&((MC68000_Regs *)rgs)->regs.d[4], 4, 67, 7 },
			{ "D5", (int *)&((MC68000_Regs *)rgs)->regs.d[5], 4, 67, 8 },
			{ "D6", (int *)&((MC68000_Regs *)rgs)->regs.d[6], 4, 67, 9 },
			{ "D7", (int *)&((MC68000_Regs *)rgs)->regs.d[7], 4, 67, 10 },
			{ "A0", (int *)&((MC68000_Regs *)rgs)->regs.a[0], 4, 67, 12 },
			{ "A1", (int *)&((MC68000_Regs *)rgs)->regs.a[1], 4, 67, 13 },
			{ "A2", (int *)&((MC68000_Regs *)rgs)->regs.a[2], 4, 67, 14 },
			{ "A3", (int *)&((MC68000_Regs *)rgs)->regs.a[3], 4, 67, 15 },
			{ "A4", (int *)&((MC68000_Regs *)rgs)->regs.a[4], 4, 67, 16 },
			{ "A5", (int *)&((MC68000_Regs *)rgs)->regs.a[5], 4, 67, 17 },
			{ "A6", (int *)&((MC68000_Regs *)rgs)->regs.a[6], 4, 67, 18 },
			{ "A7", (int *)&((MC68000_Regs *)rgs)->regs.a[7], 4, 67, 19 },
			{ "", (int *)-1, -1, -1, -1 }
		},
	},
};


#endif /* _MAMEDBG_H */
