/* find-undef-syms

Copyright (C) 2023 Termux

This file is part of termux-tools.

termux-tools is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or (at
your option) any later version.

termux-tools is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with termux-tools.  If not, see
<https://www.gnu.org/licenses/>.  */

#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "arghandling.h"

#define DT_GNU_HASH 0x6ffffef5
#define DT_VERSYM 0x6ffffff0
#define DT_FLAGS_1 0x6ffffffb
#define DT_VERNEEDED 0x6ffffffe
#define DT_VERNEEDNUM 0x6fffffff

#define DT_AARCH64_BTI_PLT 0x70000001
#define DT_AARCH64_PAC_PLT 0x70000003
#define DT_AARCH64_VARIANT_PCS 0x70000005

#define DF_1_NOW	0x00000001	/* Set RTLD_NOW for this object.  */
#define DF_1_GLOBAL	0x00000002	/* Set RTLD_GLOBAL for this object.  */
#define DF_1_NODELETE	0x00000008	/* Set RTLD_NODELETE for this object.*/

static char const *const usage_message[] =
{ "\
\n\
Processes ELF files and check for undefined symbols that would\n\
otherwise cause runtime errors.\n\
\n\
Options:\n\
\n\
--help                display this help and exit\n\
--version             output version information and exit\n"
};

int parse_file(const char *file_name)
{
	Elf *elf;
	Elf_Scn *scn = NULL;
	GElf_Shdr shdr;
	Elf_Data *data;
	GElf_Sym sym;
	int fd, index, count;

	elf_version(EV_CURRENT);

	fd = open(file_name, O_RDWR);
	if (fd < 0) {
		char* error_message;
		if (asprintf(&error_message, "open(\"%s\")", file_name) == -1)
			error_message = (char*) "open()";
		perror(error_message);
		return 1;
	}

	elf = elf_begin(fd, ELF_C_READ, NULL);

	while ((scn = elf_nextscn(elf, scn)) != NULL) {
		gelf_getshdr(scn, &shdr);
		if (shdr.sh_type == SHT_SYMTAB) {
			/* found a symbol table, go print it. */
			break;
		}
	}

	data = elf_getdata(scn, NULL);
	count = shdr.sh_size / shdr.sh_entsize;

	/* print the symbol names */
	for (index = 0; index < count; ++index) {
		gelf_getsym(data, index, &sym);

		if ((sym.st_info & 0xf) == 0 && sym.st_info >> 4 == 1)
			printf("%s contains undefined symbols: %s\n", file_name,
			       elf_strptr(elf, shdr.sh_link, sym.st_name));
	}
	elf_end(elf);
	close(fd);
        return 0;
}

int main(int argc, char **argv)
{
	int skip_args = 0;
	if (argc == 1 || argmatch(argv, argc, "-help", "--help", 3, NULL, &skip_args)) {
		printf("Usage: %s [OPTION-OR-FILENAME]...\n", argv[0]);
		for (unsigned int i = 0; i < ARRAYELTS(usage_message); i++)
			fputs(usage_message[i], stdout);
		return 0;
	}

	if (argmatch(argv, argc, "-version", "--version", 3, NULL, &skip_args)) {
		printf("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
		printf(("%s\n"
			"%s comes with ABSOLUTELY NO WARRANTY.\n"
			"You may redistribute copies of %s\n"
			"under the terms of the GNU General Public License.\n"
			"For more information about these matters, "
			"see the file named COPYING.\n"),
			COPYRIGHT, PACKAGE_NAME, PACKAGE_NAME);
		return 0;
	}

	for (int i = skip_args+1; i < argc; i++) {
		if (parse_file(argv[i]) != 0)
			return 1;
	}

	return 0;
}
