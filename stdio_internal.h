#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#include "so_stdio.h"

#ifndef STDIO_INTERNAL
#define STDIO_INTERNAL

#ifndef SO_EOF
#define SO_EOF -1
#endif

#ifndef DIE
/* useful macro for handling error codes */
#define DIE(assertion, indicator, call_description)	\
	do {											\
		if (assertion) {							\
			indicator = SO_EOF;						\
			fprintf(stderr, "(%s, %d): ",			\
					__FILE__, __LINE__);			\
			perror(call_description);				\
			exit SO_EOF;							\
		}											\
	} while (0)
#endif

#define ARRAY_SIZE(a)	(sizeof(a) / sizeof(*(a)))
#define MAX(a, b)		((a > b) ? a : b)
#define MIN(a, b)		((a > b) ? b : a)

#define BUFLEN	4096
#define OK		0
#define ERR		7

enum operation { READ, WRITE, VOID };
enum mode { r, rplus, w, wplus, append, aplus, err };

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

static const enum mode read_en[] = { r, rplus, wplus, aplus };
static const enum mode write_en[] = { rplus, w, wplus, append, aplus };

struct _so_file {
	int fd;
	int pid;
	char err_encountered;
	enum mode mode;
	enum operation last_operation;
	char buffer[BUFLEN];
	off_t buf_available_offset;
	off_t buf_data_offset;
	off_t file_offset;
	off_t file_size;
};

/* Converts strings like "r", "r+", "a", etc
 * into defined enum types
 */
enum mode str_to_enum (const char *str);

/* Checks if mode is eligible
 * for reading mode
 */
int can_read(enum mode m);

/* Checks if mode is eligible
 * for writing mode
 */
int can_write(enum mode m);

/* Calculates and returns the flags
 * needed for opening the file
 */
int compute_open_flags(enum mode mode);

/* Checks if we have valid, unused data
 * in the buffer
 */
int is_buffer_consumed(SO_FILE *stream);

/* Checks if the buffer reached it's
 * maximum capacity
 */
int is_buffer_full(SO_FILE *stream);

/* Resets offsets and deletes data
 * from the buffer
 */
void invalidate_buffer(SO_FILE *stream);

/* Writes into 'fd' 'nbytes' from 'buf' using syscall write
 * as many times as needed to complete the job. Returns
 * the number of written bytes
 */
ssize_t write_nbytes(int fd, char *buf, size_t nbytes);

/* Reads 'nbytes' from 'fd' into 'buf' using syscall read
 * as many times as needed to complete the job. Returns
 * the number of read bytes
 */
ssize_t read_nbytes(int fd, void *buf, size_t nbytes);

/* Manages all cursors and offsets for populating
 * the stream's buffer with upcoming data from
 * the file
 */
size_t buffered_read(SO_FILE *stream);

#endif
