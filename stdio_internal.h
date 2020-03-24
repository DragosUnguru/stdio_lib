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
#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit SO_EOF;					\
		}									\
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
} conversion[] = {
	{r,			"r"},
	{rplus, 	"r+"},
	{w,			"w"},
	{wplus,		"w+"},
	{append,	"a"},
	{aplus,		"a+"}
};

static const enum mode read_en[] = { r, rplus, wplus, aplus };
static const enum mode write_en[] = { rplus, w, wplus, append, aplus };

/* File structure */
struct _so_file {
	int fd;							/* File descriptor */
	int pid;						/* Process ID if popened */
	char err_encountered;			/* If any error occured */
	enum mode mode;					/* Mode of the file opened (r, w, a, etc) */
	enum operation last_operation;	/* Last operation executed on the buffer */
	char buffer[BUFLEN];			/* Buffer */
	off_t buf_available_offset;		/* Next empty buffer position */
	off_t buf_data_offset;			/* Marks the position in the buffer of in queue data */
	off_t file_offset;				/* Offset from the beggining of the file. A logic file cursor */
	off_t file_size;				/* File size in bytes */
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

#endif	/* STDIO_INTERNAL */
