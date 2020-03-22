#include "stdio_internal.h"

SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	SO_FILE *stream;
	enum mode mode_type;
	off_t ret;
	int flags;

	mode_type = str_to_enum(mode);
	flags = compute_open_flags(mode_type);

	if (mode_type == err)
		return NULL;

	stream = malloc(sizeof(*stream));

	/* Build the SO_FILE structure */
	stream->fd = open(pathname, flags, 0644);
	if (stream->fd == -1) {
		free(stream);
		return NULL;
	}

	stream->buf_data_offset = 0;
	stream->buf_available_offset = 0;
	stream->last_operation = VOID;
	stream->mode = mode_type;

	stream->file_offset = lseek(stream->fd, 0, SEEK_CUR);
	DIE(stream->file_offset == -1, stream->last_operation, "fopen failed");

	stream->file_size = lseek(stream->fd, 0, SEEK_END);
	DIE(stream->file_size == -1, stream->last_operation, "fopen failed");

	ret = lseek(stream->fd, stream->file_offset, SEEK_SET);
	DIE(ret == -1, stream->last_operation, "fopen failed");

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
	return (stream->file_offset == stream->file_size);
}

long so_ftell(SO_FILE *stream)
{
	off_t res;

	res = lseek(stream->fd, 0, SEEK_CUR);
	DIE(res == -1, stream->last_operation, "ftell failed");

	return res;
}

int so_fflush(SO_FILE *stream)
{
	ssize_t ret;

	if (stream->last_operation != WRITE || !stream->buf_available_offset)
		return OK;

	ret = write_nbytes(stream->fd, stream->buffer, (size_t) stream->buf_available_offset);
	DIE(ret == SO_EOF, stream->last_operation, "fflush failed");

	stream->file_offset = so_ftell(stream);
	stream->file_size = MAX(stream->file_size, stream->file_offset);
	invalidate_buffer(stream);

	return OK;
}

int so_fseek(SO_FILE *stream, long offset, int whence)
{
	off_t res;

	if (stream->last_operation == READ)
		invalidate_buffer(stream);
	else if (stream->last_operation == WRITE)
		so_fflush(stream);

	res = lseek(stream->fd, offset, whence);
	DIE(res == -1, stream->last_operation, "fseek failed");

	stream->file_offset = so_ftell(stream);

	return OK;
}

int so_fgetc(SO_FILE *stream)
{
	char res;
	off_t check;

	if (so_feof(stream) || !can_read(stream->mode))
		return SO_EOF;

	so_fflush(stream);

	if (is_buffer_consumed(stream))
		buffered_read(stream);

	res = stream->buffer[stream->buf_data_offset++];

	/* Move file pointer 1 position */
	check = lseek(stream->fd, 1, SEEK_CUR);
	DIE(check == -1, stream->last_operation, "fseek failed");

	stream->file_offset++;

	return (int) res;
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	long demanded_bytes = size * nmemb;
	size_t read_bytes = 0;
	size_t total_read_bytes = 0;
	off_t res;

	if (!can_read(stream->mode))
		return 0;

	so_fflush(stream);

	while (demanded_bytes) {
		if (is_buffer_consumed(stream))
			buffered_read(stream);

		if (so_feof(stream))
			return 0;

		read_bytes = MIN(demanded_bytes,
		stream->buf_available_offset - stream->buf_data_offset);

		memcpy(ptr + total_read_bytes,
		stream->buffer + stream->buf_data_offset, read_bytes);

		stream->buf_data_offset += read_bytes;
		demanded_bytes -= read_bytes;
		total_read_bytes += read_bytes;

		/* Set file cursor accordingly */
		res = lseek(stream->fd, read_bytes, SEEK_CUR);
		DIE(res == -1, stream->last_operation, "lseek failed");

		stream->file_offset += read_bytes;
	}

	return nmemb;
}

int so_fputc(int c, SO_FILE *stream)
{
	if (!can_write(stream->mode))
		return 0;

	/*	CAM CIUDAT: TESTUL 13: FPUTC LARGE
	if (stream->last_operation != WRITE)
		invalidate_buffer(stream);
	*/
	stream->last_operation = WRITE;

	if (is_buffer_full(stream))
		so_fflush(stream);

	stream->buffer[stream->buf_available_offset++] = (char) c;

	return c;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	long demanded_bytes = size * nmemb;
	size_t read_bytes = 0;
	size_t total_read_bytes = 0;

	if (!can_write(stream->mode))
		return 0;

	/* Invalidate buffer if needed */
	if (stream->last_operation != WRITE)
		invalidate_buffer(stream);

	while (demanded_bytes) {
		if (is_buffer_full(stream))
			so_fflush(stream);

		read_bytes = MIN(demanded_bytes,
					BUFLEN - stream->buf_available_offset);

		memcpy(stream->buffer + stream->buf_available_offset,
		ptr + total_read_bytes, read_bytes);

		stream->buf_available_offset += read_bytes;
		stream->last_operation = WRITE;

		demanded_bytes -= read_bytes;
		total_read_bytes += read_bytes;
	}

	return nmemb;
}

SO_FILE *so_popen(const char *command, const char *type)
{

}

int so_pclose(SO_FILE *stream)
{
	
}

int so_fclose(SO_FILE *stream)
{
	so_fflush(stream);
	DIE(close(stream->fd) == -1, stream->last_operation, "close failed");
	free(stream);

	return OK;
}
