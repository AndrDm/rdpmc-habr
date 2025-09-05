EUROASM AutoSegment=Yes, CPU=X64, SIMD=AVX2
rdtsc_sleep PROGRAM Format=PE, Width=64, Model=Flat, IconFile=, Entry=Start:

INCLUDE winscon.htm, winabi.htm, cpuext64.htm

Msg1 D "Hello, rdtsc",0
Msg2 D "RDTSC=",0
Buffer DB 80 * B
    
Start: nop ; Machine instruction tells €ASM to switch to [.text] (AutoSegment=Yes).
	StdOutput Msg1, Eol=Yes, Console=Yes

	rdtsc
	shl rdx, 32
	or rax, rdx
	mov r15, rax
	cpuid

	WinABI Sleep, 1000
	
	rdtscp
	shl rdx, 32
	or rax, rdx
	sub rax, r15

	StoD Buffer
	StdOutput Msg2, Buffer, Console=Yes

	TerminateProgram

ENDPROGRAM rdtsc_sleep
