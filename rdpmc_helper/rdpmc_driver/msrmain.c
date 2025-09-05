#include <ntddk.h>
#include <intrin.h>


VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
    UNREFERENCED_PARAMETER(DriverObject);
    DbgPrint("Driver unloaded\n");
}

VOID SetPCEFlag() {
    ULONG_PTR cr4;

    cr4 = __readcr4();               // Read CR4
    DbgPrint("CR4 before setting PCE: %p\n", (PVOID)cr4);

    cr4 |= 0x100;                   // Set bit 8 (PCE)
    __writecr4(cr4);                // Write back to CR4
    
    cr4 = __readcr4();
    DbgPrint("CR4 after setting PCE: %p\n", (PVOID)cr4);
}

VOID SetCR4PCE()
{
    ULONG_PTR cr4 = __readcr4();
    DbgPrint("PCE CR4 before setting PCE: %p\n", (PVOID)cr4);
    cr4 |= (1 << 8); // Set bit 8 (PCE)
    __writecr4(cr4);
    cr4 = __readcr4();
    DbgPrint("PCE CR4 after setting PCE: %p\n", (PVOID)cr4);
}

VOID SetCR4PCEOnAllCores()
{
    //KAFFINITY activeProcessors = KeQueryActiveProcessors();
    for (ULONG i = 0; i < KeQueryActiveProcessorCount(NULL); ++i)
    {
        GROUP_AFFINITY affinity = { 0 };
        affinity.Mask = (KAFFINITY)1 << i;
        affinity.Group = 0;

        GROUP_AFFINITY oldAffinity;
        KeSetSystemGroupAffinityThread(&affinity, &oldAffinity);

        SetCR4PCE();
        DbgPrint("PCE CR4 set for cpu %d\n", i);
        KeRevertToUserGroupAffinityThread(&oldAffinity);
    }
}


// second part
// pmu_enable_fixed_gp.c
// Windows kernel (Ring 0, C) code to enable:
//   - 3 fixed counters: INST_RETIRED.ANY, CPU_CLK_UNHALTED.CORE, CPU_CLK_UNHALTED.REF_TSC
//   - 4 general-purpose counters: L1D load hit/miss, L2 demand-data hit/miss
//
// Build with WDK; include in your driver project.
// Call PmuEnableFixedAndGpAllCPUs(...) from DriverEntry or an IOCTL.
// Call PmuDisableAllCPUs() to stop counters.
//
// References for MSRs, fields, and events:
// - Intel PMU programming (PerfEvtSel*, PerfGlobalCtrl, FixedCtrCtrl)  [Intel PMU guide / SDM]
// - RDPMC ECX encoding & CR4.PCE                                      [Intel SDM RDPMC chapter]
// - Events: MEM_LOAD_UOPS_RETIRED.* (0xD1), L2_RQSTS.* (0x24)          [Skylake/Comet Lake event lists]

#include <ntddk.h>
#include <intrin.h>

// ---------- MSR addresses ----------
#define IA32_PERFEVTSEL0        0x186u    // +n for sel1..sel3
#define IA32_PMC0               0x0C1u    // +n for pmc1..pmc3

#define IA32_FIXED_CTR_CTRL     0x38Du
#define IA32_PERF_GLOBAL_CTRL   0x38Fu

#define IA32_FIXED_CTR0         0x309u    // INST_RETIRED.ANY
#define IA32_FIXED_CTR1         0x30Au    // CPU_CLK_UNHALTED.CORE
#define IA32_FIXED_CTR2         0x30Bu    // CPU_CLK_UNHALTED.REF_TSC

// ---------- IA32_PERFEVTSELx bit fields ----------
#define EVSEL_USR               (1ull << 16)  // count in user mode (CPL>0)
#define EVSEL_OS                (1ull << 17)  // count in kernel mode (CPL==0)
#define EVSEL_ANYTHREAD         (1ull << 21)  // count both SMT threads (usually 0)
#define EVSEL_EN                (1ull << 22)  // enable

// ---------- IA32_FIXED_CTR_CTRL (4 bits per fixed counter) ----------
#define FIXED_EN_OS             (1ull << 0)
#define FIXED_EN_USR            (1ull << 1)
#define FIXED_ANY               (1ull << 2)   // usually 0
#define FIXED_PMI               (1ull << 3)   // PMI on overflow (0 unless you wire PMIs)

// ---------- CR4.PCE (bit 8) permits user-mode RDPMC when set ----------
#define CR4_PCE                 (1ull << 8)

// ---------- Events to program into GP counters (Skylake/Comet Lake client) ----------
// GP0: L1D load hit   -> MEM_LOAD_UOPS_RETIRED.L1_HIT   : Event 0xD1, Umask 0x01
// GP1: L1D load miss  -> MEM_LOAD_UOPS_RETIRED.L1_MISS  : Event 0xD1, Umask 0x08
// GP2: L2 DD hit      -> L2_RQSTS.DEMAND_DATA_RD_HIT    : Event 0x24, Umask 0x41
// GP3: L2 DD miss     -> L2_RQSTS.DEMAND_DATA_RD_MISS   : Event 0x24, Umask 0x21
typedef struct _EVENT_UMASK {
    UCHAR evt;
    UCHAR um;
} EVENT_UMASK;

static const EVENT_UMASK g_GpEventsL1[2] = {
    { 0xD1, 0x01 },  // GP0: L1D load hit
    { 0xD1, 0x08 },  // GP1: L1D load miss
};

static const EVENT_UMASK g_GpEvents[4] = {
    { 0x24, 0x41 },  // GP0: L2 demand data read hit
    { 0x24, 0x21 },  // GP1: L2 demand data read miss
    { 0xD1, 0x20 },  // GP2: L3 load hit (MEM_LOAD_RETIRED.L3_HIT)
    { 0xD1, 0x40 },  // GP3: L3 load miss (MEM_LOAD_RETIRED.L3_MISS)
};

static __forceinline ULONGLONG
BuildPerfEvtSel(UCHAR evt, UCHAR umask, BOOLEAN enUsr, BOOLEAN enOs, BOOLEAN anyThread)
{
    ULONGLONG v = 0;
    v |= (ULONGLONG)evt;
    v |= ((ULONGLONG)umask) << 8;
    if (enUsr)     v |= EVSEL_USR;
    if (enOs)      v |= EVSEL_OS;
    if (anyThread) v |= EVSEL_ANYTHREAD;   // keep FALSE unless you *need* both SMT threads
    v |= EVSEL_EN;
    return v;
}

static __forceinline ULONGLONG
BuildFixedCtrl(BOOLEAN enUsr, BOOLEAN enOs, BOOLEAN anyThread, BOOLEAN pmi)
{
    ULONGLONG per = 0;
    if (enOs)      per |= FIXED_EN_OS;
    if (enUsr)     per |= FIXED_EN_USR;
    if (anyThread) per |= FIXED_ANY;
    if (pmi)       per |= FIXED_PMI;
    // ctr0 in bits 3:0, ctr1 in 7:4, ctr2 in 11:8
    return (per << 0) | (per << 4) | (per << 8);
}

static __forceinline VOID
SetUserRdpmcOnThisCPU(BOOLEAN allow)
{
    ULONGLONG cr4 = __readcr4();
    if (allow) cr4 |= CR4_PCE;
    else       cr4 &= ~CR4_PCE;
    __writecr4(cr4);
    // RDPMC ECX: bit30=1 selects fixed counters; =0 selects general-purpose counters.
}

static VOID
ProgramPMU_OnThisCPU(BOOLEAN countUser, BOOLEAN countKernel, BOOLEAN allowUserRdpmc)
{
    // 0) Stop everything on this CPU
    __writemsr(IA32_PERF_GLOBAL_CTRL, 0);

    // 1) Fixed counters: enable OS+USR as requested; no AnyThread/PMI by default
    __writemsr(IA32_FIXED_CTR_CTRL, BuildFixedCtrl(countUser, countKernel, FALSE, FALSE));

    // 2) Program GP selectors (IA32_PERFEVTSEL0..3) and clear GP counters (IA32_PMC0..3)
    for (int i = 0; i < 4; ++i) {
        ULONG msrSel = IA32_PERFEVTSEL0 + (ULONG)i;
        ULONG msrPmc = IA32_PMC0 + (ULONG)i;
        ULONGLONG selVal = BuildPerfEvtSel(g_GpEvents[i].evt, g_GpEvents[i].um,
            countUser, countKernel, FALSE /*anyThread*/);
        __writemsr(msrSel, selVal);    // program event
        __writemsr(msrPmc, 0);         // clear counter (48-bit width typical)
    }

    // 3) Clear fixed counters to start from zero
    __writemsr(IA32_FIXED_CTR0, 0);
    __writemsr(IA32_FIXED_CTR1, 0);
    __writemsr(IA32_FIXED_CTR2, 0);

    // 4) Optionally allow user-mode RDPMC (CR4.PCE)
    SetUserRdpmcOnThisCPU(allowUserRdpmc);

    // 5) Globally enable: GP0..3 (bits 0..3), Fixed0..2 (bits 32..34)
    {
        ULONGLONG enableGp = (1ull << 0) | (1ull << 1) | (1ull << 2) | (1ull << 3);
        ULONGLONG enableFx = (1ull << 32) | (1ull << 33) | (1ull << 34);
        __writemsr(IA32_PERF_GLOBAL_CTRL, enableGp | enableFx);
    }
}

static VOID
DisablePMU_OnThisCPU(VOID)
{
    __writemsr(IA32_PERF_GLOBAL_CTRL, 0);
}

// Execute cb() once on each logical CPU (processor-group aware).
static VOID
ForEachLogicalCPU(VOID(*cb)(VOID))
{
    ULONG total = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
    for (ULONG idx = 0; idx < total; ++idx)
    {
        PROCESSOR_NUMBER pn;
        RtlZeroMemory(&pn, sizeof(pn));
        KeGetProcessorNumberFromIndex(idx, &pn); // maps idx -> (Group, Number)

        GROUP_AFFINITY gaNew, gaOld;
        RtlZeroMemory(&gaNew, sizeof(gaNew));
        RtlZeroMemory(&gaOld, sizeof(gaOld));
        gaNew.Group = pn.Group;
        gaNew.Mask = (KAFFINITY)(1ull << pn.Number);

        KeSetSystemGroupAffinityThread(&gaNew, &gaOld);
        cb();  // run on that CPU
        KeRevertToUserGroupAffinityThread(&gaOld);
    }
}

// ---------- Public APIs (C) you call from your driver ----------

// Enable fixed + 4 GP counters on all logical CPUs.
// - countUser:   count when CPL>0
// - countKernel: count when CPL==0
// - allowUserRdpmc: set CR4.PCE so user-mode can execute RDPMC
typedef struct _PMU_CFG_CTX {
    BOOLEAN countUser;
    BOOLEAN countKernel;
    BOOLEAN allowUserRdpmc;
} PMU_CFG_CTX;

static PMU_CFG_CTX gPmuCtx;

static VOID PmuPerCpuEnableCallback(VOID)
{
    ProgramPMU_OnThisCPU(gPmuCtx.countUser, gPmuCtx.countKernel, gPmuCtx.allowUserRdpmc);
}

VOID
PmuEnableFixedAndGpAllCPUs(BOOLEAN countUser, BOOLEAN countKernel, BOOLEAN allowUserRdpmc)
{
    PAGED_CODE();
    gPmuCtx.countUser = countUser;
    gPmuCtx.countKernel = countKernel;
    gPmuCtx.allowUserRdpmc = allowUserRdpmc;
    ForEachLogicalCPU(PmuPerCpuEnableCallback);
}

// Stop all fixed + GP counters on all CPUs
VOID
PmuDisableAllCPUs(VOID)
{
    PAGED_CODE();
    ForEachLogicalCPU(DisablePMU_OnThisCPU);
}


NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);

    DriverObject->DriverUnload = DriverUnload;

    DbgPrint("Driver loaded\n");

    //SetPCEFlag();
    //SetCR4PCEOnAllCores();
    PmuEnableFixedAndGpAllCPUs(TRUE, TRUE, TRUE);
    DbgPrint("CR4 PCE has been set\n");

    return STATUS_SUCCESS;
}
