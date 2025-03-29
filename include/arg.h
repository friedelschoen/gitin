#pragma once

/**
 * This small library, provides a simple way to parse command-line arguments. This
 * library only supports single-character arguments (e.g. short-arguments).
 *
 * These arguments must be stored in `int argc` and `char** argv`. For every argument
 * the code between `ARGBEGIN` and `ARGEND` will be repeated. In this scope, you may
 * call the macro `OPT` to get the current argument. There are also two macros to
 * handle optional and required arguments: `ARGF` and `EARGF(usage)`. `ARGF` is
 * tries to get the argument for the current option and returns `NULL` if the end
 * already is reached. `EARGF(usage)` does the same, but will paste `usage` if no
 * argument is provided. `usage` should be a function-call which exits.
 *
 * Example:
 *
 * void usage(int code) {
 *     fputs("usage: test -C <dir> -fh", stderr);
 *     exit(code);
 * }
 *
 * ARGBEGIN
 * switch (OPT) {
 * 	case 'C':
 * 	   pwd = EARGF(usage(1));
 * 	   break;
 * 	case 'f':
 * 	   force = 1;
 * 	   break;
 * 	case 'h':
 * 	   usage(0);
 * 	default:
 * 	   fprintf(stderr, "error: unknown option '-%c'\n", OPT);
 * 	   usage(1);
 * }
 * ARGEND
 *
 * % test -fC /home -- passes
 * % test -fC       -- failes
 */

#define SHIFT (argc--, argv++)

#define ARGBEGIN                                       \
	for (SHIFT; *argv && *argv[0] == '-'; SHIFT) {     \
		if ((*argv)[1] == '-' && (*argv)[2] == '\0') { \
			SHIFT;                                     \
			break;                                     \
		}                                              \
		for (char* opt = *argv + 1; *opt; opt++) {

#define ARGEND \
	}          \
	}

#define OPT  (*opt)
#define ARGF (argv[1] ? (SHIFT, *argv) : NULL)
#define EARGF(usage) \
	(argv[1] ? (SHIFT, *argv) : (printf("'-%c' requires an argument\n", *opt), usage, ""))
