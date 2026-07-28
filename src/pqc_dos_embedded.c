/*
 * pqc_dos_embedded.c — mechanism O's PQC-reassembly memory-DoS, measured on a
 * CONSTRAINED Cortex-M microcontroller (real ARMv7-M ISA in QEMU, hard SRAM cap).
 *
 * Embedded sibling of the server-side DoS measurement. That version measures
 * peak RSS with getrusage(); a microcontroller has only tens of KiB of SRAM, so the
 * naive "reassemble the whole PQC object, THEN verify" design does not merely use more
 * memory — it CANNOT RUN: the allocation fails / the device faults. That is the
 * executive demo: a stock constrained AP melts down under a PQC fragment flood while
 * the gated (mechanism O) build survives indefinitely on the same silicon.
 *
 *   naive       : allocate N*frag up front (reassemble-then-verify). On a device whose
 *                 SRAM is far smaller than the flood, the allocation returns NULL ->
 *                 status=oom (the device cannot proceed; on real hardware this is an
 *                 allocation failure or watchdog reset).
 *   incremental : mechanism O. Bounded MAX_BUFFER_BYTES (16 KiB) window; fold each
 *                 fragment into a running SHA-256 chain as it arrives -> heap bounded
 *                 regardless of flood size -> status=ok.
 *
 * Fully FREESTANDING (this arm-none-eabi-gcc ships no newlib): own vector table, own
 * semihosting I/O, own bump allocator over a fixed SRAM arena, own memcpy/memset, and
 * a self-contained SHA-256 (mechanism O's real per-fragment work). No libc, no liboqs.
 * The "device SRAM" is the arena size (and the RAM region in embedded.ld). Output is
 * key=value lines over ARM semihosting, parsed by embedded_dos_ci.py.
 *
 * Build/run: see build_embedded.sh (arm-none-eabi-gcc -nostdlib, embedded.ld,
 *            qemu-system-arm -M lm3s6965evb -semihosting).
 */
#include <stdint.h>
#include <stddef.h>

#ifndef FLOOD_N
#  define FLOOD_N 200000UL              /* fragments (200000 * 512 B = ~102 MB flood) */
#endif
#ifndef FRAG_BYTES
#  define FRAG_BYTES 512UL              /* per-fragment payload */
#endif
#ifndef MAX_BUFFER_BYTES
#  define MAX_BUFFER_BYTES (16UL * 1024UL)   /* mechanism O bounded per-session cap */
#endif
#ifndef DEVICE_RAM_BYTES
#  define DEVICE_RAM_BYTES (64UL * 1024UL)   /* modeled MCU SRAM (lm3s6965evb) */
#endif
#ifndef ARENA_BYTES
#  define ARENA_BYTES (48UL * 1024UL)        /* reassembly heap budget within SRAM */
#endif

/* ---- freestanding mem primitives (gcc may emit calls to these) ---- */
void *memcpy(void *d, const void *s, size_t n){ unsigned char *a=d; const unsigned char *b=s; while(n--) *a++=*b++; return d; }
void *memset(void *d, int c, size_t n){ unsigned char *a=d; while(n--) *a++=(unsigned char)c; return d; }

/* ---- ARM semihosting (bkpt 0xAB) ---- */
static long sh_call(long op, void *arg){
    register long r0 asm("r0")=op; register void *r1 asm("r1")=arg;
    asm volatile("bkpt 0xAB":"+r"(r0):"r"(r1):"memory");
    return r0;
}
static void sh_write0(const char *s){ sh_call(0x04, (void*)s); }      /* SYS_WRITE0 */
static void sh_exit(int code){ uint32_t blk[2]={0x20026u,(uint32_t)code}; sh_call(0x18, blk); } /* SYS_EXIT */

static const char *utoa_(unsigned long v, char *buf){
    char tmp[24]; int i=0;
    if(v==0) tmp[i++]='0';
    while(v){ tmp[i++]=(char)('0'+(v%10UL)); v/=10UL; }
    int j=0; while(i) buf[j++]=tmp[--i]; buf[j]='\0'; return buf;
}
static void emit_u(const char *k, unsigned long v){ char b[24]; sh_write0(k); sh_write0(utoa_(v,b)); }

/* ---- bump allocator over a fixed SRAM arena (the device's RAM ceiling) ---- */
static unsigned char g_arena[ARENA_BYTES];
typedef struct { unsigned long used; } alloc_t;
static void *aalloc(alloc_t *a, unsigned long n){
    if(n > (unsigned long)ARENA_BYTES - a->used) return 0;   /* overflow-safe */
    void *p=&g_arena[a->used]; a->used += n; return p;
}

/* ---- compact self-contained SHA-256 ---- */
typedef struct { uint32_t s[8]; uint64_t len; uint8_t buf[64]; unsigned long n; } sha_ctx;
static const uint32_t K256[64]={
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
#define ROR(x,n) (((x)>>(n))|((x)<<(32-(n))))
static void sha_block(sha_ctx *c, const uint8_t *p){
    uint32_t w[64],a,b,cc,d,e,f,g,h,t1,t2; int i;
    for(i=0;i<16;i++) w[i]=((uint32_t)p[i*4]<<24)|((uint32_t)p[i*4+1]<<16)|((uint32_t)p[i*4+2]<<8)|p[i*4+3];
    for(i=16;i<64;i++){ uint32_t s0=ROR(w[i-15],7)^ROR(w[i-15],18)^(w[i-15]>>3);
        uint32_t s1=ROR(w[i-2],17)^ROR(w[i-2],19)^(w[i-2]>>10); w[i]=w[i-16]+s0+w[i-7]+s1; }
    a=c->s[0];b=c->s[1];cc=c->s[2];d=c->s[3];e=c->s[4];f=c->s[5];g=c->s[6];h=c->s[7];
    for(i=0;i<64;i++){ uint32_t S1=ROR(e,6)^ROR(e,11)^ROR(e,25),ch=(e&f)^((~e)&g);
        t1=h+S1+ch+K256[i]+w[i]; uint32_t S0=ROR(a,2)^ROR(a,13)^ROR(a,22),mj=(a&b)^(a&cc)^(b&cc);
        t2=S0+mj; h=g;g=f;f=e;e=d+t1;d=cc;cc=b;b=a;a=t1+t2; }
    c->s[0]+=a;c->s[1]+=b;c->s[2]+=cc;c->s[3]+=d;c->s[4]+=e;c->s[5]+=f;c->s[6]+=g;c->s[7]+=h;
}
static void sha_init(sha_ctx *c){ c->s[0]=0x6a09e667;c->s[1]=0xbb67ae85;c->s[2]=0x3c6ef372;c->s[3]=0xa54ff53a;
    c->s[4]=0x510e527f;c->s[5]=0x9b05688c;c->s[6]=0x1f83d9ab;c->s[7]=0x5be0cd19;c->len=0;c->n=0; }
static void sha_upd(sha_ctx *c,const uint8_t *d,unsigned long n){ c->len+=n;
    while(n){ unsigned long k=64-c->n; if(k>n)k=n; memcpy(c->buf+c->n,d,k); c->n+=k; d+=k; n-=k;
        if(c->n==64){ sha_block(c,c->buf); c->n=0; } } }
static void sha_fin(sha_ctx *c,uint8_t out[32]){ uint64_t bits=c->len*8ULL; int i; uint8_t pad=0x80,z=0;
    sha_upd(c,&pad,1); while(c->n!=56) sha_upd(c,&z,1);
    uint8_t L[8]; for(i=0;i<8;i++)L[i]=(uint8_t)(bits>>(56-8*i)); sha_upd(c,L,8);
    for(i=0;i<8;i++){ out[i*4]=(uint8_t)(c->s[i]>>24);out[i*4+1]=(uint8_t)(c->s[i]>>16);
        out[i*4+2]=(uint8_t)(c->s[i]>>8);out[i*4+3]=(uint8_t)c->s[i]; } }
static void sha256(const uint8_t *d,unsigned long n,uint8_t o[32]){ sha_ctx c; sha_init(&c); sha_upd(&c,d,n); sha_fin(&c,o); }

int main(void){
    unsigned long n=FLOOD_N, frag=FRAG_BYTES, cap=MAX_BUFFER_BYTES;
    alloc_t A={0};

    sh_write0("device=cortex-m3(lm3s6965evb) isa=ARMv7-M");
    emit_u(" ram_bytes=", DEVICE_RAM_BYTES); emit_u(" arena_bytes=", ARENA_BYTES); sh_write0("\n");
    emit_u("flood_fragments=", n); emit_u(" frag_bytes=", frag);
    emit_u(" flood_total_bytes=", n*frag); emit_u(" bounded_cap_bytes=", cap); sh_write0("\n");

    uint8_t *fragbuf=(uint8_t*)aalloc(&A, frag);
    if(!fragbuf){ sh_write0("fatal=fragbuf_alloc_failed\n"); sh_exit(1); return 1; }
    memset(fragbuf,0xAB,frag);

    /* ---- naive: reassemble-then-verify (needs ALL fragments resident) ---- */
    unsigned long total=n*frag;
    uint8_t *reassembly=(uint8_t*)aalloc(&A, total);
    if(!reassembly){
        unsigned long ratio=total/DEVICE_RAM_BYTES;   /* division-first: no 32-bit overflow */
        sh_write0("naive_status=oom"); emit_u(" naive_requested_bytes=", total);
        emit_u(" naive_x_device_ram=", ratio); sh_write0("\n");
    } else {
        for(unsigned long p=0;p<total;p+=4096) reassembly[p]=fragbuf[0];
        sh_write0("naive_status=ok"); emit_u(" naive_requested_bytes=", total);
        sh_write0(" (device was large enough)\n");
        A.used -= total;   /* release for the next phase */
    }

    /* ---- incremental: mechanism O — bounded window + SHA-256 fold ---- */
    uint8_t *bounded=(uint8_t*)aalloc(&A, cap);
    if(!bounded){ sh_write0("fatal=bounded_alloc_failed\n"); sh_exit(1); return 1; }
    memset(bounded,0,cap);
    uint8_t chain[32]; sha256((const uint8_t*)"origin",6,chain);
    unsigned long processed=0, held=0;
    for(unsigned long i=0;i<n;i++){
        if(held+frag>cap) held=0;            /* mechanism O: drain window, never grow */
        memcpy(bounded+held,fragbuf,frag); held+=frag;
        uint8_t tmp[32+64]; unsigned long fl=frag<64?frag:64;
        memcpy(tmp,chain,32); memcpy(tmp+32,fragbuf,fl); sha256(tmp,32+fl,chain);
        processed++;
    }
    sh_write0("incremental_status=ok"); emit_u(" incremental_processed=", processed);
    emit_u(" incremental_peak_heap_bytes=", A.used); sh_write0("\n");
    { char b[24]; const char *hx="0123456789abcdef"; char hh[17];
      for(int i=0;i<8;i++){ hh[i*2]=hx[chain[i]>>4]; hh[i*2+1]=hx[chain[i]&0xF]; } hh[16]='\0';
      sh_write0("incremental_chain="); sh_write0(hh); sh_write0("\n"); (void)b; }
    sh_write0("RESULT=done\n");
    sh_exit(0);
    return 0;
}

/* ---- vector table + reset (freestanding, no startup files) ---- */
extern unsigned long _estack;
void Reset_Handler(void){ (void)main(); sh_exit(0); for(;;){} }
__attribute__((section(".isr_vector"), used))
void (* const g_vectors[])(void) = {
    (void(*)(void))(&_estack),   /* initial SP  */
    Reset_Handler,               /* reset       */
};
