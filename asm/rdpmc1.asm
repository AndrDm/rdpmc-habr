EUROASM AutoSegment=Yes, CPU=X64, SIMD=AVX2
rdpmc1 PROGRAM Format=PE, Width=64, Model=Flat, IconFile=, Entry=Start:

INCLUDE winscon.htm, winabi.htm, cpuext64.htm

Msg0 D "Instructions = ",0
Buffer DB 80 * B
   
Start: nop
	WinABI SetProcessAffinityMask, -1, 1 ; important!

	mov ecx, 0x40000000
	RDPMC
	shl rdx, 32
	or rax, rdx
	mov r10, rax
	cpuid
	mov ecx, 0x40000000
	RDPMC
	shl rdx, 32
	or rax, rdx
	sub rax, r10 ; subtract previous
	StoD Buffer
	StdOutput Msg0, Buffer, Eol=Yes, Console=Yes

	TerminateProgram

ENDPROGRAM rdpmc1
