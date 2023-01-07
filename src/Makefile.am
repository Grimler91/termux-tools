# Copyright (C) 2022 Termux

# This file is part of termux-tools.

# termux-tools is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# termux-tools is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with termux-tools.  If not, see
# <https://www.gnu.org/licenses/>.

AM_CFLAGS = -Wall -Wextra -pedantic

bin_PROGRAMS = termux-fix-shebang find-undef-syms

termux_fix_shebang_SOURCES = fix-shebang.c arghandling.c

find_undef_syms_SOURCES = find-undef-syms.cpp arghandling.c
find_undef_syms_LDADD = -lelf
