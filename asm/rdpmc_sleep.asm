EUROASM AutoSegment=Yes, CPU=X64, SIMD=AVX2
rdpmc_sleep PROGRAM Format=PE, Width=64, Model=Flat, IconFile=, Entry=Start:

INCLUDE winscon.htm, winabi.htm, cpuext64.htm

Msg0 D "Instructions = ",0
Msg1 D "Core Cycles  = ",0
Msg2 D "Reference Cycles  = ",0

Buffer DB 80 * B

StartBench %MACRO Counter, Destination
	mov ecx, %Counter
	RDPMC
	shl rdx, 32
	or rax, rdx
	mov %Destination, rax	
%ENDMACRO StartBench

EndBench %MACRO Counter, Source, Message
	mov ecx, %Counter
	RDPMC
	shl rdx, 32
	or rax, rdx
	sub rax, %Source ; subtract previous
	Clear Buffer, 80
	StoD Buffer
	StdOutput %Message, Buffer, Eol=Yes, Console=Yes
%ENDMACRO EndBench
   
Start: nop
	WinABI SetProcessAffinityMask, -1, 1 ; important!

	StartBench 0x40000002, r10
	StartBench 0x40000001, r9
	StartBench 0x40000000, r8

	WinABI Sleep, 1000

	EndBench 0x40000000, r8, Msg0
	EndBench 0x40000001, r9, Msg1
	EndBench 0x40000002, r10, Msg2
     
	TerminateProgram

ENDPROGRAM rdpmc_sleep