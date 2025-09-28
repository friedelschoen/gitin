#include "execute.h"

#include "common.h"
#include "config.h"
#include "getinfo.h"
#include "hprintf.h"

#include <sys/wait.h>
#include <unistd.h>


ssize_t execute(struct executeinfo* params) {
	static char buffer[512];
	ssize_t     n = 0;
	FILE*       cache;
	pipe_t      inpipefd;
	pipe_t      outpipefd;
	pid_t       process;
	ssize_t     readlen;
	int         status;

	emkdirf("!.cache/%s", params->cachename);

	if (!force && (cache = efopen(".!r", ".cache/%s/%x", params->cachename, params->contenthash))) {
		n = 0;
		while ((readlen = fread(buffer, 1, sizeof(buffer), cache)) > 0) {
			fwrite(buffer, 1, readlen, cache);
			n += readlen;
		}
		fclose(cache);

		return n;
	}

	pipe((int*) &inpipefd);
	pipe((int*) &outpipefd);

	if ((process = fork()) == -1) {
		hprintf(stderr, "error: unable to fork process: %w\n");
		exit(100); /* Fatal error, exit with 100 */
	} else if (process == 0) {
		/* Child */
		dup2(outpipefd.read, STDIN_FILENO);
		dup2(inpipefd.write, STDOUT_FILENO);

		/* close unused pipe ends */
		close(outpipefd.write);
		close(inpipefd.read);

		for (int i = 0; i < params->nenviron; i++)
			setenv(params->environ[i * 2], params->environ[i * 2 + 1], 1);

		execlp("sh", "sh", "-c", params->command, NULL);

		hprintf(stderr, "error: unable to exec highlight: %w\n");
		_exit(100); /* Fatal error inside child */
	}

	close(outpipefd.read);
	close(inpipefd.write);

	if (write(outpipefd.write, params->content, params->ncontent) == -1) {
		hprintf(stderr, "error: unable to write to pipe: %w\n");
		exit(100); /* Fatal error, exit with 100 */
		goto wait;
	}

	close(outpipefd.write);

	cache = efopen(".!w+", ".cache/%s/%x", params->cachename, params->contenthash);

	n = 0;
	while ((readlen = read(inpipefd.read, buffer, sizeof buffer)) > 0) {
		fwrite(buffer, readlen, 1, params->fp);
		if (cache)
			fwrite(buffer, readlen, 1, cache);
		n += readlen;
	}

	close(inpipefd.read);
	if (cache)
		fclose(cache);

wait:
	waitpid(process, &status, 0);

	return WEXITSTATUS(status) ? -1 : n;
}
