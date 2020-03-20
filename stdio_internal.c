#include "stdio_internal.h"

enum mode str_to_enum(const char *str)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(conversion); ++i)
		if (!strcmp(str, conversion[i].str))
			return conversion[i].val;

	DIE(1, i, "Invalid file mode.");
}

int compute_open_flags(enum mode mode)
{
	int flags = 0;

	if (mode == rplus || mode == wplus)
		flags |= O_RDWR;

	if (mode != r && mode != rplus)
		flags |= O_CREAT;

	if (mode == w || mode == wplus)
		flags |= O_TRUNC;

	if (mode == r || mode == aplus)
		flags |= O_RDONLY;

	if (mode == append || mode == aplus)
		flags |= O_APPEND;

	if (mode == w)
		flags |= O_WRONLY;

	return flags;
}