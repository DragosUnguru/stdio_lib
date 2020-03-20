#include "so_stdio.h"
#include "stdio_internal.h"

SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	SO_FILE *stream;
	enum mode mode_type;
	int flags;

	mode_type = str_to_enum(mode);
	flags = compute_open_flags(mode_type);

	stream = malloc(sizeof(*stream));

	/* Build the SO_FILE structure */
	stream->fd = open(pathname, flags, 0760);
	DIE(stream->fd < 0, stream->last_operation, "fopen failed.");

	stream->buf_offset = 0;
	stream->last_operation = VOID;
	stream->mode = mode_type;
	stream->file_offset = lseek(stream->fd, 0, SEEK_CUR);
	DIE(stream->file_offset == -1, stream->last_operation, "fopen failed");

	return stream;
}

int so_fileno(SO_FILE *stream)
{
	return stream->fd;
}

int so_ferror(SO_FILE *stream)
{
	return (stream->last_operation == ERR);
}

int so_feof(SO_FILE *stream)
{
	off_t ret;
	off_t end_position;

	/* Get the file's end offset */
	end_position = lseek(stream->fd, 0, SEEK_END);
	DIE(end_position == -1, stream->last_operation, "feof failed");

	/* Reset file cursor back to it's original position */
	ret = lseek(stream->fd, stream->file_offset, SEEK_SET);
	DIE(ret == -1, stream->last_operation, "feof failed");

	/* Compare */
	return (stream->file_offset == end_position);
}

int so_fflush(SO_FILE *stream)
{
	int ret;

	if (stream->last_operation != WRITE)
		return OK;

	ret = write(stream->fd, stream->buffer, stream->buf_offset + 1);
	DIE(ret == -1, stream->last_operation, "fflush failed");

	stream->last_operation = VOID;
	stream->buf_offset = 0;
	memset(stream->buffer, 0, BUFLEN);

	return OK;
}

int so_fgetc(SO_FILE *stream)
{
	stream->last_operation = READ;

	return OK;
}

int so_fclose(SO_FILE *stream)
{
	DIE(close(stream->fd) == -1, stream->last_operation, "close failed");
	free(stream);

	return OK;
}
