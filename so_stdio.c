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
	stream->err_encountered = OK;
	stream->pid = -1;

	stream->file_offset = lseek(stream->fd, 0, SEEK_CUR);
	DIE(stream->file_offset == -1, "fopen failed");

	stream->file_size = lseek(stream->fd, 0, SEEK_END);
	DIE(stream->file_size == -1, "fopen failed");

	ret = lseek(stream->fd, stream->file_offset, SEEK_SET);
	DIE(ret == -1, "fopen failed");

	return stream;
}

int so_fileno(SO_FILE *stream)
{
	return stream->fd;
}

int so_ferror(SO_FILE *stream)
{
	return stream->err_encountered;
}

int so_feof(SO_FILE *stream)
{
	return (stream->file_offset == stream->file_size + 1);
}

long so_ftell(SO_FILE *stream)
{
	return stream->file_offset;
}

int so_fflush(SO_FILE *stream)
{
	ssize_t ret;

	/* Nothing to do if the last operation wasn't a write */
	if (stream->last_operation != WRITE || !stream->buf_available_offset)
		return OK;

	ret = write_nbytes(stream->fd, stream->buffer,
					(size_t) stream->buf_available_offset);
	if (ret == SO_EOF) {
		stream->err_encountered = ERR;
		return SO_EOF;
	}

	stream->file_size = MAX(stream->file_size, stream->file_offset);
	invalidate_buffer(stream);

	return OK;
}

int so_fseek(SO_FILE *stream, long offset, int whence)
{
	off_t res;

	/* Manage buffer */
	if (stream->last_operation == READ)
		invalidate_buffer(stream);
	else if (stream->last_operation == WRITE)
		so_fflush(stream);

	res = lseek(stream->fd, offset, whence);
	if (res == -1) {
		stream->err_encountered = ERR;
		return SO_EOF;
	}

	stream->file_offset = lseek(stream->fd, 0, SEEK_CUR);
	DIE(stream->file_offset == -1, "fseek failed");

	return OK;
}

int so_fgetc(SO_FILE *stream)
{
	char res;
	off_t check;
	size_t read_bytes;

	/* Flush any data stored in the
	 * buffer, if it's the case
	 */
	so_fflush(stream);

	/* Check if the file was opened
	 * in a reading mode
	 */
	if (!can_read(stream->mode))
		return SO_EOF;

	/* Repopulate buffer with data
	 * if needed
	 */
	if (is_buffer_consumed(stream)) {
		read_bytes = buffered_read(stream);

		if (!read_bytes) {
			stream->file_offset = stream->file_size + 1;
			return SO_EOF;
		}
	}

	res = stream->buffer[stream->buf_data_offset++];

	/* Move file pointer 1 position */
	if (stream->pid == -1) {
		check = lseek(stream->fd, 1, SEEK_CUR);
		DIE(check == -1, "fseek failed");

		stream->file_offset++;
	}

	return (int) res;
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	long demanded_bytes = size * nmemb;
	ssize_t read_bytes = 0;
	size_t total_read_bytes = 0;
	off_t res;

	if (!can_read(stream->mode))
		return 0;

	so_fflush(stream);

	while (demanded_bytes) {
		lseek(stream->fd, 0, SEEK_CUR);
		if (is_buffer_consumed(stream)) {
			read_bytes = buffered_read(stream);

			if (!read_bytes) {
				stream->file_offset = stream->file_size + 1;
				return (total_read_bytes / size);
			}

			if (read_bytes == SO_EOF)
				return 0;
		}

		/* Read the minimum of:
		 *	demanded bytes
		 *	available buffer capacity
		 *	file bytes left
		 */
		read_bytes = MIN(demanded_bytes,
		stream->buf_available_offset - stream->buf_data_offset);

		memcpy(ptr + total_read_bytes,
		stream->buffer + stream->buf_data_offset, read_bytes);

		stream->buf_data_offset += read_bytes;
		demanded_bytes -= read_bytes;
		total_read_bytes += read_bytes;

		/* Set file cursor accordingly */
		if (stream->pid == -1) {
			res = lseek(stream->fd, read_bytes, SEEK_CUR);
			DIE(res == -1, "lseek failed");

			stream->file_offset += read_bytes;
		}
	}

	return nmemb;
}

int so_fputc(int c, SO_FILE *stream)
{
	/* Check if file is opened in
	 * r+, w, w+, a, or a+ mode
	 */
	if (!can_write(stream->mode))
		return 0;

	stream->last_operation = WRITE;
	stream->file_offset++;

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

		/* Get the minimum no. of bytes between
		 * available space in the buffer and
		 * demanded no. of bytes
		 */
		read_bytes = MIN(demanded_bytes,
					BUFLEN - stream->buf_available_offset);

		memcpy(stream->buffer + stream->buf_available_offset,
		ptr + total_read_bytes, read_bytes);

		stream->buf_available_offset += read_bytes;
		stream->last_operation = WRITE;

		demanded_bytes -= read_bytes;
		total_read_bytes += read_bytes;
	}

	if (stream->pid == -1)
		stream->file_offset += total_read_bytes;

	return nmemb;
}

SO_FILE *so_popen(const char *command, const char *type)
{
	int fds[2];
	int read_fd, write_fd;
	enum mode casted_type;
	SO_FILE *stream;

	casted_type = str_to_enum(type);
	stream = malloc(sizeof(*stream));

	/* Create a pipe and a pair of file descriptors for its both ends */
	pipe(fds);
	read_fd = fds[0];
	write_fd = fds[1];

	/* Fork in order to create process */
	stream->pid = fork();

	switch (stream->pid) {
	case -1:
		/* Fork failed */
		free(stream);
		close(read_fd);
		close(write_fd);

		return NULL;
	case 0:
		/* Child process */

		if (casted_type == w) {
			/* Child process will be reading */
			close(write_fd);
			dup2(read_fd, STDIN_FILENO);
			/* Close original descriptor we got from pipe() */
			close(read_fd);
		} else {
			/* Child process will be writing */
			close(read_fd);
			dup2(write_fd, STDOUT_FILENO);
			/* Close original descriptor we got from pipe() */
			close(write_fd);
		}

		/* Execute command via shell */
		execl("/bin/sh", "sh", "-c", command, NULL);

		return NULL;
	default:
		/* Parent */

		/* Build stream */
		stream->buf_data_offset = 0;
		stream->buf_available_offset = 0;
		stream->last_operation = VOID;
		stream->mode = casted_type;
		stream->err_encountered = OK;
		stream->file_offset = 0;
		stream->file_size = BUFLEN;

		if (casted_type == w) {
			stream->fd = write_fd;
			close(read_fd);
		} else {
			stream->fd = read_fd;
			close(write_fd);
		}

		return stream;
	}
}

int so_fclose(SO_FILE *stream)
{
	int ret = OK;

	/* Write any pending data */
	ret = so_fflush(stream);

	if (close(stream->fd) == -1)
		ret = SO_EOF;
	free(stream);

	return ret;
}

int so_pclose(SO_FILE *stream)
{
	int ret, status, pid;

	/* First, we have to close the pipe fds, so retain the pid */
	pid = stream->pid;

	so_fclose(stream);
	ret = waitpid(pid, &status, 0);

	return (ret == -1) ? -1 : 0;
}
