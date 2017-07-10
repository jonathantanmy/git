#include "cache.h"
#include "pkt-line.h"
#include "revision.h"
#include "run-command.h"

static const char * const server_endpoint_usage[] = {
	N_("git server-endpoint <dir>"),
	NULL
};

/*
 * Returns 1 if all blobs are reachable. If not, returns 0 and stores the hash
 * of one of the unreachable blobs in unreachable.
 */
static int are_all_reachable(const struct object_array *blobs, struct object_id *unreachable)
{
	struct child_process cmd = CHILD_PROCESS_INIT;
	static const char *argv[] = {
		"rev-list", "--max-count=1", "--objects", "--use-bitmap-index", "--stdin", "--not", "--all", NULL,
	};
	int i;
	char buf[41] = {0};

	cmd.argv = argv;
	cmd.git_cmd = 1;
	cmd.in = -1;
	cmd.out = -1;

	if (start_command(&cmd))
		goto error;
	
	for (i = 0; i < blobs->nr; i++) {
		write_in_full(cmd.in, sha1_to_hex(blobs->objects[i].item->oid.hash), 40);
		write_in_full(cmd.in, "\n", 1);
	}
	close(cmd.in);
	cmd.in = -1;

	i = read_in_full(cmd.out, buf, 40);
	close(cmd.out);
	cmd.out = -1;

	if (finish_command(&cmd))
		goto error;

	if (i) {
		if (get_oid_hex(buf, unreachable))
			goto error;
		return 0;
	}

	return 1;

error:
	if (cmd.out >= 0)
		close(cmd.out);
	die("problem with running rev-list");
}

static void send_blobs(const struct object_array *blobs)
{
	struct child_process cmd = CHILD_PROCESS_INIT;
	static const char *argv[] = {
		"pack-objects", "--stdout", NULL
	};
	int i;

	cmd.argv = argv;
	cmd.git_cmd = 1;
	cmd.in = -1;
	cmd.out = 0;

	if (start_command(&cmd))
		goto error;
	
	for (i = 0; i < blobs->nr; i++) {
		write_in_full(cmd.in, sha1_to_hex(blobs->objects[i].item->oid.hash), 40);
		write_in_full(cmd.in, "\n", 1);
	}
	close(cmd.in);
	cmd.in = -1;

	if (finish_command(&cmd))
		goto error;

	return;

error:
	die("problem with running pack-objects");
}

static int fetch_blob(void)
{
	char *line;

	struct object_array wanted_blobs = OBJECT_ARRAY_INIT;
	struct object_id unreachable;

	while ((line = packet_read_line(0, NULL))) {
		const char *arg;
		if (skip_prefix(line, "want ", &arg)) {
			struct object_id oid;
			struct object *obj;
			if (get_oid_hex(arg, &oid)) {
				packet_write_fmt(1, "ERR invalid object ID <%s>", arg);
				return 0;
			}
			obj = parse_object(&oid);
			if (!obj || obj->type != OBJ_BLOB) {
				packet_write_fmt(1, "ERR not our blob <%s>", arg);
				return 0;
			}
			add_object_array(obj, NULL, &wanted_blobs);
		}
	}

	if (!are_all_reachable(&wanted_blobs, &unreachable)) {
		packet_write_fmt(1, "ERR not our blob <%s>", oid_to_hex(&unreachable));
		return 0;
	}

	packet_write_fmt(1, "ACK\n");
	send_blobs(&wanted_blobs);

	return 0;
}

int cmd_main(int argc, const char **argv)
{
	struct option options[] = {
		OPT_END()
	};

	char *line;

	packet_trace_identity("server-endpoint");

	argc = parse_options(argc, argv, NULL, options, server_endpoint_usage, 0);

	if (argc != 1)
		die("must have 1 arg");

	if (!enter_repo(argv[0], 0))
		die("server-endpoint: %s does not appear to be a git repository", argv[0]);

	line = packet_read_line(0, NULL);
	if (!strcmp(line, "fetch-blobs"))
		return fetch_blob();
	die("only fetch-blobs is supported");
}
