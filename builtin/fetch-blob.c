#include "cache.h"
#include "builtin.h"
#include "archive.h"
#include "transport.h"
#include "parse-options.h"
#include "pkt-line.h"
#include "sideband.h"

static const char * const fetch_blob_usage[] = {
	N_("git fetch-blob <repository>"),
	NULL
};

static void read_output(int fd)
{
	struct child_process cmd = CHILD_PROCESS_INIT;
	static const char *argv[] = {
		"index-pack", "--stdin", NULL
	};

	char *line;
	const char *p;

	line = packet_read_line(fd, NULL);
	if (skip_prefix(line, "ERR ", &p))
		die("Remote returned error: %s", p);

	cmd.argv = argv;
	cmd.git_cmd = 1;
	cmd.in = fd;
	cmd.out = 0;

	if (run_command(&cmd))
		goto error;

	close(cmd.in);

	return;

error:
	die("problem with running index-pack");
}

static void run_remote(const char *remote_string)
{
	struct remote *remote;
	struct transport *transport;
	int fd[2];

	remote = remote_get(remote_string);
	if (!remote->url[0])
		die(_("git fetch-blob: Remote with no URL"));
	transport = transport_get(remote, remote->url[0]);
	transport_connect(transport, "git-server-endpoint", "git-server-endpoint", fd);

	packet_write_fmt(fd[1], "fetch-blobs\n");
	for (;;) {
		char line[GIT_SHA1_HEXSZ + 3];
		struct object_id oid;
		if (!fgets(line, sizeof(line), stdin)) {
			if (feof(stdin))
				break;
			if (!ferror(stdin))
				die("fgets returned NULL, not EOF, not error!");
			if (errno != EINTR)
				die_errno("fgets");
			clearerr(stdin);
			continue;
		}
		if (get_oid_hex(line, &oid))
			die("expected sha1, got garbage <%s>", line);

		packet_write_fmt(fd[1], "want %s\n", oid_to_hex(&oid));
	}
	packet_flush(fd[1]);

	read_output(fd[0]);
}

int cmd_fetch_blob(int argc, const char **argv, const char *prefix)
{
	struct option local_opts[] = {
		OPT_END()
	};

	argc = parse_options(argc, argv, prefix, local_opts, fetch_blob_usage,
			     0);
	if (argc != 1)
		usage(fetch_blob_usage[0]);

	run_remote(argv[0]);

	return 0;
}
