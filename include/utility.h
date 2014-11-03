
#if 0
	#define TRACE(x) x
#else
	#define TRACE(x)
#endif

#define STATE(_f, args...) TRACE(printf("0x%06x:(%04x) " _f, PC << 1, IR, ## args));

#define ljrj_bits(_value, _start, _end) (((_value) << (31 - _start)) >> (31 - (_start - _end)))

static inline uint32_t bits(uint32_t value, uint8_t start, uint8_t end)
{
	return(ljrj_bits(value, start, end));
}

static inline int32_t sext(int32_t value, uint8_t start, uint8_t end)
{
	return(ljrj_bits(value, start, end));
}

