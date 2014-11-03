static inline uint64_t get_dtime(void) {
   	uint32_t hi, lo;
   	
	__asm__ __volatile__ ("xorl %%eax,%%edx\n" : : : "%eax", "%edx");
	__asm__ __volatile__ ("xorl %%eax,%%edx\n" : : : "%eax", "%edx");
	__asm__ __volatile__ ("xorl %%eax,%%edx\n" : : : "%eax", "%edx");
	__asm__ __volatile__ ("xorl %%eax,%%edx\n" : : : "%eax", "%edx");
	__asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
	
	return(((uint64_t)hi << 32) | (uint64_t)lo);
}

static inline uint64_t get_elapsed_dtime(uint64_t start_time)
{
	return(get_dtime() - start_time);
}
