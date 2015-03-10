#include <stdint.h>

typedef struct spd_t {
	unsigned long pc;
	uint32_t *ibuf;
	unsigned int ibuf_index;
	unsigned int ibuf_size;
	char out[1024];
} spd_t;

void spd_init(spd_t *);
void spd_set_input_buffer(spd_t *, unsigned char *, int);
int spd_disassemble(spd_t *);
char *spd_insn_asm(spd_t *);
void spd_set_pc(spd_t *, unsigned long);
