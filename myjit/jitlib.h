/*
 * MyJIT 
 * Copyright (C) 2010 Petr Krajca, <krajcap@inf.upol.cz>
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

#ifndef JITLIB_H
#define JITLIB_H

#define _XOPEN_SOURCE 600

#include "cpu-detect.h"

#include "set.h"

#ifdef JIT_ARCH_COMMON86
#include "common86-specific.h"
#endif

#ifdef JIT_ARCH_SPARC
#include "sparc-specific.h"
#endif

#include "rmap.h"
#include "reg-allocator.h"
#include "jitlib-core.h"

#endif
