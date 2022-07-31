/* find-undef-syms

Partly based on termux-elf-cleaner:
Copyright (C) 2017 Fredrik Fornwall
Copyright (C) 2022 Termux

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

#include <elf.h>
#include <fcntl.h>
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

template<typename ElfWord /*Elf{32_Word,64_Xword}*/,
	 typename ElfHeaderType /*Elf{32,64}_Ehdr*/,
	 typename ElfSectionHeaderType /*Elf{32,64}_Shdr*/,
	 typename ElfDynamicSectionEntryType /* Elf{32,64}_Dyn */>
bool check_symbols(uint8_t* bytes, size_t elf_file_size, char const* file_name)
{
	if (sizeof(ElfSectionHeaderType) > elf_file_size) {
		fprintf(stderr, "%s: Elf header for '%s' would end at %zu but file size only %zu\n",
			PACKAGE_NAME, file_name, sizeof(ElfSectionHeaderType), elf_file_size);
		return false;
	}
	ElfHeaderType* elf_hdr = reinterpret_cast<ElfHeaderType*>(bytes);

	size_t last_section_header_byte = elf_hdr->e_shoff + sizeof(ElfSectionHeaderType) * elf_hdr->e_shnum;
	if (last_section_header_byte > elf_file_size) {
		fprintf(stderr, "%s: Section header for '%s' would end at %zu but file size only %zu\n",
			PACKAGE_NAME, file_name, last_section_header_byte, elf_file_size);
		return false;
	}
	ElfSectionHeaderType* section_header_table = reinterpret_cast<ElfSectionHeaderType*>(bytes + elf_hdr->e_shoff);

	/* Iterate over section headers */
	for (unsigned int i = 1; i < elf_hdr->e_shnum; i++) {
		ElfSectionHeaderType* section_header_entry = section_header_table + i;
		if (section_header_entry->sh_type == SHT_SYMTAB) {
			size_t const last_dynamic_section_byte = section_header_entry->sh_offset + section_header_entry->sh_size;
			if (last_dynamic_section_byte > elf_file_size) {
				fprintf(stderr, "%s: Dynamic section for '%s' would end at %zu but file size only %zu\n",
					PACKAGE_NAME, file_name, last_dynamic_section_byte, elf_file_size);
				return false;
			}

			printf("sym: %u\n", section_header_entry->st_name);
		}
	}
	return true;
}

int parse_file(const char *file_name)
{
	int fd = open(file_name, O_RDWR);
	if (fd < 0) {
		char* error_message;
		if (asprintf(&error_message, "open(\"%s\")", file_name) == -1)
			error_message = (char*) "open()";
		perror(error_message);
		return 1;
	}

	struct stat st;
	if (fstat(fd, &st) < 0) {
		perror("fstat()");
		if (close(fd) != 0)
			perror("close()");
		return 1;
	}

	if (st.st_size < (long long) sizeof(Elf32_Ehdr)) {
		if (close(fd) != 0) {
			perror("close()");
			return 1;
		}
		return 0;
	}

	void* mem = mmap(0, st.st_size, PROT_READ | PROT_WRITE,
			 MAP_SHARED, fd, 0);
	if (mem == MAP_FAILED) {
		perror("mmap()");
		if (close(fd) != 0)
			perror("close()");
		return 1;
	}

	uint8_t* bytes = reinterpret_cast<uint8_t*>(mem);
	if (!(bytes[0] == 0x7F && bytes[1] == 'E' &&
	      bytes[2] == 'L' && bytes[3] == 'F')) {
		// Not the ELF magic number.
		munmap(mem, st.st_size);
		if (close(fd) != 0) {
			perror("close()");
			return 1;
		}
		return 0;
	}

	if (bytes[/*EI_DATA*/5] != 1) {
		fprintf(stderr, "%s: Not little endianness in '%s'\n",
			PACKAGE_NAME, file_name);
		munmap(mem, st.st_size);
		if (close(fd) != 0) {
			perror("close()");
			return 1;
		}
		return 0;
	}

	uint8_t const bit_value = bytes[/*EI_CLASS*/4];
	if (bit_value == 1) {
		if (!check_symbols<Elf32_Word, Elf32_Ehdr, Elf32_Shdr,
		    Elf32_Dyn>(bytes, st.st_size, file_name)) {
			munmap(mem, st.st_size);
			if (close(fd) != 0)
				perror("close()");
			return 1;
		}
	} else if (bit_value == 2) {
		if (!check_symbols<Elf64_Xword, Elf64_Ehdr, Elf64_Shdr,
		    Elf64_Dyn>(bytes, st.st_size, file_name)) {
			munmap(mem, st.st_size);
			if (close(fd) != 0)
				perror("close()");
			return 1;
		}
	} else {
		fprintf(stderr, "%s: Incorrect bit value %d in '%s'\n",
			PACKAGE_NAME, bit_value, file_name);
		munmap(mem, st.st_size);
		if (close(fd) != 0)
			perror("close()");
		return 1;
	}

	if (msync(mem, st.st_size, MS_SYNC) < 0) {
		perror("msync()");
		munmap(mem, st.st_size);
		if (close(fd) != 0)
			perror("close()");
		return 1;
	}

	munmap(mem, st.st_size);
	close(fd);
	return 0;
}

int main(int argc, char **argv)
{
	/*
	  1. Create list of symbols in Makefile with:
	    SYMBOLS="$(llvm-readelf -s $($TERMUX_HOST_PLATFORM-clang -print-libgcc-file-name) | grep "FUNC    GLOBAL HIDDEN" | awk '{print $8}')"
	    SYMBOLS+=" $(echo libandroid_{sem_{open,close,unlink},shm{ctl,get,at,dt}})"
	  and sed it in a header to create a vector/array during compile time
	  2. Loop over input files, see if they are libraries
	  3. See if they have any symbols matching "NOTYPE  GLOBAL DEFAULT  UND", and where symbol is one of the ones in symbol-list from above
	 */

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
