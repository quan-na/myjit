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
#include <ctype.h>
#include "io.h"

static unsigned char *ibuf;
static int ibuf_capacity = 16;
static int ibuf_size = 0;

void output_color_normal()
{
	printf("\033[0m");
}

void output_color_white()
{
	printf("\033[1;37m");
}

void output_color_yellow()
{
	printf("\033[0;33m");
}

void output_color_cyan()
{
	printf("\033[1;36m");
}

static int output_print_bytes(unsigned char *data, int size)
{
	int out = 0;
	for (int i = 0; (i < size) && (i < OUTPUT_BYTES_PER_LINE); i++) {
		printf("%02x ", data[i]);
		out++;
	}
	if (out < OUTPUT_BYTES_PER_LINE) {
		for (int i = out; i < OUTPUT_BYTES_PER_LINE; i++)
			printf("   ");
	}
	return out;
}

void output_code(unsigned long addr, unsigned char *data, int size, char *text)
{
	printf("  %04lx: ", addr & 0xffff);
	int out = output_print_bytes(data, size);
	printf("   %s\n", text);
	while (out < size) {
		printf("          ");
		data += out;
		out += output_print_bytes(data, size - out);
		printf("\n");
	}
}

void input_init()
{
	ibuf = malloc(ibuf_capacity);
}

void input_free()
{
	free(ibuf);
}

void input_clear()
{
        ibuf_size = 0;
}

unsigned char *input_buffer()
{
	return ibuf;
}

int input_size()
{
	return ibuf_size;
}

static inline void input_putchar(char c)
{
        if ((ibuf_size + 1) == ibuf_capacity) {
                ibuf_capacity *= 2;
                ibuf = realloc(ibuf, ibuf_capacity);
        }
        ibuf[ibuf_size++] = c;
}

int input_read()
{
        while (!feof(stdin)) {
                char c = getchar();
                if ((c == EOF) || (c == '\r') || (c == '\n')) break;
                input_putchar(c);
        }
        input_putchar('\0');
        if (!feof(stdin)) return 1;
        else return 0;
}

void input_convert()
{
        int len = ibuf_size - 1;
        char clearbuf[len];
        int j = 0;
        for (int i = 0; i < len; i++) {
                char c = tolower(ibuf[i]);
                if ((c >= '0') && (c <= '9')) clearbuf[j++] = c - '0';
                if ((c >= 'a') && (c <= 'f')) clearbuf[j++] = c - 'a' + 10;
        }
        for (int i = 0; i < (j / 2); i++)
                ibuf[i] = clearbuf[i * 2] * 16 + clearbuf[i * 2 + 1];
        ibuf_size = j / 2;
}
