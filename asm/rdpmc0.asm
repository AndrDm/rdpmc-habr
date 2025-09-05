EUROASM AutoSegment=Yes, CPU=X64, SIMD=AVX2
rdpmc0 PROGRAM Format=PE, Width=64, Model=Flat, IconFile=, Entry=Start:

INCLUDE winscon.htm, winabi.htm, cpuext64.htm

Msg0 D "Counter = ",0
Buffer DB 80 * B
    
Start: nop
	mov ecx, 0x40000000
	RDPMC
	shl rdx, 32
	or rax, rdx

	StoD Buffer
	StdOutput Msg0, Buffer, Eol=Yes, Console=Yes

	TerminateProgram

ENDPROGRAM rdpmc0
