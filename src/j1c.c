#include <stdint.h>
#include <stdio.h>

#include "dtime.h"
#include "utility.h"

#define CONFIG_FLASH flash
#define CONFIG_CLOCK_CYCLE_SHIFT(x) x

typedef struct j1cpu_t {
	uint16_t	t;
//	uint16_t	n;
	uint16_t	pc;
	uint16_t	ir;
	uint64_t	cycle;
	struct stack_t {
		uint16_t sp;
		uint16_t data[32];
	}stack[2];
	uint16_t	zero[65536];
	uint16_t	flash[65536];
}j1cpu_t, *j1cpu_p;

enum {
	dstack = 0,
	rstack
};

#define PC j1->pc
#define IR j1->ir

#define DSP j1->stack[dstack].sp
#define RSP j1->stack[rstack].sp

#define TOS j1->t
#define NOS j1->stack[dstack].data[DSP - 1]

#define RTOS j1->stack[rstack].data[RSP]

j1cpu_t _j1cpu, *j1cpu = &_j1cpu;

static uint16_t ior(j1cpu_p j1, uint16_t addr)
{
	uint16_t dout;
	
	switch(addr) {
		case	0x6000:
			dout = CONFIG_CLOCK_CYCLE_SHIFT(j1->cycle);
			break;
		case	0x6002:
			dout = CONFIG_CLOCK_CYCLE_SHIFT(j1->cycle) >> 16;
			break;
		default:
			dout = 0x0946;
			break;
	}
	
	return(dout);
}

static inline uint16_t _f16_get(j1cpu_p j1, uint16_t addr)
{
	uint16_t data = j1->CONFIG_FLASH[addr];
	return(data >> 8 | data << 8);
}

static inline uint16_t _m16_get(j1cpu_p j1, uint16_t addr)
{
	uint16_t data = 0;
	
	if(bits(addr, 15, 14))
		printf("\n\t%06x: ior(%06x, %06x)=%04x\n", 
			j1->pc << 1, addr, bits(addr, 13, 0), data = ior(j1, addr));
	else {
		data = j1->zero[addr];
		data = (data >> 8 | data << 8);
	}

	return(data);
}

static inline void _m16_set(j1cpu_p j1, uint16_t addr, uint16_t data)
{
	if(bits(addr, 15, 14)) {
		printf("\n\t%06x: iow(%06x, %06x)\n", j1->pc << 1, addr, bits(addr, 13, 0));
//		iow(j1, addr, data);
	} else
		j1->zero[addr] = data >> 8 | data << 8;
}

static inline void push(j1cpu_p j1, uint8_t sn, uint16_t data)
{
	j1->stack[sn].data[j1->stack[sn].sp++] = data;
}

static inline uint16_t pop(j1cpu_p j1, uint8_t sn)
{
	return(j1->stack[sn].data[--j1->stack[sn].sp]);
}

static inline uint16_t j1_alu(j1cpu_p j1, uint16_t ir)
{
	uint8_t op = bits(ir, 11, 8);

	STATE("alu: op=%01x dsp(%03x, %02x) rsp(%03x, %02x) ", op, DSP, ir & 3, RSP, (ir >> 2) & 3);

	uint16_t dbus;

	switch(op) {
		case	0x0:
			dbus = TOS;
			TRACE(printf("T(%04x)", dbus));
			break;
		case	0x1:
			dbus = NOS;
			TRACE(printf("N(%04x)", dbus));
			break;
		case	0x2:
			dbus = TOS + NOS;
			TRACE(printf("T(%04x) + N(%04x) = %04x", TOS, NOS, dbus));
			break;
		case	0x3:
			dbus = TOS & NOS;
			TRACE(printf("T(%04x) & N(%04x) = %04x", TOS, NOS, dbus));
			break;
		case	0x4:
			dbus = TOS | NOS;
			TRACE(printf("T(%04x) | N(%04x) = %04x", TOS, NOS, dbus));
			break;
		case	0x5:
			dbus = TOS ^ NOS;
			TRACE(printf("T(%04x) ^ N(%04x) = %04x", TOS, NOS, dbus));
			break;
		case	0x6:
			dbus = ~TOS;
			TRACE(printf("~T(%04x) = %04x", TOS, dbus));
			break;
		case	0x7:
			dbus = ((NOS == TOS) ? -1 : 0);
			TRACE(printf("N(%04x) == T(%04x) = %04x", NOS, TOS, dbus));
			break;
		case	0x8:
			dbus = (((signed)NOS < (signed)TOS) ? -1 : 0);
			TRACE(printf("signed(N(%04x) < T(%04x)) = %04x", NOS, TOS, dbus));
			break;
		case	0x9: {
			uint8_t shift = (TOS & 0xf);
			dbus = NOS >> shift;
			TRACE(printf("N(%04x) >> %04x = %04x", NOS, shift, dbus));
		} break;
		case	0xa:
			dbus = TOS - 1;
			TRACE(printf("T(%04x) - 1 = %04x", TOS, dbus));
			break;
		case	0xb:
			dbus = RTOS;
			TRACE(printf("rstack[%04x](%04x)", RSP, dbus));
			break;
		case	0xc: // iord / ramrd
			dbus = _m16_get(j1, TOS);
			TRACE(printf("rd[%04x](%04x)", TOS, dbus));
			break;
		case	0xd: {
			uint8_t shift = (TOS & 0xf);
			dbus = NOS << shift;
			TRACE(printf("N(%04x) << %02x = %04x", NOS, shift, dbus));
		} break;
		case	0xe:
			dbus = (RSP << (3 + 4)) | DSP;
			TRACE(printf("rsp(%03x) | dsp(%03x) = %04x", RSP, DSP, dbus));
			break;
		case	0xf:
			dbus = ((NOS < TOS) ? -1 : 0);
			TRACE(printf("N(%04x) < T(%04x) = %04x", NOS, TOS, dbus));
			break;
	}

	return(dbus);
}

int j1_step(j1cpu_p j1)
{
	uint16_t ir = IR = _f16_get(j1, PC);
	uint16_t new_pc = PC + 1;
	
	if (ir & (1 << 15)) { // literal
		uint16_t lit = ir & 0x7fff;
		STATE("lit 0x%04x\n", lit);
		push(j1, dstack, TOS);
		TOS = lit;
	} else {
		uint8_t op = bits(ir, 15, 13);
		if (op == 3) {
			uint16_t dbus = j1_alu(j1, ir);

			if (ir & (1 << 5)) {
				TRACE(printf(" wr[0x%06x]=0x%04x", TOS, NOS));
				_m16_set(j1, TOS, NOS);
			}

			if (ir & (1 << 6)) {
				TRACE(printf(" rstack[%03x] = %04x", RSP, TOS)); 
				RTOS = TOS;
			}

			if (ir & (1 << 7)) {
				NOS = TOS;
//				TOS = dbus;
				TRACE(printf(" >> N[%03x]", DSP)); 
//				TRACE(printf(" dstack[%03x] = %04x", j1->dsp, dbus)); 
//				j1->dstack[j1->dsp] = dbus;
			}

			if (ir & 0xc) {
				int16_t rd = sext(ir, 3, 2);
				TRACE(uint16_t _RSP = RSP);
				RSP += rd;
				RSP &= 0x1f;
				
				TRACE(printf(" R%c%1d(%03x, %03x)", 
					rd & 2 ? '-' : '+', (ir & 1), _RSP, RSP));
			}

			if (ir & 3) {
				int16_t dd = sext(ir, 1, 0);
				TRACE(uint16_t _DSP = DSP);
				DSP += dd;
				DSP &= 0x1f;
				
				TRACE(printf(" D%c%1d(%03x, %03x)", 
					ir & 2 ? '-' : '+', ir & 1, _DSP, DSP));
			}

			if (ir & (1 << 12)) {
				TRACE(printf(" ret"));
				new_pc = RTOS;
			}

			TOS = dbus;

			TRACE(printf("\n"));
		} else {
			TRACE(const char *opname);
			TRACE(const char *branchstr = "");
			uint16_t branch_pc = bits(ir, 12, 0);
			
			if (op == 2) {
				push(j1, rstack, new_pc);
				TRACE(opname = "call");
			} else {
				TRACE(opname = "jump");
				if (op == 1) {
					uint8_t branch = (0 == TOS);
					TRACE(branchstr = branch ? "will " : "will not ");
					if(!branch)
						branch_pc = new_pc;
				}
			}
			
			STATE("%s%s 0x%06x\n", branchstr, opname, branch_pc << 1);

			new_pc = branch_pc;
		}
	}

	PC = new_pc;

	return(1);
}

void j1_reset(j1cpu_p j1)
{
	PC = 0;
	DSP = 0;
	RSP = 0;
}

void j1_init(j1cpu_p j1)
{
	FILE *fp = fopen("j1.bin", "r");
	
	if(!fp)
		return;
	
	int i = 0;
	uint8_t *zero = (uint8_t *)&j1->CONFIG_FLASH;
	while(!feof(fp) && (0 != fread(zero, 1, 1, fp)))
		i++, zero++;
	
	TRACE(printf("%s: loaded j1.bin, size = %06x\n", __FUNCTION__, i));
	
	fclose(fp);
}

int main(void)
{
	j1cpu_t _j1, *j1 = &_j1;

	j1_init(j1);
	j1_reset(j1);
	
	j1->cycle = 0;
	uint64_t start_time = get_dtime();
	
//	while((cycle < (1 << 22)) && j1_step(j1)) {
	while(j1_step(j1)) {
		j1->cycle++;
	}
	
	uint64_t elapsed_dtime = get_elapsed_dtime(start_time);
	
	double eacdt = (double)elapsed_dtime / (double)j1->cycle;
	
	printf("%s: elapsed=%016llu cycle=%016llu eacdt=%010.3f\n", 
		__FUNCTION__, elapsed_dtime, j1->cycle, eacdt);

	return(0);
}
