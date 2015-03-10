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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "io.h"

#include "udis86/udis86.h"
#include "sparc/sparc-dis.h"

unsigned long global_addr;

enum {
	AMD64,
	I386,
	SPARC,
	DATA,
	TEXT,
	COMMENT
} global_disassm_mode = DATA;

ud_t dis_amd64;
ud_t dis_i386;
spd_t dis_sparc;

void disassm_amd64(unsigned char *buf, int len)
{
	ud_set_input_buffer(&dis_amd64, buf, len);
	while (ud_disassemble(&dis_amd64)) {
		int len = ud_insn_len(&dis_amd64);
		output_code(global_addr, buf, len, (char *)ud_insn_asm(&dis_amd64));
		buf += len;
		global_addr += len;
	}
}

void disassm_i386(unsigned char *buf, int len)
{
	ud_set_input_buffer(&dis_i386, buf, len);
	while (ud_disassemble(&dis_i386)) {
		int len = ud_insn_len(&dis_i386);
		output_code(global_addr, buf, len, (char *)ud_insn_asm(&dis_i386));
		buf += len;
		global_addr += len;
	}
}

void disassm_text(unsigned char *buf, int len)
{
	output_color_white();
	for (int i = 0; i < len; i++)
		putchar(buf[i]);
	output_color_normal();
	printf("\n");
}

void disassm_comment(unsigned char *buf, int len)
{
	output_color_cyan();
	for (int i = 0; i < len; i++)
		putchar(buf[i]);
	output_color_normal();
	printf("\n");
}

void disassm_data(unsigned char *buf, int len)
{
	output_color_yellow();
	do {
		int out = 0;
		char text[OUTPUT_BYTES_PER_LINE + 1];
		for (int i = 0; i < OUTPUT_BYTES_PER_LINE; i++) {
			if (i < len) {
				out++;
				text[i] = (buf[i] < 32 ? '.' : buf[i]);
			}
		}
		text[out] = '\0';
		output_code(global_addr, buf, out, text);

		global_addr += out;
		buf += out;
		len -= out;
	} while (len > 0);
	output_color_normal();
}

void disassm_sparc(unsigned char *buf, int len)
{
/*
	while (len > 0) {
		output_code(global_addr, buf, 4, "not yet supported");
		buf += 4;
		global_addr += 4;
		len -= 4;
	}
	*/
	spd_set_input_buffer(&dis_sparc, buf, len);
	while (spd_disassemble(&dis_sparc)) {
		int len = 4;
		output_code(global_addr, buf, len, (char *)spd_insn_asm(&dis_sparc));
		buf += len;
		global_addr += len;
	}

}

void disassm_directive(unsigned char *buf)
{
	const char *xbuf = (const char *)buf;
	if (!strcmp(xbuf, ".text")) global_disassm_mode = TEXT;
	else if (!strcmp(xbuf, ".data")) global_disassm_mode = DATA;
	else if (!strcmp(xbuf, ".comment")) global_disassm_mode = COMMENT;
	else if (!strcmp(xbuf, ".amd64")) global_disassm_mode = AMD64;
	else if (!strcmp(xbuf, ".i386")) global_disassm_mode = I386;
	else if (!strcmp(xbuf, ".sparc")) global_disassm_mode = SPARC;
	else if (!strncmp(xbuf, ".addr=", 6)) {
		global_addr = strtol(xbuf + 6, NULL, 16);
		ud_set_pc(&dis_amd64, global_addr);
		ud_set_pc(&dis_i386, global_addr);
		spd_set_pc(&dis_sparc, global_addr);
		if (sizeof(void *) == 8) printf("%016lx:\n", global_addr);
		else printf("%016lx:\n", global_addr);
	}
	else if (!strcmp(xbuf, ".nl")) printf("\n");
	else printf("Unknown directive: %s\n", buf);
} 

int main() 
{
	ud_init(&dis_i386);
	ud_init(&dis_amd64);
	ud_set_mode(&dis_i386, 32);
	ud_set_mode(&dis_amd64, 64);
	ud_set_syntax(&dis_i386, UD_SYN_INTEL);
	ud_set_syntax(&dis_amd64, UD_SYN_INTEL);

	spd_init(&dis_sparc);

	global_addr = 0x0;
	input_init();

	int input;
	do {
		input = input_read();
		if (input_size() - 1 > 0) {
			if (input_buffer()[0] == '.') disassm_directive(input_buffer());
			else {
				if ((global_disassm_mode != TEXT) && (global_disassm_mode != COMMENT)) input_convert();
				switch (global_disassm_mode) {
					case DATA: disassm_data(input_buffer(), input_size()); break;
					case TEXT: disassm_text(input_buffer(), input_size() - 1); break;
					case COMMENT: disassm_comment(input_buffer(), input_size() - 1); break;
					case AMD64: disassm_amd64(input_buffer(), input_size()); break;
					case I386: disassm_i386(input_buffer(), input_size()); break;
					case SPARC: disassm_sparc(input_buffer(), input_size()); break;
				}
			}
		}
		input_clear();		
	} while (input);
	input_free();
	return 0;
}
