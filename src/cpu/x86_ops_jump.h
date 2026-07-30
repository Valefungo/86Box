#define cond_O   (VF_SET())
#define cond_NO  (!VF_SET())
#define cond_B   (CF_SET())
#define cond_NB  (!CF_SET())
#define cond_E   (ZF_SET())
#define cond_NE  (!ZF_SET())
#define cond_BE  (CF_SET() || ZF_SET())
#define cond_NBE (!CF_SET() && !ZF_SET())
#define cond_S   (NF_SET())
#define cond_NS  (!NF_SET())
#define cond_P   (PF_SET())
#define cond_NP  (!PF_SET())
#define cond_L   (((NF_SET()) ? 1 : 0) != ((VF_SET()) ? 1 : 0))
#define cond_NL  (((NF_SET()) ? 1 : 0) == ((VF_SET()) ? 1 : 0))
#define cond_LE  (((NF_SET()) ? 1 : 0) != ((VF_SET()) ? 1 : 0) || (ZF_SET()))
#define cond_NLE (((NF_SET()) ? 1 : 0) == ((VF_SET()) ? 1 : 0) && (!ZF_SET()))

#define opJ(condition)                                                  \
    static int opJ##condition(uint32_t fetchdat)                        \
    {                                                                   \
        int8_t offset = (int8_t) getbytef();                            \
        CLOCK_CYCLES(timing_bnt);                                       \
        if (cond_##condition) {                                         \
            cpu_state.pc += offset;                                     \
            if (!(cpu_state.op32 & 0x100))                              \
                cpu_state.pc &= 0xffff;                                 \
            CLOCK_CYCLES_ALWAYS(timing_bt);                             \
            CPU_BLOCK_END();                                            \
            PREFETCH_RUN(timing_bt + timing_bnt, 2, -1, 0, 0, 0, 0, 0); \
            PREFETCH_FLUSH();                                           \
            return 1;                                                   \
        }                                                               \
        PREFETCH_RUN(timing_bnt, 2, -1, 0, 0, 0, 0, 0);                 \
        return 0;                                                       \
    }                                                                   \
                                                                        \
    static int opJ##condition##_w(uint32_t fetchdat)                    \
    {                                                                   \
        int16_t offset = (int16_t) getwordf();                          \
        CLOCK_CYCLES(timing_bnt);                                       \
        if (cond_##condition) {                                         \
            cpu_state.pc += offset;                                     \
            cpu_state.pc &= 0xffff;                                     \
            CLOCK_CYCLES_ALWAYS(timing_bt);                             \
            CPU_BLOCK_END();                                            \
            PREFETCH_RUN(timing_bt + timing_bnt, 3, -1, 0, 0, 0, 0, 0); \
            PREFETCH_FLUSH();                                           \
            return 1;                                                   \
        }                                                               \
        PREFETCH_RUN(timing_bnt, 3, -1, 0, 0, 0, 0, 0);                 \
        return 0;                                                       \
    }                                                                   \
                                                                        \
    static int opJ##condition##_l(UNUSED(uint32_t fetchdat))            \
    {                                                                   \
        uint32_t offset = getlong();                                    \
        if (cpu_state.abrt)                                             \
            return 1;                                                   \
        CLOCK_CYCLES(timing_bnt);                                       \
        if (cond_##condition) {                                         \
            cpu_state.pc += offset;                                     \
            CLOCK_CYCLES_ALWAYS(timing_bt);                             \
            CPU_BLOCK_END();                                            \
            PREFETCH_RUN(timing_bt + timing_bnt, 5, -1, 0, 0, 0, 0, 0); \
            PREFETCH_FLUSH();                                           \
            return 1;                                                   \
        }                                                               \
        PREFETCH_RUN(timing_bnt, 5, -1, 0, 0, 0, 0, 0);                 \
        return 0;                                                       \
    }

// clang-format off
opJ(O)
opJ(NO)
opJ(B)
opJ(NB)
opJ(E)
opJ(NE)
opJ(BE)
opJ(NBE)
opJ(S)
opJ(NS)
opJ(P)
opJ(NP)
opJ(L)
opJ(NL)
opJ(LE)
opJ(NLE)
    // clang-format on

static int
opLOOPNE_w(uint32_t fetchdat)
{
    int8_t offset = (int8_t) getbytef();
    CX--;
    CLOCK_CYCLES((is486) ? 7 : 11);
    PREFETCH_RUN(11, 2, -1, 0, 0, 0, 0, 0);
    if (CX && !ZF_SET()) {
        cpu_state.pc += offset;
        if (!(cpu_state.op32 & 0x100))
            cpu_state.pc &= 0xffff;
        CPU_BLOCK_END();
        PREFETCH_FLUSH();
        return 1;
    }
    return 0;
}
static int
opLOOPNE_l(uint32_t fetchdat)
{
    int8_t offset = (int8_t) getbytef();
    ECX--;
    CLOCK_CYCLES((is486) ? 7 : 11);
    PREFETCH_RUN(11, 2, -1, 0, 0, 0, 0, 0);
    if (ECX && !ZF_SET()) {
        cpu_state.pc += offset;
        if (!(cpu_state.op32 & 0x100))
            cpu_state.pc &= 0xffff;
        CPU_BLOCK_END();
        PREFETCH_FLUSH();
        return 1;
    }
    return 0;
}

static int
opLOOPE_w(uint32_t fetchdat)
{
    int8_t offset = (int8_t) getbytef();
    CX--;
    CLOCK_CYCLES((is486) ? 7 : 11);
    PREFETCH_RUN(11, 2, -1, 0, 0, 0, 0, 0);
    if (CX && ZF_SET()) {
        cpu_state.pc += offset;
        if (!(cpu_state.op32 & 0x100))
            cpu_state.pc &= 0xffff;
        CPU_BLOCK_END();
        PREFETCH_FLUSH();
        return 1;
    }
    return 0;
}
static int
opLOOPE_l(uint32_t fetchdat)
{
    int8_t offset = (int8_t) getbytef();
    ECX--;
    CLOCK_CYCLES((is486) ? 7 : 11);
    PREFETCH_RUN(11, 2, -1, 0, 0, 0, 0, 0);
    if (ECX && ZF_SET()) {
        cpu_state.pc += offset;
        if (!(cpu_state.op32 & 0x100))
            cpu_state.pc &= 0xffff;
        CPU_BLOCK_END();
        PREFETCH_FLUSH();
        return 1;
    }
    return 0;
}

static int
opLOOP_w(uint32_t fetchdat)
{
    int8_t offset = (int8_t) getbytef();
    CX--;
    CLOCK_CYCLES((is486) ? 7 : 11);
    PREFETCH_RUN(11, 2, -1, 0, 0, 0, 0, 0);
    if (CX) {
        cpu_state.pc += offset;
        if (!(cpu_state.op32 & 0x100))
            cpu_state.pc &= 0xffff;
        CPU_BLOCK_END();
        PREFETCH_FLUSH();
        return 1;
    }
    return 0;
}
static int
opLOOP_l(uint32_t fetchdat)
{
    int8_t offset = (int8_t) getbytef();
    ECX--;
    CLOCK_CYCLES((is486) ? 7 : 11);
    PREFETCH_RUN(11, 2, -1, 0, 0, 0, 0, 0);
    if (ECX) {
        cpu_state.pc += offset;
        if (!(cpu_state.op32 & 0x100))
            cpu_state.pc &= 0xffff;
        CPU_BLOCK_END();
        PREFETCH_FLUSH();
        return 1;
    }
    return 0;
}

static int
opJCXZ(uint32_t fetchdat)
{
    int8_t offset = (int8_t) getbytef();
    CLOCK_CYCLES(5);
    if (!CX) {
        cpu_state.pc += offset;
        if (!(cpu_state.op32 & 0x100))
            cpu_state.pc &= 0xffff;
        CLOCK_CYCLES(4);
        CPU_BLOCK_END();
        PREFETCH_RUN(9, 2, -1, 0, 0, 0, 0, 0);
        PREFETCH_FLUSH();
        return 1;
    }
    PREFETCH_RUN(5, 2, -1, 0, 0, 0, 0, 0);
    return 0;
}
static int
opJECXZ(uint32_t fetchdat)
{
    int8_t offset = (int8_t) getbytef();
    CLOCK_CYCLES(5);
    if (!ECX) {
        cpu_state.pc += offset;
        if (!(cpu_state.op32 & 0x100))
            cpu_state.pc &= 0xffff;
        CLOCK_CYCLES(4);
        CPU_BLOCK_END();
        PREFETCH_RUN(9, 2, -1, 0, 0, 0, 0, 0);
        PREFETCH_FLUSH();
        return 1;
    }
    PREFETCH_RUN(5, 2, -1, 0, 0, 0, 0, 0);
    return 0;
}

static int
opJMP_r8(uint32_t fetchdat)
{
    int8_t offset = (int8_t) getbytef();
    cpu_state.pc += offset;
    if (!(cpu_state.op32 & 0x100))
        cpu_state.pc &= 0xffff;
    CPU_BLOCK_END();
    CLOCK_CYCLES((is486) ? 3 : 7);
    PREFETCH_RUN(7, 2, -1, 0, 0, 0, 0, 0);
    PREFETCH_FLUSH();
    return 0;
}
static int
opJMP_r16(uint32_t fetchdat)
{
    int16_t offset = (int16_t) getwordf();
    cpu_state.pc += offset;
    cpu_state.pc &= 0xffff;
    CPU_BLOCK_END();
    CLOCK_CYCLES((is486) ? 3 : 7);
    PREFETCH_RUN(7, 3, -1, 0, 0, 0, 0, 0);
    PREFETCH_FLUSH();
    return 0;
}
static int
opJMP_r32(UNUSED(uint32_t fetchdat))
{
    int32_t offset = (int32_t) getlong();
    if (cpu_state.abrt)
        return 1;
    cpu_state.pc += offset;
    CPU_BLOCK_END();
    CLOCK_CYCLES((is486) ? 3 : 7);
    PREFETCH_RUN(7, 5, -1, 0, 0, 0, 0, 0);
    PREFETCH_FLUSH();
    return 0;
}

static int
opJMP_far_a16(uint32_t fetchdat)
{
    uint16_t addr;
    uint16_t seg;
    uint32_t old_pc;

    addr = getwordf();
#ifdef CLAUDE_LOG
    {
        static int done = 0;
        if (!done) {
            done = 1;
            uint32_t segaddr = cs + cpu_state.pc;
            uint8_t  direct_lo = mem_readb_phys(segaddr);
            uint8_t  direct_hi = mem_readb_phys(segaddr + 1);
            uint8_t *biased_lo = NULL, *biased_hi = NULL;
            uint16_t biased_val = 0xFFFF;
            if (PCCACHE_VALID(segaddr)) {
                biased_lo  = (uint8_t *) PTR_RECOMBINE(&pccache2[segaddr], &pccache2[0]);
                biased_hi  = (uint8_t *) PTR_RECOMBINE(&pccache2[segaddr + 1], &pccache2[0]);
                biased_val = mem_load_u16_unaligned((void *) PTR_RECOMBINE(&pccache2[segaddr], &pccache2[0]));
            }
            pclog("# opJMP_far_a16 FIRST CALL: addr=%04X cs_base=%08X pc=%08X segaddr=%08X "
                  "pccache=%08X pccache2=%p pccache_valid=%d "
                  "direct_bytes=%02X %02X biased_ptr_lo=%p biased_ptr_hi=%p biased_deref=%02X %02X "
                  "mem_load_u16_unaligned=%04X\n",
                  addr, cs, cpu_state.pc, segaddr,
                  (unsigned) pccache, (void *) pccache2, PCCACHE_VALID(segaddr),
                  direct_lo, direct_hi,
                  (void *) biased_lo, (void *) biased_hi,
                  biased_lo ? *biased_lo : 0xFF, biased_hi ? *biased_hi : 0xFF,
                  biased_val);
        }
    }
#endif
    seg  = getword();
    if (cpu_state.abrt)
        return 1;
    old_pc       = cpu_state.pc;
    cpu_state.pc = addr;
    op_loadcsjmp(seg, old_pc);
    CPU_BLOCK_END();
    PREFETCH_RUN(11, 5, -1, 0, 0, 0, 0, 0);
    PREFETCH_FLUSH();
    return 0;
}
static int
opJMP_far_a32(UNUSED(uint32_t fetchdat))
{
    uint16_t seg;
    uint32_t addr;
    uint32_t old_pc;

    addr = getlong();
    seg  = getword();
    if (cpu_state.abrt)
        return 1;
    old_pc       = cpu_state.pc;
    cpu_state.pc = addr;
    op_loadcsjmp(seg, old_pc);
    CPU_BLOCK_END();
    PREFETCH_RUN(11, 7, -1, 0, 0, 0, 0, 0);
    PREFETCH_FLUSH();
    return 0;
}

static int
opCALL_r16(uint32_t fetchdat)
{
    int16_t addr = (int16_t) getwordf();

    PUSH_W(cpu_state.pc);
    cpu_state.pc += addr;
    cpu_state.pc &= 0xffff;
    CPU_BLOCK_END();
    CLOCK_CYCLES((is486) ? 3 : 7);
    PREFETCH_RUN(7, 3, -1, 0, 0, 1, 0, 0);
    PREFETCH_FLUSH();
    return 0;
}
static int
opCALL_r32(UNUSED(uint32_t fetchdat))
{
    int32_t addr = getlong();

    if (cpu_state.abrt)
        return 1;
    PUSH_L(cpu_state.pc);
    cpu_state.pc += addr;
    CPU_BLOCK_END();
    CLOCK_CYCLES((is486) ? 3 : 7);
    PREFETCH_RUN(7, 5, -1, 0, 0, 0, 1, 0);
    PREFETCH_FLUSH();
    return 0;
}

static int
opRET_w(UNUSED(uint32_t fetchdat))
{
    uint16_t ret;

    ret = POP_W();
    if (cpu_state.abrt)
        return 1;
    cpu_state.pc = ret;
    CPU_BLOCK_END();

    CLOCK_CYCLES((is486) ? 5 : 10);
    PREFETCH_RUN(10, 1, -1, 1, 0, 0, 0, 0);
    PREFETCH_FLUSH();
    return 0;
}
static int
opRET_l(UNUSED(uint32_t fetchdat))
{
    uint32_t ret;

    ret = POP_L();
    if (cpu_state.abrt)
        return 1;
    cpu_state.pc = ret;
    CPU_BLOCK_END();

    CLOCK_CYCLES((is486) ? 5 : 10);
    PREFETCH_RUN(10, 1, -1, 0, 1, 0, 0, 0);
    PREFETCH_FLUSH();
    return 0;
}

static int
opRET_w_imm(uint32_t fetchdat)
{
    uint16_t ret;
    uint16_t offset = getwordf();

    ret = POP_W();
    if (cpu_state.abrt)
        return 1;
    if (stack32)
        ESP += offset;
    else
        SP += offset;
    cpu_state.pc = ret;
    CPU_BLOCK_END();

    CLOCK_CYCLES((is486) ? 5 : 10);
    PREFETCH_RUN(10, 5, -1, 1, 0, 0, 0, 0);
    PREFETCH_FLUSH();
    return 0;
}
static int
opRET_l_imm(uint32_t fetchdat)
{
    uint32_t ret;
    uint16_t offset = getwordf();

    ret = POP_L();
    if (cpu_state.abrt)
        return 1;
    if (stack32)
        ESP += offset;
    else
        SP += offset;
    cpu_state.pc = ret;
    CPU_BLOCK_END();

    CLOCK_CYCLES((is486) ? 5 : 10);
    PREFETCH_RUN(10, 5, -1, 0, 1, 0, 0, 0);
    PREFETCH_FLUSH();
    return 0;
}
