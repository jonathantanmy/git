#include "cache.h"
#include "pkt-line.h"
#include "run-command.h"

char packet_buffer[LARGE_PACKET_MAX];
static const char *packet_trace_prefix = "git";
static struct trace_key trace_packet = TRACE_KEY_INIT(PACKET);
static struct trace_key trace_pack = TRACE_KEY_INIT(PACKFILE);

void packet_trace_identity(const char *prog)
{
	packet_trace_prefix = xstrdup(prog);
}

static const char *get_trace_prefix(void)
{
	return in_async() ? "sideband" : packet_trace_prefix;
}

static int packet_trace_pack(const char *buf, unsigned int len, int sideband)
{
	if (!sideband) {
		trace_verbatim(&trace_pack, buf, len);
		return 1;
	} else if (len && *buf == '\1') {
		trace_verbatim(&trace_pack, buf + 1, len - 1);
		return 1;
	} else {
		/* it's another non-pack sideband */
		return 0;
	}
}

static void packet_trace(const char *buf, unsigned int len, int write)
{
	int i;
	struct strbuf out;
	static int in_pack, sideband;

	if (!trace_want(&trace_packet) && !trace_want(&trace_pack))
		return;

	if (in_pack) {
		if (packet_trace_pack(buf, len, sideband))
			return;
	} else if (starts_with(buf, "PACK") || starts_with(buf, "\1PACK")) {
		in_pack = 1;
		sideband = *buf == '\1';
		packet_trace_pack(buf, len, sideband);

		/*
		 * Make a note in the human-readable trace that the pack data
		 * started.
		 */
		buf = "PACK ...";
		len = strlen(buf);
	}

	if (!trace_want(&trace_packet))
		return;

	/* +32 is just a guess for header + quoting */
	strbuf_init(&out, len+32);

	strbuf_addf(&out, "packet: %12s%c ",
		    get_trace_prefix(), write ? '>' : '<');

	/* XXX we should really handle printable utf8 */
	for (i = 0; i < len; i++) {
		/* suppress newlines */
		if (buf[i] == '\n')
			continue;
		if (buf[i] >= 0x20 && buf[i] <= 0x7e)
			strbuf_addch(&out, buf[i]);
		else
			strbuf_addf(&out, "\\%o", buf[i]);
	}

	strbuf_addch(&out, '\n');
	trace_strbuf(&trace_packet, &out);
	strbuf_release(&out);
}

/*
 * If we buffered things up above (we don't, but we should),
 * we'd flush it here
 */
void packet_flush(int fd)
{
	packet_trace("0000", 4, 1);
	write_or_die(fd, "0000", 4);
}

int packet_flush_gently(int fd)
{
	packet_trace("0000", 4, 1);
	if (write_in_full(fd, "0000", 4) == 4)
		return 0;
	return error("flush packet write failed");
}

void packet_buf_flush(struct strbuf *buf)
{
	packet_trace("0000", 4, 1);
	strbuf_add(buf, "0000", 4);
}

static void set_packet_header(char *buf, const int size)
{
	static char hexchar[] = "0123456789abcdef";

	#define hex(a) (hexchar[(a) & 15])
	buf[0] = hex(size >> 12);
	buf[1] = hex(size >> 8);
	buf[2] = hex(size >> 4);
	buf[3] = hex(size);
	#undef hex
}

static void format_packet(struct strbuf *out, const char *fmt, va_list args)
{
	size_t orig_len, n;

	orig_len = out->len;
	strbuf_addstr(out, "0000");
	strbuf_vaddf(out, fmt, args);
	n = out->len - orig_len;

	if (n > LARGE_PACKET_MAX)
		die("protocol error: impossibly long line");

	set_packet_header(&out->buf[orig_len], n);
	packet_trace(out->buf + orig_len + 4, n - 4, 1);
}

static int packet_write_fmt_1(int fd, int gently,
			      const char *fmt, va_list args)
{
	struct strbuf buf = STRBUF_INIT;
	ssize_t count;

	format_packet(&buf, fmt, args);
	count = write_in_full(fd, buf.buf, buf.len);
	if (count == buf.len)
		return 0;

	if (!gently) {
		check_pipe(errno);
		die_errno("packet write with format failed");
	}
	return error("packet write with format failed");
}

void packet_write_fmt(int fd, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	packet_write_fmt_1(fd, 0, fmt, args);
	va_end(args);
}

int packet_write_fmt_gently(int fd, const char *fmt, ...)
{
	int status;
	va_list args;

	va_start(args, fmt);
	status = packet_write_fmt_1(fd, 1, fmt, args);
	va_end(args);
	return status;
}

int packet_writel(int fd, const char *line, ...)
{
	va_list args;
	int err;
	va_start(args, line);
	for (;;) {
		if (!line)
			break;
		if (strlen(line) > LARGE_PACKET_DATA_MAX)
			return -1;
		err = packet_write_fmt_gently(fd, "%s\n", line);
		if (err)
			return err;
		line = va_arg(args, const char*);
	}
	va_end(args);
	return packet_flush_gently(fd);
}

int packet_write_gently(int fd_out, const char *buf, size_t size)
{
	static char packet_write_buffer[LARGE_PACKET_MAX];
	size_t packet_size;

	if (size > sizeof(packet_write_buffer) - 4)
		return error("packet write failed - data exceeds max packet size");

	packet_trace(buf, size, 1);
	packet_size = size + 4;
	set_packet_header(packet_write_buffer, packet_size);
	memcpy(packet_write_buffer + 4, buf, size);
	if (write_in_full(fd_out, packet_write_buffer, packet_size) == packet_size)
		return 0;
	return error("packet write failed");
}

void packet_buf_write(struct strbuf *buf, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	format_packet(buf, fmt, args);
	va_end(args);
}

int write_packetized_from_fd(int fd_in, int fd_out)
{
	static char buf[LARGE_PACKET_DATA_MAX];
	int err = 0;
	ssize_t bytes_to_write;

	while (!err) {
		bytes_to_write = xread(fd_in, buf, sizeof(buf));
		if (bytes_to_write < 0)
			return COPY_READ_ERROR;
		if (bytes_to_write == 0)
			break;
		err = packet_write_gently(fd_out, buf, bytes_to_write);
	}
	if (!err)
		err = packet_flush_gently(fd_out);
	return err;
}

int write_packetized_from_buf(const char *src_in, size_t len, int fd_out)
{
	int err = 0;
	size_t bytes_written = 0;
	size_t bytes_to_write;

	while (!err) {
		if ((len - bytes_written) > LARGE_PACKET_DATA_MAX)
			bytes_to_write = LARGE_PACKET_DATA_MAX;
		else
			bytes_to_write = len - bytes_written;
		if (bytes_to_write == 0)
			break;
		err = packet_write_gently(fd_out, src_in + bytes_written, bytes_to_write);
		bytes_written += bytes_to_write;
	}
	if (!err)
		err = packet_flush_gently(fd_out);
	return err;
}

static int get_packet_data(int fd, char **src_buf, size_t *src_size,
			   void *dst, unsigned size, int options)
{
	ssize_t ret;

	if (fd >= 0 && src_buf && *src_buf)
		die("BUG: multiple sources given to packet_read");

	/* Read up to "size" bytes from our source, whatever it is. */
	if (src_buf && *src_buf) {
		ret = size < *src_size ? size : *src_size;
		memcpy(dst, *src_buf, ret);
		*src_buf += ret;
		*src_size -= ret;
	} else {
		ret = read_in_full(fd, dst, size);
		if (ret < 0)
			die_errno("read error");
	}

	/* And complain if we didn't get enough bytes to satisfy the read. */
	if (ret < size) {
		if (options & PACKET_READ_GENTLE_ON_EOF)
			return -1;

		die("The remote end hung up unexpectedly");
	}

	return ret;
}

static int packet_length(const char *linelen)
{
	int val = hex2chr(linelen);
	return (val < 0) ? val : (val << 8) | hex2chr(linelen + 2);
}

int packet_read(int fd, char **src_buf, size_t *src_len,
		char *buffer, unsigned size, int options)
{
	int len, ret;
	char linelen[4];

	ret = get_packet_data(fd, src_buf, src_len, linelen, 4, options);
	if (ret < 0)
		return ret;
	len = packet_length(linelen);
	if (len < 0)
		die("protocol error: bad line length character: %.4s", linelen);
	if (!len) {
		packet_trace("0000", 4, 0);
		return 0;
	}
	len -= 4;
	if (len >= size)
		die("protocol error: bad line length %d", len);
	ret = get_packet_data(fd, src_buf, src_len, buffer, len, options);
	if (ret < 0)
		return ret;

	if ((options & PACKET_READ_CHOMP_NEWLINE) &&
	    len && buffer[len-1] == '\n')
		len--;

	buffer[len] = 0;
	packet_trace(buffer, len, 0);
	return len;
}

static char *packet_read_line_generic(int fd,
				      char **src, size_t *src_len,
				      int *dst_len)
{
	int len = packet_read(fd, src, src_len,
			      packet_buffer, sizeof(packet_buffer),
			      PACKET_READ_CHOMP_NEWLINE);
	if (dst_len)
		*dst_len = len;
	return (len > 0) ? packet_buffer : NULL;
}

char *packet_read_line(int fd, int *len_p)
{
	return packet_read_line_generic(fd, NULL, NULL, len_p);
}

int packet_read_line_gently(int fd, int *dst_len, char **dst_line)
{
	int len = packet_read(fd, NULL, NULL,
			      packet_buffer, sizeof(packet_buffer),
			      PACKET_READ_CHOMP_NEWLINE|PACKET_READ_GENTLE_ON_EOF);
	if (dst_len)
		*dst_len = len;
	if (dst_line)
		*dst_line = (len > 0) ? packet_buffer : NULL;
	return len;
}

char *packet_read_line_buf(char **src, size_t *src_len, int *dst_len)
{
	return packet_read_line_generic(-1, src, src_len, dst_len);
}

ssize_t read_packetized_to_strbuf(int fd_in, struct strbuf *sb_out)
{
	int packet_len;

	size_t orig_len = sb_out->len;
	size_t orig_alloc = sb_out->alloc;

	for (;;) {
		strbuf_grow(sb_out, LARGE_PACKET_DATA_MAX);
		packet_len = packet_read(fd_in, NULL, NULL,
			/* strbuf_grow() above always allocates one extra byte to
			 * store a '\0' at the end of the string. packet_read()
			 * writes a '\0' extra byte at the end, too. Let it know
			 * that there is already room for the extra byte.
			 */
			sb_out->buf + sb_out->len, LARGE_PACKET_DATA_MAX+1,
			PACKET_READ_GENTLE_ON_EOF);
		if (packet_len <= 0)
			break;
		sb_out->len += packet_len;
	}

	if (packet_len < 0) {
		if (orig_alloc == 0)
			strbuf_release(sb_out);
		else
			strbuf_setlen(sb_out, orig_len);
		return packet_len;
	}
	return sb_out->len - orig_len;
}
