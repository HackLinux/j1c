#include <stdint.h>
#include <stdio.h>

#include "dtime.h"

#if 1
	#define T(x) x
#else
	#define T(x)
#endif

#define STATE(_f, args...) T(printf("0x%06x:(%04x) " _f, pc << 1, insn, ## args));

#define ljrj_bits(_value, _start, _end) (((_value) << (31 - _start)) >> (31 - (_start - _end)))

static inline uint32_t bits(uint32_t value, uint8_t start, uint8_t end)
{
	return(ljrj_bits(value, start, end));
}

static inline int32_t sext(int32_t value, uint8_t start, uint8_t end)
{
	return(ljrj_bits(value, start, end));
}

static uint16_t zero[65536];

static inline uint16_t _m16_get(uint16_t addr)
{
	uint16_t data = zero[addr];
	return(data >> 8 | data << 8);
}

static inline void _m16_set(uint16_t addr, uint16_t data)
{
	zero[addr] = data >> 8 | data << 8;
}

static uint16_t insn;
static uint8_t sys_rst_i;

static inline uint16_t immediate(void)
{
	return(insn & 0x7fff);
}

static uint16_t pc, _pc;

static inline uint16_t pc_plus_1(void)
{
	return(pc + 1);
}

static uint16_t dstack[32];
static uint16_t dsp, _dsp, st0;

static uint16_t rstack[32];
static uint16_t rsp, _rsp, _rstkD, _rstkW;

static inline uint16_t st1(void)
{
	return(dstack[dsp]);
}

static inline uint16_t rst0(void)
{
	return(rstack[rsp]);
}

static inline uint8_t st0sel(void)
{
	switch(bits(insn, 14, 13))
	{
		case	0:
		case	2:
			return(0);
			break;
		case	1:
			return(1);
			break;
		case	3:
			return(bits(insn, 11, 8));
			break;
	}
	
	return(0);
}

static inline uint8_t is_lit(void)
{
	return((insn >> 15) & 1);
}

static uint16_t io[65536];

static uint16_t io_din(void)
{
	return(io[st0]);

}

static uint16_t ram[65536];

static uint16_t ramrd(void)
{
	return(ram[st0]);
}

static inline uint16_t _st0(void)
{
	uint16_t dbus = 0;

	if(is_lit()) {
		dbus = immediate();
		T(printf("lit(%04x)", dbus));
	} else {
		uint8_t _st0sel = st0sel();
		T(printf(" st0sel(%04x)", dbus));
		switch(_st0sel) {
		case	0x0:
			dbus = st0;
			T(printf(" T(%04x)", dbus));
			break;
		case	0x1:
			dbus = st1();
			T(printf(" N(%04x)", dbus));
			break;
		case	0x2:
			dbus = st0 + st1();
			break;
		case	0x3:
			dbus = st0 & st1();
			break;
		case	0x4:
			dbus = st0 | st1();
			break;
		case	0x5:
			dbus = st0 ^ st1();
			break;
		case	0x6:
			dbus = ~st0;
			break;
		case	0x7:
			dbus = ((st1() == st0) ? -1 : 0);
			break;
		case	0x8:
			dbus = (((signed)st1() < (signed)st0) ? -1 : 0);
			break;
		case	0x9:
			dbus = st1() >> (st0 & 0xf);
			break;
		case	0xa:
			dbus = st0 - 1;
			break;
		case	0xb:
			dbus = rst0();
			break;
		case	0xc: // iord / ramrd
			dbus = (st0 & 0xc000) ? io_din() : ramrd();
			break;
		case	0xd:
			dbus = st1() << (st0 & 0xf);
			break;
		case	0xe:
			dbus = (rsp << (3 + 5)) | dsp;
			break;
		case	0xf:
			dbus = ((st1() < st0) ? -1 : 0);
			break;	
	}}

	return(dbus);
}

static inline uint8_t is_alu(void)
{
	return(bits(insn, 15, 13) == 3);
}


static inline uint8_t is_call(void)
{
	return(bits(insn, 15, 13) == 2);
}

static inline uint8_t is_cond(void)
{
	return((bits(insn, 15, 13) == 1) && (0 == st0));
}

static inline uint8_t is_jump(void)
{
	return(bits(insn, 15, 13) == 0);
}

static inline uint8_t _ramWE(void)
{
	return(is_alu() && (insn & (1 << 5)));
}

static inline uint8_t io_rd(void)
{
	return(is_alu() && (0xc == st0sel()));
}

static inline uint8_t _dstkW(void)
{
	return(is_lit() || (is_alu() && (insn & (1 << 6))));
}

static inline int8_t dd(void)
{
	return(sext(insn, 1, 0));
}

static inline int8_t rd(void)
{
	return(sext(insn, 3, 2));
}

int j1_step(void)
{
	insn = _m16_get(pc);

	STATE();

	if(is_lit()) {
//		T(printf("is_lit "));
		_dsp = dsp + 1;
		_rsp = rsp;
		_rstkW = 0;
//		_rstkD = _pc;
	} else if(is_alu()) {
		_dsp = dsp + dd();
		_rsp = rsp + rd();
		_rstkW = insn & (1 << 6);
		_rstkD = st0;
		T(printf("alu: dsp(%03x, %01x)=%03x rsp(%03x, %01x)=%03x _rstkW=%01x _rstkD=%04x",
			dsp, dd() & 3, _dsp & 0x1f, rsp, rd() & 3, _rsp & 0x1f, _rstkW, _rstkD));
	} else {
		if(is_jump()) {
//			T(printf("is_jump "));
			_dsp = dsp - 1;
		} else
			_dsp = dsp;
		if(is_call()) {
//			T(printf("is_call"));
			_rsp = rsp + 1;
			_rstkW = 1;
			_rstkD = pc_plus_1();
		} else {
			_rsp = rsp;
			_rstkW = 0;
//			_rstkD = _pc;
		}
	}
	
	if(sys_rst_i) {
		_pc = pc;
	} else {
		if (is_jump() || is_cond() || is_call()) {
			_pc = bits(insn, 12, 0);
			T(printf("jump(%06x)", _pc << 1));
		} else if (is_alu() && (insn & (1 << 12))) {
			_pc = rst0();
			T(printf(" ret(%06x)", _pc << 1));
		} else {
			_pc = pc_plus_1();
//			T(printf("next(%06x)", _pc));
		}
	}
	
	if(sys_rst_i) {
		_pc = 0;
		dsp = 0;
		st0 = 0;
		rsp = 0;
	} else {
		dsp = _dsp;
		pc = _pc;
		st0 = _st0();
		rsp = _rsp;
	}
	
	if(_dstkW()) {
		T(printf(" dstack[%03x] = %04x ", _dsp, st0));
		dstack[_dsp] = st0;
	}

	if(_rstkW) {
		T(printf(" rstack[%03x] = %04x ", _rsp, _rstkD));
		rstack[_rsp] = _rstkD;
	}

	if(_ramWE()) {
		T(printf("\nw[%06x](%04x)\n", st0, st1()));
		
	}

	T(printf("\n"));

	return(1);
}

void j1_reset(void)
{
	pc = 0;
	dsp = 0;
	st0 = 0;
	rsp = 0;
}

void j1_init(void)
{
	FILE *fp = fopen("j1.bin", "r");
	
	if(!fp)
		return;
	
	int i = 0;
	uint8_t *dst = (uint8_t *)&zero;
	while(!feof(fp) && (0 != fread(dst, 1, 1, fp)))
		i++, dst++;
	
	T(printf("%s: loaded j1.bin, size = %06x\n", __FUNCTION__, i));
	
	fclose(fp);
}

int main(void)
{
	j1_init();
	j1_reset();
	
	uint32_t cycle = 0;
	uint64_t start_time = get_dtime();
	
	sys_rst_i = 1;
	j1_step();
	
	sys_rst_i = 0;
	while((cycle < (1 << 8)) && j1_step())
		cycle++;
	
	uint64_t elapsed_dtime = get_elapsed_dtime(start_time);
	
	double eacdt = (double)elapsed_dtime / (double)cycle;
	
	printf("%s: elapsed=%016llu count=%08x eacdt=%010.3f\n", 
		__FUNCTION__, elapsed_dtime, cycle, eacdt);

	return(0);
}
