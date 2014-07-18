//----------------------------------------------------------------------
//  e3.c v0.92  Copyright (C) 2000 Albrecht Kleine  <kleine@ak.sax.de>
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
//  adapted to mipsel (fritz box fon): 2005-08 C.O. (Christian Ostheimer)
//---------------------------------------------------------------------

#ifndef __GNUC__
#error "Sorry, but I need GNU C."
#endif

#ifndef MAX_TEXTSIZE
 #define MAX_TEXTSIZE 10240000 // x86 pc
#endif
#define CURSORMGNT
#define LESSWRITEOPS
#ifdef LESSWRITEOPS
#define LESSWRITEOPS_OR_CURSORMGNT
#endif
#ifdef CURSORMGNT
#define LESSWRITEOPS_OR_CURSORMGNT
#endif

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

struct termios termios,orig;
//---------------------------------------------------------------------
//
//  CONSTANT DATA AREA
//
#define lowest 0x3b
unsigned char Ktable[]={
	0x45 - lowest,		// ^K@	xlatb table for making pseudo-scancode
	0x45 - lowest,		// ^ka	45h points to an an offset in jumptab1
	0x41 - lowest,		// ^kb	41h for example points to KeyCtrlKB function offset
	0x43 - lowest,		// ^kc
	0x5d - lowest,		// ^kd
	0x45 - lowest,		// ^ke	45h means SimpleRet i.e. 'do nothing'
	0x45 - lowest,		// ^kf
	0x45 - lowest,		// ^kg
	0x57 - lowest,		// ^kh
	0x45 - lowest,		// ^ki
	0x45 - lowest,		// ^kj
	0x42 - lowest,		// ^kk
	0x45 - lowest,		// ^kl
	0x45 - lowest,		// ^km
	0x45 - lowest,		// ^kn
	0x45 - lowest,		// ^ko
	0x45 - lowest,		// ^kp
	0x46 - lowest,		// ^kq
	0x3d - lowest,		// ^kr
	0x5c - lowest,		// ^ks
	0x45 - lowest,		// ^kt
	0x45 - lowest,		// ^ku
	0x3b - lowest,		// ^kv
	0x3e - lowest,		// ^kw
	0x44 - lowest,		// ^kx
	0x4e - lowest,		// ^ky
	0x45 - lowest};		// ^kz

unsigned char Qtable[]={
	0x45 - lowest,		// ^q@	ditto for ^Q menu
	0x54 - lowest,		// ^qa
	0x5a - lowest,		// ^qb
	0x61 - lowest,		// ^qc former 76h ctrl-PageDown
	0x4f - lowest,		// ^qd
	0x58 - lowest,		// ^qe
	0x55 - lowest,		// ^qf
	0x45 - lowest,		// ^qg
	0x4a - lowest,		// ^qh, ^qDEL
	0x62 - lowest,		// ^qi
	0x45 - lowest,		// ^qj
	0x5b - lowest,		// ^qk
	0x45 - lowest,		// ^ql
	0x45 - lowest,		// ^qm
	0x45 - lowest,		// ^qn
	0x45 - lowest,		// ^qo
	0x4c - lowest,		// ^qp
	0x45 - lowest,		// ^qq
	0x63 - lowest,		// ^qr former ctrl-PageUp
	0x47 - lowest,		// ^qs
	0x45 - lowest,		// ^qt
	0x45 - lowest,		// ^qu
	0x56 - lowest,		// ^qv
	0x5e - lowest,		// ^qw former 73h ctrl-left
	0x59 - lowest,		// ^qx
	0x40 - lowest,		// ^qy
	0x5f - lowest};		// ^qz former 74h ctrl-right

//  Using terminals the F-keys are not yet supported (was DOS only). 



//------
char filename[]		="   NAME:";
char filesave[]		="   SAVE:";
char asksave[]		="SAVE? Y/N";
char block[]		="BLK NAME:";
char askfind[]		="^QF FIND:";
char askreplace1[]	="^QA REPL:";
char askreplace2[]	="^QA WITH:";
char asklineno[]	="^QI LINE:";
char optiontext[]	="OPT? C/B";
#define stdtxtlen	10

char screencolors0[]={27,'[','4','0','m',27,'[','3','7','m',27,'[','0','m'};
// pointer pbold0 instead of array bold0[] to avoid alignment issue on mipsel
// (content of bold0[] is now appended to screencolors0, C.O.)
char * pbold0=screencolors0 + 10;	// reset to b/w;
char screencolors1[]={27,'[','4',/*'4'*/'1','m',27,'[','3','3','m',27,'[','1','m',27,'[','7','m'}; // yellow on red /*blue*/
char * pbold1=screencolors1 + 10;	// bold 

#define preversevideoX pbold1		// good for "linux" terminal on /dev/tty (but not xterm,kvt)
#define scolorslen	14
#define boldlen	4			// take care length of bold0 == length of bold1
//#define O_WRONLY_CREAT_TRUNC	01101
#define O_WRONLY_CREAT_TRUNC	(O_WRONLY | O_CREAT | O_TRUNC)
#define PERMS	0644

char resfile[]=LIBDIR"/e3.res";
char helpfile[]=LIBDIR"/e3ws.hlp";

#define TAB		8
#define TABCHAR		0x9	//  ^I
#define SPACECHAR	' '
#define CHANGED		'*'
#define UNCHANGED	SPACECHAR
#define LINEFEED	0xa
#define NEWLINE		LINEFEED

//---------------------------------------------------------------------

unsigned char screenbuffer [62*(160+32)];	// estimated 62 lines 160 columns, 32 byte ESC seq (ca.12k)
#define screenbuffer_end screenbuffer[62*(160+32)]
					// If you really have higher screen resolution,
					// ...no problem, except some useless redrawing happens.

#define winsize_size	8
unsigned char winsize [winsize_size];

#define setkplen	10
unsigned char setkp [setkplen];		// to store cursor ESC seq like  db 27,'[000;000H'

long revvoff;

long lines;			// equ 24 or similar i.e. screen lines-1 (1 for statusline)
long columns;			// equ 80 or similar dword (using only LSB)

long columne;			// helper for display of current column
long zloffst;			// helper: chars scrolled out at left border
long fileptr;			// helper for temp storage of current pos in file
long kurspos;			// cursor position set by DispNewScreen()
long kurspos2;			// cursor position set by other functions

long tabcnt;			// longernal helper byte in DispNewScreen() only
long changed;			// status byte: (UN)CHANGED
long oldQFpos;
long bereitsges;		// byte used for ^L
long blockbegin;
long blockende;
long endeedit;			// byte controls program exit
long old;			// helper for ^QP
long veryold;			// ditto
long linenr;			// current line
long showblock;			// helper for ^KH
long suchlaenge;		// helper for ^QA,^QF
long repllaenge;
long vorwarts;
long grossklein;		// helper byte for ^QF,^QA

long ch2linebeg;		// helper keeping cursor pos max at EOL (up/dn keys)
long numeriere;			// byte controls re-numeration
long read_b;			// buffer for getchar
long isbold;			// control of bold display of ws-blocks
long inverse;
long insstat;

#define errlen	100
unsigned char error [errlen];	// reserved space for string: 'ERROR xxx:tttteeeexxxxtttt',0

long maxlen;

#define maxfilenamelen	255
unsigned char filepath [maxfilenamelen+1];
unsigned char bakpath [maxfilenamelen+1];
unsigned char blockpath [maxfilenamelen+1];
unsigned char replacetext [maxfilenamelen+1];
unsigned char suchtext [maxfilenamelen+1];
#define optslen	8
unsigned char optbuffer [optslen];	// buffer for search/replace options and for ^QI

long uid;
long gid;
long perms;
struct stat fstatbuf;

unsigned char screenline [256+4*scolorslen];	// max possible columns + 4 color ESC seq per line
					// (buffer for displaying a text line)
#define errbufsize	4000

unsigned char errmsgs [errbufsize];
//------
#define max	MAX_TEXTSIZE
unsigned char text [max];
#define sot	(text+1)		// start-of-text

//--------------------------- END OF DATA SECTION ---------------------

long stack[100];

#define RETURN	goto *(*esp--)
#define CALL(calladr,retadr) *++esp=(long)&&_loc##retadr; goto calladr; _loc##retadr:
#define PUSH(arg) *++esp=arg
//#define POP(arg) (long*)arg=*esp--
#define POP(arg) arg=(long *)(*esp--);

int main(int argc,char**argv,char **envp)
{
  long eax=0,ecx=0,edx=0,ebx=0,edi=0,esi=0,extra=0,extra2=0,parm;
  long *esp=stack;
  unsigned char *ebp=0;
  #define jumps1 0x29
  static void* jarray[]={
	&&KeyCtrlKV,			// 3bh  ^KV  F1 (DOS only)
	&&KeyCtrlL,			// 3ch  ^L   F2 (ditto)
	&&KeyCtrlKR,			// 3dh  ^KR  F3 (etc)
	&&KeyCtrlKW,			// 3eh  ^KW
	&&KeyCtrlT,			// 3fh  ^T
	&&KeyCtrlQY,			// 40h  ^QY
	&&KeyCtrlKB,			// 41h  ^KB
	&&KeyCtrlKK,			// 42h  ^KK
	&&KeyCtrlKC,			// 43h  ^KC
	&&KeyCtrlKX,			// 44h  ^KX  F10
	&&SimpleRet,			// 45h       F11
	&&KeyCtrlKQ,			// 46h       F12
	&&KeyHome,			// 47h
	&&KeyUp,			// 48h
	&&KeyPgUp,			// 49h
	&&KeyCtrlQDel,			// 4ah ^QDel
	&&KeyLeft,			// 4bh
	&&KeyCtrlQP,			// (5 no num lock) 
	&&KeyRight,			// 4dh
	&&KeyCtrlKY,			// (+)  ^KY
	&&KeyEnd,			// 4fh
	&&KeyDown,			// 50H
	&&KeyPgDn,			// 51h
	&&KeyIns,			// 52H
	&&KeyDel,			// 53H
	&&KeyCtrlQA,			// 54h ^QA sF1
	&&KeyCtrlQF,			// 55h ^QF sf1
	&&KeyCtrlQV,			// 56h
	&&KeyCtrlKH,			// 57h
	&&KeyCtrlQE,			// 58h
	&&KeyCtrlQX,			// 59h
	&&KeyCtrlQB,			// 5Ah ^QB
	&&KeyCtrlQK,			// 5Bh ^QK  sF8
	&&KeyCtrlKS,			// 5ch ^KS  sF9
	&&KeyCtrlKD,			// 5dh ^KD  sF10
	&&KeyCtrlLeft,			// 5eh	^Left was 73h  (compare notes above)
	&&KeyCtrlRight,			// 5fh	^Right (was 74h)
	&&SimpleRet,			// 60h	^End (was 75h)
	&&KeyCtrlQC,			// 61h	^PageDown (was 76h)
	&&KeyCtrlQI,			// 62h	^Home  (was 77h)
	&&KeyCtrlQR,			// 63h	^PageUp (was 84h)
// jumps1......= 29h
	&&SimpleRet,			// ^@	(only former DOS version has a "jumptab2")
	&&KeyHome,			// ^a	(compare notes above)
	&&SimpleRet,			// ^b
	&&KeyPgDn,			// ^c
	&&KeyRight,			// ^d
	&&KeyUp,			// ^e
	&&KeyEnd,			// ^f
	&&KeyDel,			// ^g 7
	&&KeyDell,			// ^h 8   DEL (7fh is translated to this)
	&&NormChar,			// ^i 9
	&&KeyRet,			// ^j = 0ah
	&&CtrlKMenu,			// ^k b
	&&KeyCtrlL,			// ^l c
	&&SimpleRet,			// ^m 0dh
	&&SimpleRet,			// ^n e
	&&SimpleRet,			// ^o f
	&&SimpleRet,			// ^p 10
	&&CtrlQMenu,			// ^q 11
	&&KeyPgUp,			// ^r 12
	&&KeyLeft,			// ^s 13
	&&KeyCtrlT,			// ^t 14
	&&SimpleRet,			// ^u 15
	&&KeyIns,			// ^v 16
	&&SimpleRet,			// ^w 17
	&&KeyDown,			// ^x 18
	&&KeyCtrlY,			// ^y 19
	&&SimpleRet,			// ^z
	&&SimpleRet			// esc
	};
	CALL(SetTermStruc,0);
	CALL(ReadResource,1);
	esi=(long)*++argv;
#ifdef CURSORMGNT
	if (!strcmp(getenv("TERM"),"linux"))
	 revvoff+=boldlen;		// special inverse cursor on linux terminals
#endif	
//------
	do {
	 CALL(NewFile,2);
	 if (extra) break;
	 do {
	  CALL(DispNewScreen,3);
	  CALL(RestoreStatusLine,4);
	  CALL(HandleChar,5);
	 } while (!endeedit);
	 esi=0;				// just like if no arg is present
	} while (endeedit==2);		// ^KD repeat edit using another file
//------
	CALL(KursorStatusLine,6);
	ecx = (long)&text;		// enter next line on terminal NEWLINE is @ byte [text]
	edx=1;
	CALL(WriteFile0,7);
	eax = (long)&orig;
	CALL(SetTermStruc2,8);		// restore termios settings
	goto Exit;
//---------------------------------------------------------------------
//  MAIN function for processing keys
//
HandleChar:CALL(ReadChar,9);
	if (!eax) goto CompJump2;	// eax=0 for cursor keys
	if (eax>=0x1c) goto NormChar;
	ebx = eax+jumps1;
	goto CompJump2;
NormChar:CALL(CheckMode,10);
	if (!extra) goto OverWriteChar;
	PUSH(eax);
	eax=1;
	CALL(InsertByte,11);
	POP(eax);
	if (extra) goto InsWriteEnd;	// error: text buffer full
OverWriteChar:*(unsigned char*)edi++=eax;
	changed = CHANGED;
InsWriteEnd:RETURN;
//------
//
//  helper for HandleChar
//
CtrlKMenu:ebx = (long)&Ktable;
	ecx = 0x20204b5e;		// ^K
	goto Menu;
CtrlQMenu:ebx = (long)&Qtable;
	ecx = 0x2020515e;		// ^Q
Menu:	CALL(MakeScanCode,12);
	if (eax>=27) goto EndeRet;	// if no valid scancode
CompJump2:ebx=(long)jarray[(int)ebx];
//------
	CALL(*ebx,13);			// the general code jump dispatcher
//------	
	if (numeriere)
	{
	PUSH(edi);
	esi=edi;
	edi = (long)(sot);
	linenr=0;
	do {
	linenr++;
	CALL(LookForward,14);
	++edi;				// point to start of next line
	} while ((unsigned long)edi<=(unsigned long)esi);
	POP(edi);
	}
	numeriere = 0;
	RETURN;
//------
MakeScanCode:CALL(WriteTwo,15);		// ebx expects xlat-table
	PUSH(ebx);
	CALL(GetChar,16);
	POP(ebx);
	eax&=0x1f;
	if (eax>=27) goto EndeRet;
	ebx=*((unsigned char*)(ebx+eax));	// returns pseudo "scancode" in ebx
EndeRet:RETURN;				// exception: ok=cy here
//---------------------------------------------------------------------
//
//  processing special keys: cursor, ins, del
//
KeyRet:	CALL(CheckMode,17);
	if (!extra) goto OvrRet;
	CALL(CountToLineBegin,18);	// set esi / returns eax
	++esi;
	++esi;
	if (!eax) goto KeyRetNoIndent;
	eax=-1;
KeyRetSrch:++eax;			// search non (SPACE or TABCHAR)
	if (*(unsigned char*)(esi+eax)==SPACECHAR) goto KeyRetSrch;
	if (*(unsigned char*)(esi+eax)==TABCHAR)   goto KeyRetSrch;
KeyRetNoIndent:
	PUSH(esi);
	PUSH(eax);			// eax is 0 or =indented chars
	CALL(GoDown,19);
	POP(eax);
	PUSH(eax);
	++eax;				// 1 extra for 0ah
	CALL(InsertByte,20);
	POP(ecx);			// # blanks
	POP(esi);			// where to copy
	if (extra) goto SimpleRet;
	++linenr;
	*(unsigned char*)edi++=NEWLINE;
	if (ecx)
	do {
	*(unsigned char*)edi++=*(unsigned char*)esi++;
	} while (--ecx);		// copy upper line i.e. SPACES,TABS into next
SimpleRet:RETURN;
OvrRet:	eax=0;
	ch2linebeg = eax;
	goto DownRet;
//------
KeyDown:CALL(CountColToLineBeginVis,21);
DownRet:CALL(GoDown,22);
	CALL(LookLineDown,23);
	goto SetColumn;
//------
KeyUp:	CALL(GoUp,24);
	CALL(CountColToLineBeginVis,25);
	CALL(LookLineUp,26);
	goto SetColumn;
//------
KeyPgUp:CALL(CountColToLineBeginVis,27);
	CALL(LookPageUp,28);
	goto SetColumn;
//------
KeyPgDn:CALL(CountColToLineBeginVis,29);
	CALL(LookPgDown,30);		// 1st char last line
//------
SetColumn:ecx = ch2linebeg;		// maximal columns
	edx=0;				// counts visible columns i.e. expand TABs
	--edi;
SCloop:	++edi;
	if (edx>=ecx) goto SCret;
	if (*(unsigned char*)edi==NEWLINE) goto SCret;	// don't go beyond line earlier line end
	if (*(unsigned char*)edi==TABCHAR) goto SCtab;
	++edx;				// count columns
	goto SCloop;
SCtab:	edx+= (TAB - edx % TAB);		// spaces for Tab:
	if (edx<=ecx) goto SCloop;	// this tab to far away right? no
SCret:	RETURN;
//------
KeyHome:CALL(CountToLineBegin,32);
	edi-=eax;
	RETURN;
//------
KeyEnd:	CALL(CountToLineEnd,33);
	edi+=eax;			// points to a 0ah char
	RETURN;
//------
KeyIns:	insstat = !insstat;
	RETURN;
//------
KeyDell:CALL(KeyLeft,34);
	if (extra) goto KeyDell2;
KeyDel:	if ((unsigned long)edi>=(unsigned long)ebp) goto KeyLeftEnd;
	eax=1;				// delete one @ cursor
	goto DeleteByte;
KeyDell2:if ((unsigned long)edi<=(unsigned long)sot) goto KeyLeftEnd;
	--linenr;
	--edi;
	goto KeyCtrlT1;
//------
KeyLeft:if ((((unsigned char*)edi)[-1]) != NEWLINE)
	{
	  --edi;
	  extra=0;
	}
	else
	 extra=1;
KeyLeftEnd:RETURN;
//------
KeyRight:if (*(unsigned char*)edi != NEWLINE)
	{
	  ++edi;
	  extra=0;
	}
	else
	 extra=1;	  
	RETURN;
//------
KeyCLeft3:if ((unsigned long)edi<=(unsigned long)(sot)) goto KeyCLEnd;
	--edi;
KeyCtrlLeft:CALL(KeyLeft,35);
	if (extra) goto KeyCLeft3;
	if (*(unsigned char*)edi <= 0x2f) goto KeyCtrlLeft;
	if (((unsigned char*)edi)[-1] > 0x2f) goto KeyCtrlLeft;
KeyCLEnd:RETURN;
//------
KeyCRight3:CALL(CheckEof,36);
	if (extra) goto KeyCREnd;
	++edi;
KeyCtrlRight:CALL(KeyRight,37);
	if (extra) goto KeyCRight3;
	if (*(unsigned char*)edi <= 0x2f) goto KeyCtrlRight;
	if (((unsigned char*)edi)[-1] > 0x2f) goto KeyCtrlRight;
KeyCREnd:RETURN;
//---------------------------------------------------------------------
//
//  processing special keys from the Ctrl-Q menu
//
KeyCtrlQE:CALL(LookPgBegin,38);		// goto top left on screen
	CALL(KursorFirstLine,39);
	goto CQFNum;
//------
KeyCtrlQX:CALL(LookPgEnd,40);		// 1st goto last line on screen
	CALL(KeyEnd,41);		// 2nd goto line end
	CALL(KursorLastLine,42);
	goto CQFNum;
//------
KeyCtrlQV:if (!bereitsges) goto CtrlQFEnd;	// goto last ^QA,^QF pos
	edi = oldQFpos;
	goto CQFNum;
//------
KeyCtrlQA:bereitsges = 2;
	CALL(AskForReplace,43);
	if (extra) goto CtrlQFEnd;
CQACtrlL:PUSH(edi);
	CALL(FindText,44);
	if (!extra) goto CtrlQFNotFound;
	eax = suchlaenge;
	CALL(DeleteByte,45);
	eax = repllaenge;
	CALL(InsertByte,46);
	esi = (long)&replacetext;
	CALL(MoveBlock,47);
	goto CQFFound;
//------
KeyCtrlQF:bereitsges = 1;
	CALL(AskForFind,48);
	if (extra) goto CtrlQFEnd;
CQFCtrlL:PUSH(edi);
	CALL(FindText,49);
	if (!extra) goto CtrlQFNotFound;
CQFFound:oldQFpos = edi;
	POP(esi);			// dummy
CQFNum:	numeriere = 1;
	RETURN;
CtrlQFNotFound:POP(edi);
CtrlQFEnd:RETURN;
//------
KeyCtrlQC:edi = (long)ebp;
	goto CQFNum;
//------
KeyCtrlQR:edi = (long)sot;
	goto CQFNum;
//------
KeyCtrlQP:ecx = veryold;
	if ((unsigned long)ecx>(unsigned long)ebp) goto SimpleRet4;
	edi=ecx;
//------
KeyCtrlL:if (bereitsges==2) goto CQACtrlL;
	if (bereitsges==1) goto CQFCtrlL;
SimpleRet4:RETURN;
//------
KeyCtrlQB:eax=edi;
	edi = blockbegin;
CtrlQB2:if (edi) goto CQFNum;
	edi=eax;			// exit if no marker set
	RETURN;
//------
KeyCtrlQK:eax=edi;
	edi = blockende;
	goto CtrlQB2;
//------
KeyCtrlQI:CALL(GetAsciiToInteger,50);
	if (!ecx) goto SimpleRet4;
	edi = (long)sot;
	CALL(LookPD2,51);
JmpCQFN:goto CQFNum;
//------
KeyCtrlQDel:CALL(KeyLeft,52);		// delete all left of cursor
	CALL(CountToLineBegin,53);
	edi-=eax;
	CALL(DeleteByteCheckMarker,54);
	goto KeyCtrlT1;
//------
KeyCtrlQY:CALL(CountToLineEnd,55);
	goto CtrlTEnd1;
//------
KeyCtrlY:CALL(CountToLineBegin,56);
	edi-=eax;			// edi at begin
	CALL(CountToLineEnd,57);
	CALL(DeleteByteCheckMarker,58);
	goto KeyCtrlT1;
//------
KeyCtrlT:CALL(CountToWordBegin,59);
	if (*(unsigned char*)edi != NEWLINE) goto CtrlTEnd1;
KeyCtrlT1:CALL(CheckEof,60);
	if (extra) goto SimpleRet4;
	eax=1;				// 1 for 0ah
CtrlTEnd1:goto DeleteByteCheckMarker;
//---------------------------------------------------------------------
//
//  processing special Keys from Ctrl-K menu
//
KeyCtrlKY:CALL(CheckBlock,61);
	if (extra) goto SimpleRet3;	// no block: no action
	eax = blockende;
	edi = esi;			// esi is blockbegin (side effect in CheckBlock)
	eax-=esi;			// block length
	CALL(DeleteByte,62);		// out ecx:=0
	blockende = ecx;		// i.e. NO block valid now
	blockbegin = ecx;
	goto JmpCQFN;
//------
KeyCtrlKH:showblock^=1;			// flip flop
SimpleRet3:RETURN;
//------
KeyCtrlKK:blockende = edi;
	goto KCKB;
//------
KeyCtrlKW:CALL(CheckBlock,63);
	if (extra) goto SimpleRet2;	// no action
	CALL(SaveBlock,64);
	goto CtrlKREnd;
//------
KeyCtrlKC:CALL(CopyBlock,65);
	if (extra) goto SimpleRet2;
CtrlKC2:blockbegin = edi;
	blockende = eax+edi;
	RETURN;
//------
KeyCtrlKV:CALL(CopyBlock,66);
	if (extra) goto SimpleRet2;
	PUSH(edi);
	extra=0;
	if ((unsigned long)edi<(unsigned long)blockbegin) extra++;
	PUSH(extra);
	edi = blockbegin;
	CALL(DeleteByte,67);
	eax = -eax;			// (for optimizing eax is negated there)
	POP(extra);
	POP(edi);
	if (extra) goto CtrlKC2;
	blockende = edi;
	edi-=eax;
KeyCtrlKB:blockbegin = edi;
KCKB:	showblock = 1;
SimpleRet2:RETURN;
//------
KeyCtrlKR:CALL(ReadBlock,68)
	if (extra) goto CtrlKREnd;
	CALL(KeyCtrlKB,69);
	ecx+=edi;
	blockende = ecx;
CtrlKREnd:goto RestKursPos;
//------
KeyCtrlKS:CALL(SaveFile,70);		// (called by ^kd)
	PUSH(extra);
	CALL(RestKursPos,71);
	POP(extra);
	if (extra) goto CtrlKSEnd;
	changed = UNCHANGED;
CtrlKSEnd:RETURN;
//------
KeyCtrlKQ:if (changed==UNCHANGED) goto KCKXend;
	esi = (long)&asksave;
	CALL(DE1,72);
	CALL(RestKursPos,73);
	eax&=0xdf;
	if (eax !='Y') goto KCKXend;	// Y for request SAVE changes
KeyCtrlKX:CALL(KeyCtrlKS,74);
	if (extra) goto CtrlKSEnd;
KCKXend:++endeedit;
KeyKXend:RETURN;
KeyCtrlKD:CALL(KeyCtrlKS,75);
	if (extra) goto CtrlKSEnd;
	endeedit = 2;
	RETURN;
//--------------------------------------------------------------------
//
//  the general PAGE DISPLAY function: called after any pressed key
//
//  side effect: sets 'columne' for RestoreStatusLine function (displays columne)
//  variable kurspos: for placing the cursor at new position
//  register extra2 counts lines
//  register ebx counts columns visible on screen (w/o left scrolled)
//  register edx counts columns in text lines
//  register ecx screen line counter and helper for rep stos
//  register esi text index
//  register edi screen line buffer index
//
DispNewScreen:CALL(GetEditScreenSize,76);	// check changed tty size
	isbold = 0;
	inverse = 0;
	zloffst = 0;
	columne = 0;
	fileptr = edi;			// for seeking current cursor pos
	CALL(CountColToLineBeginVis,77);// i.e. expanding TABs
	ebx = columns;
	if (eax<ebx) goto DispShortLine;
	eax-=ebx;
	zloffst = eax+1;
DispShortLine:CALL(LookPgBegin,78);	// go on 1st char upper left on screen
	esi = edi;			// esi for reading chars from text
	ecx = lines;
	extra2 = -1;			// first line
	if (!ecx) goto KeyKXend;	// window appears too small
DispNewLine:++extra2;			// new line
	edi = (long)&screenline;	// line display buffer
	edx=0;				// reset char counter
	ebx = 0;				// reset screen column to 0
#ifdef LESSWRITEOPS
	CALL(SetColor2,79);		// set initial character color per each line
#endif	
DispCharLoop:if (esi != fileptr) goto DispCharL1;// display char @ cursor postion ?
	if (tabcnt) goto DispCharL1;
	kurspos = extra2<<8|ebx;
	columne = ebx+ zloffst;		// chars scrolled left hidden
#ifdef CURSORMGNT
	parm=1;
	CALL(SetInverseStatus,80);
	if (!extra) goto DispEndLine;
#endif	
DispCharL1:CALL(SetColor,81);		// set color if neccessary
//------
DispEndLine:if ((unsigned long)esi>(unsigned long)ebp) goto FillLine;	// we have passed EOF, so now fill rest of screen
	if (!tabcnt) goto ELZ;
	--tabcnt;
	goto ELZ2;
ELZ:	if (esi != (long)ebp) goto ELZ6;
	++esi;				// set esi>ebp will later trigger  "ja FillLine"
	goto ELZ2;

ELZ6:	eax=*(unsigned char*)esi++;
	if (eax != TABCHAR) goto ELZ3;
	tabcnt = TAB - edx % TAB - 1;	// spaces for Tab  and count out the tab char itself
ELZ2:	eax = SPACECHAR;
ELZ3:	if (eax==NEWLINE) goto FillLine;
	if (eax<SPACECHAR || eax== 0x7f) 
	  eax = '.';	// simply ignore chars like carriage_return etc.
	if (ebx>=columns) goto DispEndLine;	// screen width
	++edx;				// also count hidden chars (left margin)
	if (edx<=zloffst) goto ELZ5;	// load new char (but no display)
	*(unsigned char*)edi++=eax;
	
#ifdef CURSORMGNT
	parm=0;
	CALL(SetInverseStatus,83);
#endif	
	++ebx;				// counts displayed chars only
ELZ5:	goto DispCharLoop;
//------
FillLine:PUSH(ecx);			// continue rest of line
	ecx = columns;			// width
	ecx-=ebx;
	if (ecx)
	{
	 if (inverse-1) goto FillLine1;	// special cursor attribute?
	 *(unsigned char*)edi++=SPACECHAR;	// only 1st char with special attribute
#ifdef CURSORMGNT
	 parm=0;
	 CALL(SetInverseStatus,84);
#endif
	 if (!--ecx) goto FillLine2;	// one char less
FillLine1:do {
	 *(unsigned char*)edi++=SPACECHAR;	// store the rest blanks
	 } while (--ecx);
	}
FillLine2:POP(ecx);
	*(unsigned char*)edi = 0;
	CALL(ScreenLineShow,85);
	if (--ecx) goto DispNewLine;
	edi = fileptr;			// restore text pointer
	goto RestKursPos;
//---------------------------------------------------------------------
//  three helper subroutines called by DispNewScreen 
//  dealing ESC sequences for character attributes
//  expecting  parm 0|1
//  returning extra
//
SetInverseStatus:PUSH(ecx);		// returns zero flag
	PUSH(esi);
	if (!parm) goto SIS1;
	if (insstat-1) 
	{
	 extra=1;
	 goto SIS4;
	}
	inverse = 1;
	esi = (long)preversevideoX;
	esi+=revvoff;			// switch between esc seq for linux or Xterm
	goto SIS2;
SIS1:	if (inverse-1) goto SIS3;
	inverse = 0;
SIS6:	isbold = 0;
SIS5:	esi = (long)pbold0;
SIS2:	ecx = boldlen;
	do {
	*(unsigned char*)edi++=*(unsigned char*)esi++;
	} while (--ecx);
SIS3:	extra=0;
SIS4:	POP(esi);
	POP(ecx);
	RETURN;
//------
SetColor:PUSH(ecx);			// expects extra-flag:bold  else normal
	PUSH(esi);
	CALL(IsShowBlock,86);
	if (!extra) goto SCEsc1;
	if (isbold==1) goto SIS4;	// never set bold if it is already bold
	isbold = 1;
SCEsc2:	esi = (long)pbold1;
	goto SIS2;
SCEsc1:	if (!isbold) goto SIS4;		// ditto
	goto SIS6;
//------
#ifdef LESSWRITEOPS
SetColor2:PUSH(ecx);
	PUSH(esi);
	CALL(IsShowBlock,87);
	if (!extra) goto SIS5;
	goto SCEsc2;
#endif
//------
//  a little helper for SetColor* functions
IsShowBlock:if (!showblock || !blockbegin || (unsigned long)blockbegin>(unsigned long)esi) goto SBlock;
	if ((unsigned long)esi<(unsigned long)blockende) goto SB_ret;
SBlock:	extra=0;
	RETURN;
SB_ret:	extra=1;
	RETURN;
//------
//  this helper for DispNewScreen checks screen size before writing on screen
//  FIXME: adjusting edit screen resize works with xterm, but not with SVGATextMode
//
GetEditScreenSize:
	ecx = TIOCGWINSZ;
	edx = (long)&winsize;
	CALL(IOctlTerminal,88);
	eax = *(long*)edx;		// each 16 bit lines,columns
	if (!eax)  eax = 0x00500018;	// i.e. (80<<16)+24  (assume 80x24)
	lines = (eax-1)&0xFF;		// without status line
#if 1
	// this is not portable: maybe 32bit Linux plus some Un*x systems ? 
	columns = eax >> 16;		// columns > 255 are ignored...
#else
	columns = 80;
#endif
	RETURN;
//---------------------------------------------------------------------
//
//  LOWER LEVEL screen acces function (main +2 helpers)
//  this function does write the line buffer to screen i.e. terminal
//
//  at first 2 special entry points:
WriteTwo:*(long*)(&screenline) = ecx;
StatusLineShow:ecx=0;			// 0 for last line
ScreenLineShow:PUSH(eax);
	PUSH(ecx);
	PUSH(edx);
	PUSH(ebx);
	PUSH((long)ebp);
	PUSH(esi);			// expecting in ecx screen line counted from 0
	PUSH(edi);
#ifdef LESSWRITEOPS
	eax = (columns+32)*ecx;		// estimated max ESC sequences extra bytes (i.e. boldlen*X)
	ebx = 0;			// flag
	edi = eax+(long)screenbuffer;
#endif
	edx=0;
	esi = (long)&screenline;
sl3:	eax=*(unsigned char*)esi++;
	++edx;				// count message length to write
#ifdef LESSWRITEOPS
	if ((unsigned long)edi>=(unsigned long)(&screenbuffer_end)) goto sl5;	// never read/write beyond buffer
	if (eax == *(unsigned char*)edi) goto sl4;
	*(unsigned char*)edi = eax;
sl5:	++ebx;				// set flag whether line need redrawing
sl4:	++edi;
#endif
	if (eax) goto sl3;
	--edx;				// one too much
#ifdef LESSWRITEOPS
	if (!ebx) goto NoWrite;		// redraw ? ..no: quick exit
#endif
	PUSH(edx);
	edx=(lines-ecx)<<8;
	CALL(sys_writeKP,89);		// set cursor pos before writing the line
	POP(edx);
	PUSH(ecx);
	eax = (long)&screencolors1;	// set bold yellow on blue
	CALL(sys_writeSLColors,90);	// special for status line (ecx==0)
	ecx = (long)&screenline;		// second argument: integer to message to write
	CALL(WriteFile0,91);
	POP(ecx);
	eax = (long)&screencolors0;	// reset to b/w
	CALL(sys_writeSLColors,92);	// special for status line (ecx==0)
	edx = kurspos2;
	CALL(sys_writeKP,93);		// restore old cursor pos
NoWrite:POP(edi);
	POP(esi);
	POP(ebp);
	POP(ebx);
	POP(edx);
	POP(ecx);
	POP(eax);
	RETURN;
//------
//  a helper for ScreenLineShow
//
sys_writeSLColors:
	if (!ecx)			// do nothing if not in status line
	{
	PUSH(eax);
	PUSH(ecx);
	PUSH(edx);
	PUSH(ebx);
	ecx=eax;			// parameter points to ESC-xxx color string
	edx = scolorslen;
	CALL(WriteFile0,94);
	POP(ebx);
	POP(edx);
	POP(ecx);
	POP(eax);
	}
	RETURN;
//---------------------------------------------------------------------
//
//  getting line INPUT from terminal / UNDER CONSTRUCTION
//
//  expecting integer to message text in esi
//
InputStringWithMessage:
	CALL(WriteMess9MakeLine,95);
	ecx = (long)&optbuffer;
	edx = optslen;
	goto InputString;
InputString0:CALL(WriteMess9MakeLine,96);
	edx = maxfilenamelen;
//
//  expecting input line buffer in ecx
//  expecting max count byte in edx
//  return length in eax, CY for empty string (or abort via ^U)
//
InputString:PUSH(ecx);
	PUSH(edi);
	PUSH(kurspos2);
	ebx = columns-stdtxtlen;
	if (edx<ebx) goto IS8;		// TODO enable some scrolling: now truncate at end of line
	edx = ebx;
IS8:	ebx=0;
	edi = ecx;
IS0:	PUSH(ebx);
	PUSH(edx);
	kurspos2 = ebx;			// column
	kurspos2+=stdtxtlen;		// offset
	ebx = lines;			// line#
	((unsigned char*)&kurspos2)[1] = ebx;
#ifdef LESSWRITEOPS	
	screenbuffer[0] = 0;		// switching off usage of buffer v0.7
#endif	
	CALL(StatusLineShow,97);
	CALL(GetChar,98);
	POP(edx);
	POP(ebx);
	if (eax!=0x15) goto IS9;	// ^U abort (WStar)
	ebx=0;				// length 0 triggers CY flag
	goto IS1;
IS9:	if (eax==NEWLINE) goto IS1;
	if (eax-8) goto IS2;		// ^H (translated DEL)
	if (!ebx) goto IS0;		// @left border?
	--ebx;
	--edi;
	screenline[(ebx+stdtxtlen)] = SPACECHAR;
	goto IS0;
//------
IS2:	if (eax<SPACECHAR) goto IS0;
	*(unsigned char*)edi++=eax;
	screenline[(ebx+stdtxtlen)] = eax;
	++ebx;
	if (ebx<edx) goto IS0;
//------
IS1:	*(unsigned char*)edi=0;		// make asciz string
	POP(kurspos2);
	POP(edi);
	POP(ecx);
	eax=ebx;
	extra=!eax;			// set extra-flag if empty string
	RETURN;
//---------
//
//  GetChar returns ZERO flag for non ASCII (checked in HandleChar)
//  (vt300 is _NOT_ supported in asm version)
//
ReadChar:veryold = old;			// for ^QP
	old=edi;
GetChar:CALL(ReadOneChar,99);
	if (eax==0x7f)			// special case: remap DEL to Ctrl-H
	  eax = 8;
	if (eax-27) goto RCready_2; 	// ESC ?
	CALL(ReadOneChar,100);		// dont care whether '[' or 'O' (should be) or any else
	CALL(ReadOneChar,101);		// e.g.  [ for vt220,rxvt family  O for xterm,vt100 family
	ebx = 0x47-lowest;		// 47h home - the lowest
	if (eax=='1') goto RC_Tilde;
	if (eax=='7') goto RC_Tilde;	// (on rxvt)
	if (eax=='H') goto RCready;	// (on xterm)
	if (eax=='w') goto RCready;	// (on vt300)
	++ebx;				// 48h up
	if (eax=='A') goto RCready;
	if (eax=='x') goto RCready;	// (on vt300)
	++ebx;				// 49h PgUp
	if (eax=='5') goto RC_Tilde;
	if (eax=='I') goto RCready;
	if (eax=='y') goto RCready;	// (on vt300)
	++ebx;
	++ebx;				// 4Bh left
	if (eax=='D') goto RCready;
	if (eax=='t') goto RCready;	// (on vt300)
	++ebx;
	++ebx;				// 4Dh right
	if (eax=='C') goto RCready;
	if (eax=='v') goto RCready;	// (on vt300)
	++ebx;
	++ebx;				// 4Fh end
	if (eax=='4') goto RC_Tilde;
	if (eax=='8') goto RC_Tilde;	// (on rxvt only)
	if (eax=='F') goto RCready;	// (on xterm only)
	if (eax=='q') goto RCready;	// (on vt300)
	++ebx;				// 50h down
	if (eax=='B') goto RCready;
	if (eax=='r') goto RCready;	// (on vt300)
	++ebx;				// 51h PgDn
	if (eax=='6') goto RC_Tilde;
	if (eax=='G') goto RCready;
	if (eax=='s') goto RCready;	// (on vt300)
	++ebx;				// 52h insert
	if (eax=='2') goto RC_Tilde;
	if (eax=='L') goto RCready;
	if (eax=='p') goto RCready;
	++ebx;				// 53h del
	if (eax=='l') goto RCready;
	if (eax-'3') goto RCready_2;	// slightly ignore this chars
RC_Tilde:PUSH(ebx);
	CALL(ReadOneChar,102);		// read another ~ (after Integer 1..8)
	POP(ebx);
	if (eax-'~') goto GetChar;	// could be F7,sf1 etc on linuxterm: ignored
//------
RCready:eax =0;
RCready_2:RETURN;
//-----------------------------------
//  called by ReadChar/GetChar
//
ReadOneChar:CALL(ReadFile0,105);
	eax = *(unsigned char*)ecx;
	RETURN;
//---------------------------------------------------------------------
//
//  L O O K functions
//  search special text locations and set register edi to
//
LookBackward:			// set EDI to 1 before EOL (0Ah) i.e., 2 before start of next line
	PUSH(ecx);
	PUSH(ebx);
	ebx=0;
	if (((unsigned char*)edi)[-1] == NEWLINE) goto LBa3;	// at BOL
	if (*(unsigned char*)edi != NEWLINE) goto LBa1;	// at EOL ?
	--edi;				// at EOL ? start search 1 char earlier
	++ebx;				// increase counter
LBa1:	ecx = 99999;
	while (--ecx && *(unsigned char*)edi-- != NEWLINE);
	eax = ebx+99997-ecx;
LBa5:	POP(ebx);
	POP(ecx);
	goto CheckBof;
//------
LBa3:	eax=0;
	--edi;
	--edi;
	goto LBa5;
//------
LookForward:PUSH(ecx);			// don't touch edx (if called by BZNLoop only)
	ecx = 99999;
	while (--ecx && *(unsigned char*)edi++ != NEWLINE);
	eax = 99998-ecx;
	POP(ecx);
	--edi;
CheckEof:extra=0;
	if ((unsigned long)edi>=(unsigned long) ebp) extra++;
	goto CheckENum;
CheckBof:extra=0;
	if ((unsigned long)edi>=(unsigned long)&text) goto CheckEnd;
	extra=1;
CheckENum:numeriere = 1;		// if bof
CheckEnd:RETURN;
//------
LookPgBegin:edx = kurspos2;		// called by DispNewScreen to get sync with 1st char on screen
	ecx= edx>>8;			// called by KeyCtrlQE  (go upper left)
	++ecx;
	goto LookPU2;
//------
LookPgEnd:edx = kurspos2;		// goes 1st char last line on screen
	ecx = lines;
	ecx-= (edx>>8);
	goto LookPD2;
//------
LookLineUp:ecx=2;			// 2 lines: THIS line and line BEFORE
	--linenr;
	goto LookPU2;
//------
LookLineDown:ecx=2;			// 2 lines: THIS and NEXT line
	++linenr;
	goto LookPD2;
//------
LookPageUp:ecx = lines;
	linenr-=(ecx-1);		// PgUp,PgDown one line less
LookPU2:CALL(LookBackward,106);
	if (extra) goto LookPUEnd;
	++edi;
	if (--ecx) goto LookPU2;	// after loop edi points to char left of 0ah
	--edi;
LookPUEnd:++edi;
	++edi;				// now points to 1st char on screen or line
	RETURN;
//------
LookPgDown:ecx = lines;
	linenr+=(ecx-1);		// PgUp,PgDown one line less
LookPD2:CALL(LookForward,107);
	if (extra) goto LookPDEnd;
	++edi;				// 1st char next line
	if (--ecx) goto LookPD2;
	--edi;				// last char last line
LookPDEnd:edi-=eax;			// 1st char last line
	RETURN;
//---------------------------------------------------------------------
//
//  some more CHECK functions
//
CheckBlock:esi = blockbegin;		// side effect esi points to block begin
	extra=0;
	if (!showblock || ((unsigned long)blockende <(unsigned long) sot) || 
		(unsigned long)esi <(unsigned long) sot || 
		(unsigned long)blockende < (unsigned long)esi)
	  extra++;
	RETURN;
//------
CheckMode:extra=0;			// checks for INSERT status
	if (*(unsigned char*)edi == NEWLINE) extra++;
	if (insstat==1) extra++;
	RETURN;
//---------------------------------------------------------------------
//
//  C O U N T  functions
//  to return number of chars up to some place
//  (all of them are wrappers of Look....functions anyway)
//
CountToLineEnd:PUSH(edi);
	CALL(LookForward,108);
	POP(edi);
	RETURN;				// eax=chars up to line end
//------
CountColToLineBeginVis:			// counts columns represented by chars in EAX
	CALL(CountToLineBegin,109);	// i.e. EXPAND any TAB chars found
	PUSH(esi);
	edx=0;
	esi = edi - eax -1;			// startpoint to bol
CCV1:	++esi;
	if (esi>=edi) goto CCVend;
	if (*(unsigned char*)esi == TABCHAR) goto CCVTab;
	++edx;				// count visible chars
	goto CCV1;
CCVTab:	edx += (TAB -edx % TAB);
	goto CCV1;
CCVend:	ch2linebeg = edx;		// ch2linebeg: interface to Key... functions
	eax = edx;			// eax: interface to DispNewScreen
	POP(esi);
	RETURN;
//------
CountToLineBegin:PUSH(edi);		// output eax=chars up there
	CALL(LookBackward,111);
	esi = edi;			// side effect: set edi to 1st char in line
	POP(edi);
	RETURN;
//------
CountToWordBegin:			// output eax=chars up there
	esi = edi;
CountNLoop:++esi;
	if (*(unsigned char*)esi == NEWLINE) goto CTWend;
	if (*(unsigned char*)esi <= SPACECHAR) goto CountNLoop;	// below SPACE includes tab chars
	if (((unsigned char*)esi)[-1] > 0x2f) goto CountNLoop;
CTWend:	eax = esi-edi;			// maybe =0
	RETURN;
//--------------------------------------------------------------------
//
//  some CURSOR control functions
//
GoUp:	eax = 0;
	ebx = -1;
	goto UpDown;
GoDown:	eax = lines-1;
	ebx = 1;
UpDown:	edx = kurspos2;			// former was call getkurspos
	if ((edx>>8)==eax) goto Goret;
	edx+=ebx==1?256:-256;		// ONLY here we change curent line of cursor
	goto SetKursPos;
Goret:	RETURN;
//------
//  set cursor to some desired places
//
KursorFirstLine:edx=0;
	goto SetKursPos;
KursorLastLine:	edx = (lines-1)<<8;
	goto SetKursPos;
KursorStatusLine:edx = stdtxtlen+(lines<<8);
	goto SetKursPos;
RestKursPos:edx = kurspos;
SetKursPos:kurspos2 = edx;		// saves reading cursor pos   (0,0)
sys_writeKP:PUSH(eax);
	PUSH(ecx);
	PUSH(edx);
	PUSH(ebx);
	PUSH(esi);
	PUSH(edi);
	CALL(make_KPstr,112);
	ecx = (long)&setkp;		// second argument: integer to message to write
	edx = setkplen;			// third argument: message length
	CALL(WriteFile0,113);
	POP(edi);
	POP(esi);
	POP(ebx);
	POP(edx);
	POP(ecx);
	POP(eax);
	RETURN;
//------
//  make ESC sequence appropriate to most important terminals
//
make_KPstr:edx+=0x101;
//	++dl;			// expecting cursor pos in dh/dl (0,0)
//	++dh;				// both line (dh) col (dl) are counted now from 1
	edi = (long)&setkp;		// build cursor control esc string db 27,'[000;000H'
	*(unsigned char*)edi++=0x1b;	// init memory
	*(unsigned char*)edi++= '[';
	*(unsigned char*)edi++= '0';
	*(unsigned char*)edi++= '0';
	*(unsigned char*)edi++= '0';
	*(unsigned char*)edi++= ';';
	*(unsigned char*)edi++= '0';
	*(unsigned char*)edi++= '0';
	*(unsigned char*)edi++= '0';
	*(unsigned char*)edi++= 'H';
	edi = (long)&(setkp[1+3]);	// line end
	eax= edx>>8;			// DH=line
	PUSH(edx);
	CALL(IntegerToAscii,114);	// make number string
	POP(edx);
	edi = (long)&(setkp[1+3+4]);	// column end
	eax= edx&0xFF;			// DL=col
//------continued...
//  a general helper
//    expects int# in eax
IntegerToAscii:
Connum1:edx= eax%10;
	eax/=10;
	edx&=0xf;
	edx+='0';
	*(unsigned char*)edi--=edx;
	if (eax) goto Connum1;
	RETURN;
//---------------------------------------------------------------------
//
//  functions for INSERTING, COPYING and DELETING chars in text
//
InsertByte:if (!eax) goto Ins2;		// input: eax = # of bytes  edi = ptr where
	ecx = maxlen;			// max_len+offset-eofptr=freespaceecx
	ecx+=(long)(&text[1]);
	ecx-=(long)ebp;
	if ((unsigned long)ecx>=(unsigned long)eax) goto SpaceAva;	// cmp freespace - newbytes
	errno=105;
	CALL(OSerror,115);
	CALL(RestKursPos,116);
	extra=1;
	RETURN;
SpaceAva:PUSH(edi);
	esi = (long)ebp;		// end of text movsb-source
	ecx = (long)ebp+1-edi;		// space count: distance between eof and current position
	edi = (long)ebp+eax;		// movsb-destination
	do {
	*(unsigned char*)edi--=*(unsigned char*)esi--;
	} while (--ecx);

Ins0:	POP(edi);			// here is the jmp destination from DeleteByte
//------
	changed = CHANGED;
//	(long)ebp+=eax;
	ebp=(void *)((long)ebp+eax);
	if ((unsigned long)edi>=(unsigned long)blockende) goto Ins1;
	blockende+=eax;
Ins1:	if ((unsigned long)edi>=(unsigned long)blockbegin) goto Ins2;
	blockbegin+=eax;
Ins2:	extra=0;
	RETURN;				// output:nc=ok/cy=bad /ecx=0/ eax inserted / -eax deleted
//------
DeleteByteCheckMarker:			// edi points to begin
	ebx = edi+eax;			// ebx points to end
	edx = blockbegin;
	if (!((unsigned long)edi>(unsigned long)edx || 
		(unsigned long)ebx<(unsigned long)edx)) blockbegin = edi;
	edx = blockende;
	if (!((unsigned long)edi>(unsigned long)edx || 
		(unsigned long)ebx<(unsigned long)edx)) blockende = edi;
DeleteByte:if (!eax) goto MoveBlEnd;	// input in eax
	PUSH(edi);
	esi = edi+eax;			// current + x chars
	ecx = (long)ebp-esi;
	ecx++;
	do {
	*(unsigned char*)edi++=*(unsigned char*)esi++;
	} while (--ecx);
	eax = -eax;			// "neg eax" is for continuing @InsertByte
	goto Ins0;			// pending "pop edi"
//------
CopyBlock:CALL(CheckBlock,119);		// copy block, called by ^KC, ^KV
	if (extra) 
	  goto MoveBlEnd;
	if ((unsigned long)edi>=(unsigned long)blockbegin && 
		(unsigned long)edi<(unsigned long)blockende) 
	{
	  extra=1;
	  goto MoveBlEnd;
	}  
	eax = blockende-esi;		// block len
	CALL(InsertByte,121);
	if (extra) goto MoveBlEnd;
	esi = blockbegin;
MoveBlock:PUSH(edi);			// input : esi=^KB edi=current
	if (eax)
	{
	 ecx = eax;
	 do {
	 *(unsigned char*)edi++=*(unsigned char*)esi++;
	 } while (--ecx);
	}
	POP(edi);
	extra=0;			// nocarry->ok
MoveBlEnd:RETURN;			// return eax=size
//---------------------------------------------------------------------
//
//  functions reading/writing  text or blocks  from/into  files
//
NewFile:CALL(InitVars,122);
	if (!esi) goto NFnoarg;
	edi = (long)&filepath;
NF1:	eax=*(unsigned char*)esi++;
	*(unsigned char*)edi++=eax;
	if (eax) goto NF1;
	goto GetFile;
NFnoarg:ebx = (long)&helpfile;		// load file with some help text (one page)
	CALL(OpenFile0,123);
	edi = (long)sot;
	ebp = (unsigned char*)edi;
	if (eax<0) goto GF0;
	ebx=eax;
	CALL(OldFile1,124);
GF0:	CALL(DispNewScreen,125);	// if not available: clear screen only
//------
	esi = (long)&filename;
	ecx = (long)&filepath;
	CALL(InputString0,126);
	if (extra) goto NFEnd2;		// empty string not allowed here
//------
GetFile:ebx = (long)&filepath;
	CALL(OpenFile0,127);
	edi = (long)sot;
	ebp = (unsigned char*)edi;
	if (eax<0) goto NewFileEnd;
//
// In Linux/asm version here we have code for
// calculating and allocating memory: twice filesize ...
// ....plus 102400 byte reserve space for inserts.
// Currently we have hard coded 'maxlen' sized buffer.
// For FreeBSD/asm version memory always is hard coded by maxlen
//
	ebx = eax;			
//------
	CALL(Fstat,130);
	if (eax<0) goto OSerror;
	perms = fstatbuf.st_mode &  0777;
	uid = fstatbuf.st_uid;
	gid = fstatbuf.st_gid;
//------
OldFile1:edx = maxlen;			// i.e. either 'max' or filesize twice
	ecx = edi;			// sot
	CALL(ReadFile,131);
	if (eax<0) goto OSerror;
	edx=eax;			// mov edx,eax	bytes read
	CALL(CloseFile,132);
	if (eax<0) goto OSerror;
	ebp = edx+sot;			// eof_ptr=filesize+start_of_text
NewFileEnd:*(unsigned char*)ebp = NEWLINE;	// eof-marker
	extra=0;
NFEnd2:	RETURN;
//------
//   save file (called by ^KS,^KX)
//
SaveFile:extra=0;
	if (changed == UNCHANGED) goto NFEnd2;
	esi = (long)&filesave;
	CALL(WriteMess9,133);
//------
	ecx = eax = (long)&bakpath;
	if (*(int*)eax!=1886221359)	// old was ..... 'pmt/')
	{
	 ebx = esi = (long)&filepath;
	 while (	(*(unsigned char*)eax++= *(unsigned char*)esi++) ) ;
	 eax--;
	 *(unsigned char*)eax++='~';	// add BAK file extension
	 *(unsigned char*)eax++=0;
	 CALL(RenameFile,134);		// ecx is filepath
	}
	ecx = (O_WRONLY_CREAT_TRUNC);
	edx = perms;
	CALL(OpenFile,135);
	if (eax<0) goto OSerror;
	ebx=eax;			// file descriptor
	edx = gid;
	ecx = uid;
	CALL(ChownFile,137);
//------	
	ecx = (long)sot;		// ecx=bof
	edx = (long)ebp;		// eof
SaveFile2:edx-=ecx;			// edx=fileesize= eof-bof
	CALL(WriteFile,138);		// ebx=file descriptor
	if (eax<0) goto OSerror;
	errno = 5;			// just in case of....
	if (eax-edx) goto OSerror;	// all written?
	CALL(CloseFile,139);
	if (eax<0) goto OSerror;
	RETURN;
//------
//   save block (called by ^KW)
//
SaveBlock:CALL(GetBlockName,140);
	if (extra) goto DE2;
	ecx = (O_WRONLY_CREAT_TRUNC);
	ebx = (long)&blockpath;
	edx = (PERMS);
	CALL(OpenFile,141);
	if (eax<0) goto OSerror;
	ecx = esi;			// = block begin
	edx = blockende;
	ebx=eax;			// file descriptor  (xchg is only 1 byte)
	goto SaveFile2;
//------
//  read a block into buffer (by ^KR)
//
ReadBlock:CALL(GetBlockName,142);
	if (extra) goto DE2;
	ebx = (long)&blockpath;
	CALL(OpenFile0,143);
	if (eax<0) goto OSerror;
	CALL(Seek,144);
	if (eax<0) goto OSerror;
	PUSH(eax);			// eax=fileesize
	CALL(InsertByte,145);
	POP(edx);			// file size
	if (extra) goto RB_ex;		// ret if cy InsertByte told an error message itself
	ecx = edi;			// ^offset akt ptr
	CALL(ReadFile,146);
	if (eax<0) goto preOSerror;	// to delete inserted bytes (# in EDX)
	ecx = eax;			// bytes read
	CALL(CloseFile,147);
	if (eax<0) goto OSerror;
	errno = 5;			// just in case of....
	if (edx!=ecx) goto OSerror;	// all read?
	extra=0;
RB_ex:	RETURN;
//------
preOSerror:eax = edx;			// count bytes
	CALL(DeleteByte,148);		// delete space reserved for insertation
OSerror:PUSH(edi);
	edi = (long)&error[8];		// where to store ASCII value of errno
	eax = errno;
	PUSH(eax);
	CALL(IntegerToAscii,149);
	POP(ecx);			// for getting error text via LookPD2
	edi = (long)&errmsgs;
	if (!*(unsigned char*)edi) goto DE0;// are text error messages loaded ?
	CALL(LookPD2,150);		// look message # ecx in line number #
	esi = edi;
	edi = (long)&error[9];
	*(unsigned char*)edi++=' ';
	*(unsigned char*)edi++=':';
	ecx = 80;			// max strlen / compare errlen equ 100
	do {
	*(unsigned char*)edi++=*(unsigned char*)esi++;
	} while (--ecx);
DE0:	esi = (long)&error;
	POP(edi);
DE1:	CALL(WriteMess9,151);
	CALL(GetChar,152);
DE2:	// continued...
	//---------------------------------------------------------------------
	//  more STATUS LINE maintaining subroutines
RestoreStatusLine:
	PUSH(eax);
	PUSH(ecx);
	PUSH(edx);
	PUSH(ebx);
	PUSH((long)ebp);
	PUSH(esi);
	PUSH(edi);			// important e.g. for asksave
	CALL(InitStatusLine,153);
	ecx = columns;			// width
	if (ecx < (stdtxtlen+1 +5+2)) goto RSno_lineNr;	// this window is too small
	screenline[1] = changed;
	ebx = (int)542330441;		//(' SNI');		// Insert
	if (insstat==1) goto RSL1;
	ebx = (int)542266959;		//(' RVO');		// Overwrite
RSL1:	
	*((unsigned char*)&(screenline[4])) = (char)ebx;
	ebx >>=8;
	*((unsigned char*)&(screenline[5])) = (char)ebx;
	ebx >>=8;
	*((unsigned char*)&(screenline[6])) = (char)ebx;
	ebx >>=8;	
	*((unsigned char*)&(screenline[7])) = (char)ebx;
	edi = (long)(screenline+stdtxtlen);
	ecx-=(stdtxtlen+1+5);		// space for other than filename
	esi = (long)&filepath;
RSL2:	eax=*(unsigned char*)esi++;
	if (!eax) goto RSL4;
	*(unsigned char*)edi++=eax;
	if (--ecx) goto RSL2;
RSL4:	edi = (long)&(screenline[-2]);
	edi+=columns;
	eax = columne;
	++eax;				// start with 1
	CALL(IntegerToAscii,154);
	*(unsigned char*)edi-- = (':');	// delimiter ROW:COL
	eax = linenr;
	CALL(IntegerToAscii,155);
RSno_lineNr:CALL(StatusLineShow,156);	// now write all at once
	POP(edi);
	POP(esi);
	POP(ebp);
	POP(ebx);
	POP(edx);
	POP(ecx);
	POP(eax);
	extra=1;			// error status only important if called via OSError
	RETURN;
//------
//
//  write an answer prompt into status line
//  (with and without re-initialisation)
//  expecting esi points to ASCIIZ or 0A terminated string
//
WriteMess9MakeLine:CALL(InitStatusLine,157);
WriteMess9:PUSH(eax);
	PUSH(ecx);
	PUSH(edx);
	PUSH(ebx);
	PUSH((long)ebp);
	PUSH(esi);
	PUSH(edi);
	edi = (long)&screenline;
	while(	(eax=*(unsigned char*)esi++))
	{
	 if (eax == 0xa) break;		// 0xa is for error messages
	 *(unsigned char*)edi++=eax;
	}	
	CALL(StatusLineShow,158);
	POP(edi);
	POP(esi);
	POP(ebp);
	POP(ebx);
	POP(edx);
	POP(ecx);
	POP(eax);
	goto KursorStatusLine;
//------
//  a helper for other status line functions:
//  simply init an empty line  
//
InitStatusLine:
	PUSH(ecx);
	PUSH(edi);
	edi = (long)&screenline;
	ecx = columns-1;		// -1 for cygwin
	do {
	*(unsigned char*)edi++=SPACECHAR;
	} while (--ecx);
	*(unsigned char*)edi=0;		// prepare ASCIIZ string
	POP(edi);
	POP(ecx);
	RETURN;
//------
//  read a file name for block operations
//  expecting message text ptr in esi
//
GetBlockName:PUSH(eax);
	PUSH(ecx);
	PUSH(edx);
	PUSH(ebx);
	PUSH(esi);
	PUSH(edi);
	esi = (long)&block;
	ecx = (long)&blockpath;
	CALL(InputString0,159);		// cy if empty string
	PUSH(extra);
	CALL(RestKursPos,160);
	POP(extra);
	POP(edi);
	POP(esi);
	POP(ebx);
	POP(edx);
	POP(ecx);
	POP(eax);
	RETURN;
//------
//  helper for NewFile
//
InitVars:*(unsigned char*)&text = NEWLINE;	// don't touch esi!
	changed = UNCHANGED;
	oldQFpos = 0;
	bereitsges = 0;
	blockbegin = 0;
	blockende = 0;
	endeedit = 0;
	old = (long)sot;
	linenr = 1;
	showblock = 1;
	insstat = 1;
	maxlen = max;
	*(int*)(&error) = (int)1330795077;	//('ORRE');
	*(int*)((unsigned char*)(&error[4])) = (int)538976338;//('   R');
	perms = (PERMS);
	gid = -1;			//   -1 i.e. no changes in fchown
	uid = -1;
	RETURN;
//------
ReadResource:
	ebx = (long)&resfile;		// load file with some error message text
	CALL(OpenFile0,161);		// don't care about errors
	if (eax>=0)
	{
	 ebx=eax;			// mov file_descriptor to ebx
	 edx = errbufsize;
	 ecx = (long)&errmsgs;
	 CALL(ReadFile,162);
	 if (eax>=0) goto CloseFile;
	} 
	RETURN;
//------
Seek:	ebx=eax;			// mov file_descriptor to ebx
	edx=2;
	CALL(SeekFile,165);		// end
	if (eax>=0)
	{
	 edx=0;
	 PUSH(eax);
	 CALL(SeekFile,166);
	 POP(eax);
	}
	RETURN;
//---------------------------------------------------------------------
//
//  FIND/REPLACE related stuff
//
AskForReplace:esi = (long)&askreplace1;
	ecx = (long)&suchtext;
	CALL(InputString0,169);
	if (extra) goto AskFor_Ex;
	suchlaenge = eax;
	esi = (long)&askreplace2;
	ecx = (long)&replacetext;
	CALL(InputString0,170);
	goto GetOptions;
AskForFind:esi = (long)&askfind;
	ecx = (long)&suchtext;
	CALL(InputString0,171);
	if (extra) goto AskFor_Ex;
GetOptions:repllaenge = eax;
	esi = (long)&optiontext;
	CALL(InputStringWithMessage,172);// empty string is allowd for std options...
	CALL(ParseOptions,173);		// ...(set in ParseOptions)
	extra=0;
AskFor_Ex:if (!extra) goto AFE2;
	bereitsges = 0;
AFE2:	PUSH(extra);
	CALL(RestoreStatusLine,174);
	CALL(RestKursPos,175);
	POP(extra);
	RETURN;
//------
//  check string for 2 possible options: C,c,B,b  (case sensitive & backward)  
//  
ParseOptions:
	ebx = (long)&optbuffer;
	vorwarts=1;
	grossklein = 0xdf;
	do {
	eax=*(unsigned char*)ebx++ & 0x5f;	// upper case
	if (eax=='C') grossklein^=0x20;		// result 0dfh,   2*C is like not C option
	if (eax=='B') vorwarts = -vorwarts;	// similar 2*B is backward twice i.e. forward
	} while (eax);
	RETURN;
//------
//  the find subroutine itself
//
find2:	ebx = edi;
find3:	eax=*(unsigned char*)esi++;
	if (!eax) goto found;		// =end?
	if (eax>=0x41) eax&=grossklein;	// 0xff or 0xdf
	++edi;
	ecx = *(unsigned char*)edi;
	if (ecx>=0x41) ecx&=grossklein;	// 0xff or 0xdf
	if(eax==ecx) goto find3;
	edi = ebx;
FindText:edx = vorwarts;		// +1 or -1
	esi = (long)&suchtext;
	eax=*(unsigned char*)esi++;
	if (eax>=0x41) eax&=grossklein;	// 0xff or 0xdf
find1:	edi+=edx;			// +1/-1
	ecx = *(unsigned char*)edi;
	if (ecx>=0x41) ecx&=grossklein;	// 0xff or 0xdf
	if (eax==ecx) goto find2;
	if ((unsigned long)edi > (unsigned long)ebp) goto notfound;
	if ((unsigned long)edi >=(unsigned long)sot) goto find1;
notfound:extra=0;
	RETURN;
found:	edi = ebx;
	extra=1;			// edi points after location
	RETURN;
//---------------------------------------------------------------------
//
//  some GENERAL helper functions
//
GetAsciiToInteger:
	esi = (long)&asklineno;
	CALL(InputStringWithMessage,176);
	PUSH(extra);
	CALL(AskFor_Ex,177);		// repair status line & set cursor pos
	esi = ecx;			// optbuffer
	ecx=0;
	POP(extra);
	if (extra) goto AIexit;
AIload:	eax=*(unsigned char*)esi++ - '0';
	if (eax<0 || eax>9) goto AIexit;
	ecx *=10;
	ecx +=eax;
	goto AIload;
AIexit:	RETURN;				// ret ecx ,  ecx==0 if error
//---------------------------------------------------------------------
//  INTERFACE to OS kernel
//
ReadFile0:ebx=0;			// mov ebx,stdin
	edx=1;
	ecx = (long)&read_b;		// integer to buf
ReadFile:eax=read(ebx,(void*)ecx,edx);	// ebx file / ecx buffer / edx count byte
	RETURN;
WriteFile0:ebx=1;			// mov ebx,stdout
WriteFile:eax=write(ebx,(void*)ecx,edx);
	RETURN;
OpenFile0:ecx=0;			// i.e O_RDONLY
OpenFile:eax=open((char*)ebx,ecx,edx);
	RETURN;
CloseFile:eax=close(ebx);
	RETURN;
Fstat:	eax=fstat(ebx,&fstatbuf);
	RETURN;
RenameFile:eax=rename((unsigned char*)ebx,(unsigned char*)ecx);
	RETURN;
ChownFile:eax=fchown(ebx,ecx,edx);
	RETURN;
IOctlTerminal:eax=ioctl(0,ecx,edx);	// ECX TIOCGWINSZ ,EDX winsize structure ptr
	RETURN;
SeekFile:eax=lseek(ebx,0,edx);		// ecx offset / ebx file / edx method
	RETURN;
Exit:	_exit(0);	
//
//------new code for the C version:
SetTermStruc:tcgetattr(0,&orig);
	termios = orig;
	termios.c_lflag    &= (~ICANON & ~ECHO & ~ISIG);
	termios.c_iflag    &= (~IXON);
	termios.c_cc[VMIN]  = 1;
	termios.c_cc[VTIME] = 0;
	eax=(long)&termios;
SetTermStruc2:tcsetattr(0, TCSANOW, (struct termios*)eax);
	RETURN;
}
