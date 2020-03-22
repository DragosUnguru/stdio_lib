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

int can_read(enum mode m)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(read_en); ++i)
		if (m == read_en[i])
			return 1;

	return 0;
}

int can_write(enum mode m)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(write_en); ++i)
		if (m == write_en[i])
			return 1;

	return 0;
}

int is_buffer_consumed(SO_FILE *stream)
{
	return (stream->buf_data_offset == stream->buf_available_offset);
}

int is_buffer_full(SO_FILE *stream)
{
	return (stream->buf_available_offset == BUFLEN);
}

ssize_t write_nbytes(int fd, const void *buf, size_t nbytes)
{
	ssize_t written_bytes;
	ssize_t total_written_bytes;

	total_written_bytes = 0;

	while (nbytes) {
		written_bytes = write(fd, buf + total_written_bytes, nbytes);

		nbytes = MAX(nbytes - written_bytes, 0);
		total_written_bytes += written_bytes;
	}

	return total_written_bytes;
}

ssize_t read_nbytes(int fd, void *buf, size_t nbytes)
{
	ssize_t read_bytes;
	ssize_t total_read_bytes;

	total_read_bytes = 0;

	while (nbytes) {
		read_bytes = read(fd, buf + total_read_bytes, nbytes);

		nbytes = MAX(nbytes - read_bytes, 0);
		total_read_bytes += read_bytes;
	}

	return total_read_bytes;
}

size_t buffered_read(SO_FILE *stream)
{
	size_t available_space;
	ssize_t read_bytes;
	void *buf;

	so_fflush(stream);

	/* If there isn't any useful data in the buffer, read a whole new buf */
	if (is_buffer_consumed(stream)) {
		stream->buf_available_offset = 0;
		stream->buf_data_offset = 0;
	}

	available_space = MIN(stream->file_size - stream->file_offset,
					BUFLEN - stream->buf_available_offset);
	buf = stream->buffer + stream->buf_available_offset;

	/* Don't read already buffered data */
	lseek(stream->fd, stream->buf_data_offset, SEEK_CUR);

	read_bytes = read_nbytes(stream->fd, buf, available_space);
	DIE(read_bytes == SO_EOF, stream->last_operation, "buf_read failed");

	/* Move file pointer to it's original position */
	lseek(stream->fd, -(read_bytes + stream->buf_data_offset), SEEK_CUR);

	stream->buf_available_offset += read_bytes;

	return read_bytes;
}
