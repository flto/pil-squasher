/*
 * Copyright (c) 2019, Linaro Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <elf.h>

static void usage(void)
{
	extern const char *__progname;

	fprintf(stderr, "%s: <mbn output> <mdt header>\n", __progname);
	exit(1);
}

int main(int argc, char **argv)
{
	unsigned char e_ident[EI_NIDENT];
	unsigned int phnum;
	size_t phoff;
	bool is_64bit;
	size_t offset;
	void *segment;
	ssize_t n;
	char *ext;
	int mdt;
	int mbn;
	int bxx;
	int i;

	if (argc != 3)
		usage();

	ext = strstr(argv[2], ".mdt");
	if (!ext)
		errx(1, "%s doesn't end with .mdt", argv[2]);

	mbn = open(argv[1], O_RDONLY);
	if (mbn < 0)
		err(1, "failed to open %s", argv[1]);

	mdt = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (mdt < 0)
		err(1, "failed to open %s", argv[2]);

	read(mbn, e_ident, sizeof(e_ident));
	if (memcmp(e_ident, ELFMAG, SELFMAG))
		errx(1, "not an ELF file %s", argv[2]);

	if (e_ident[EI_CLASS] == ELFCLASS32) {
		Elf32_Ehdr ehdr;

		is_64bit = false;
		pread(mbn, &ehdr, sizeof(ehdr), 0);
		pwrite(mdt, &ehdr, sizeof(ehdr), 0);
		phoff = ehdr.e_phoff;
		phnum = ehdr.e_phnum;
	} else if (e_ident[EI_CLASS] == ELFCLASS64) {
		Elf64_Ehdr ehdr;

		is_64bit = true;
		pread(mbn, &ehdr, sizeof(ehdr), 0);
		pwrite(mdt, &ehdr, sizeof(ehdr), 0);
		phoff = ehdr.e_phoff;
		phnum = ehdr.e_phnum;
	} else
		errx(1, "Unsupported ELF class %d", e_ident[EI_CLASS]);

	for (i = 0; i < phnum; i++) {
		size_t p_filesz, p_offset;
		unsigned long p_flags;

		if (is_64bit) {
			Elf64_Phdr phdr;

			offset = phoff + i * sizeof(phdr);

			pread(mbn, &phdr, sizeof(phdr), offset);
			pwrite(mdt, &phdr, sizeof(phdr), offset);
			p_offset = phdr.p_offset;
			p_filesz = phdr.p_filesz;
			p_flags = phdr.p_flags;
		} else {
			Elf32_Phdr phdr;
			size_t offset;

			offset = phoff + i * sizeof(phdr);

			pread(mbn, &phdr, sizeof(phdr), offset);
			pwrite(mdt, &phdr, sizeof(phdr), offset);
			p_offset = phdr.p_offset;
			p_filesz = phdr.p_filesz;
			p_flags = phdr.p_flags;
		}

		if (!p_filesz)
			continue;

		segment = malloc(p_filesz);

		sprintf(ext, ".b%02d", i);

		n = pread(mbn, segment, p_filesz, p_offset);
		if (n < 0)
			warn("error reading segment %d: %s", i, strerror(errno));
		else  if (n != p_filesz)
			warnx("segment %d is truncated", i);
		bxx = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (bxx < 0)
			warn("failed to open %s", argv[2]);

		write(bxx, segment, p_filesz);

		if (((p_flags >> 24) & 7) == 2 || i == 0)
			write(mdt, segment, p_filesz);

		close(bxx);

		free(segment);
	}

	close(mdt);
	close(mbn);

	return 0;
}
