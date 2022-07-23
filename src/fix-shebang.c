/* fix-shebang.c

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

#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <linux/binfmts.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "arghandling.h"

#define ERR_LEN 256

bool verbose = false;

int check_shebang(char *filename, regex_t shebang_regex)
{
	char error_message[ERR_LEN] = {'\0'};
	char tmpfile_template[] = TERMUX_PREFIX "/tmp/%s.XXXXXX";
	char tmpfile[PATH_MAX];
	char shebang_line[BINPRM_BUF_SIZE];
	unsigned int max_groups = 3;
	regmatch_t matches[max_groups];
	int new_fd;

	FILE *fp = fopen(filename, "r");
	if (!fp) {
		if (snprintf(error_message, ERR_LEN-1, "fopen(\"%s\")",
			     filename) < 0)
			strncpy(error_message, "fopen()", ERR_LEN-1);
		perror(error_message);
		return 1;
	}
	fscanf(fp, "%[^\n]", shebang_line);
	int file_pos = ftell(fp);

	if (regexec(&shebang_regex, shebang_line,
		    max_groups, matches, 0))
		return 0;

	if (strncmp(shebang_line + matches[1].rm_so, "/system", 7) == 0) {
		if (verbose)
			printf(("%s: %s: %s used as interpreter,"
				" will not change shebang\n"),
			       PACKAGE_NAME, filename,
			       shebang_line + matches[1].rm_so);
		return 0;
	}

	if (strncmp(shebang_line + matches[1].rm_so, TERMUX_PREFIX "/bin/",
		    strlen(TERMUX_PREFIX) + 5) == 0 && verbose) {
		printf(("%s: %s: already has a termux shebang\n"),
		       PACKAGE_NAME, filename);
		return 0;
	}

	if (verbose) {
		printf("%s: %s: rewriting %s to #!%s/bin/%s\n",
		       PACKAGE_NAME, filename, shebang_line + matches[0].rm_so,
		       TERMUX_PREFIX, shebang_line + matches[2].rm_so);
	}
	sprintf(tmpfile, tmpfile_template, basename(filename));
	new_fd = mkstemp(tmpfile);
	if (new_fd < 0) {
		if (snprintf(error_message, ERR_LEN-1, "mkstemp(\"%s\")",
			     tmpfile) < 0)
			strncpy(error_message, "mkstemp()", ERR_LEN-1);
		perror(error_message);
		return 1;
	}

	fseek(fp, 0, SEEK_END);
	int content_size = ftell(fp) - file_pos;
	fseek(fp, file_pos, SEEK_SET);

	dprintf(new_fd, "#!%s/bin/%s", TERMUX_PREFIX,
		shebang_line + matches[2].rm_so);

	char *buf = (char *)malloc(content_size + 1);
	if (!buf) {
		fprintf(stderr, "%s: buffer allocation failed\n", PACKAGE_NAME);
		return 1;
	}

	fread(buf, 1, content_size, fp);
	dprintf(new_fd, "%s", buf);
	free(buf);

	fclose(fp);
	close(new_fd);

	if (rename(tmpfile, filename) < 0) {
		if (snprintf(error_message, ERR_LEN-1, "rename(\"%s\", \"%s\")",
			     tmpfile, filename) < 0)
			strncpy(error_message, "rename()", ERR_LEN-1);
		perror(error_message);
		free(buf);
		return 1;
	}
	return 0;
}

int main(int argc, char **argv)
{
	int skip_args = 0;
	if (argc == 1 || argmatch(argv, argc, "-help", "--help", 1, NULL,
				  &skip_args)) {
		printf("Usage: %s filenames...\n", argv[0]);
		printf(("Rewrite shebangs in specified files for running under Termux\n"
			"which is done by rewriting #!*/bin/binary to #!%s/bin/binary.\n"
			"\n"
			"Options:\n"
			"\n"
			"--help          display this help and exit\n"
			"--verbose       print extra info messages\n"
			"--version       output version information and exit\n"
			), TERMUX_PREFIX);
		exit(0);
	}

	if (argmatch(argv, argc, "-version", "--version", 4, NULL,
		     &skip_args)) {
		printf("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
		printf(("%s\n"
			"%s comes with ABSOLUTELY NO WARRANTY.\n"
			"You may redistribute copies of %s\n"
			"under the terms of the GNU General Public License.\n"
			"For more information about these matters, "
			"see the file named COPYING.\n"),
			COPYRIGHT, PACKAGE_NAME, PACKAGE_NAME);
		exit(0);
	}

	if (argmatch(argv, argc, "-verbose", "--verbose", 4, NULL, &skip_args))
		verbose = true;

	regex_t shebang_regex;
	char error_message[ERR_LEN] = {'\0'};
	char *regex_string = "#![:space:]?(.*)/bin/(.*)";
	if (regcomp(&shebang_regex, regex_string, REG_EXTENDED)) {
		strncpy(error_message, "regcomp()", ERR_LEN-1);
		perror(error_message);
		exit(1);
	}

	char filename[PATH_MAX];
	for (int i = skip_args+1; i < argc; i++) {
		if (!realpath(argv[i], filename)) {
			if (snprintf(error_message, ERR_LEN-1,
				     "realpath(\"%s\")", argv[i]) < 0)
				strncpy(error_message, "realpath()", ERR_LEN-1);
			perror(error_message);
			return 1;
		}
		check_shebang(filename, shebang_regex);
	}
	regfree(&shebang_regex);
}
