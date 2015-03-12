/*
 * MyJIT Disassembler 
 *
 * Copyright (C) 2015 Petr Krajca, <petr.krajca@upol.cz>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

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
