#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <unistd.h>
#include <fcntl.h>

#include "so_stdio.h"

/* useful macro for handling error codes */
#define DIE(assertion, indicator, call_description)	\
	do {											\
		if (assertion) {							\
			indicator = -1;							\
			fprintf(stderr, "(%s, %d): ",			\
					__FILE__, __LINE__);			\
			perror(call_description);				\
			exit(-1);								\
		}											\
	} while (0)

#define ARRAY_SIZE(a)	(sizeof(a) / sizeof(*(a)))
#define MAX(a, b)		((a > b) ? a : b)
#define MIN(a, b)		((a > b) ? b : a)

#define BUFLEN	4096
#define OK		0

enum operation {READ, WRITE, VOID, ERR};
enum mode {r, rplus, w, wplus, append, aplus};

static const struct {
    enum mode	val;
    const char	*str;
} conversion [] = {
	{r,		"r"},
	{rplus, "r+"},
	{w,		"w"},
	{wplus,	"w+"},
	{append,"a"},
	{aplus,	"a+"}
};

struct _so_file {
	int fd;
	enum mode mode;
	enum operation last_operation;
	char buffer[BUFLEN];
	off_t buf_available_offset;
	off_t buf_data_offset;
	off_t file_offset;
	off_t file_size;
};

enum mode str_to_enum (const char *str);

int compute_open_flags(enum mode mode);

int is_buffer_consumed(SO_FILE *stream);

ssize_t write_nbytes(int fd, const void *buf, size_t nbytes);

ssize_t read_nbytes(int fd, void *buf, size_t nbytes);

size_t buffered_read(SO_FILE *stream);
