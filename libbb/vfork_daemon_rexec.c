/* vi: set sw=4 ts=4: */
/*
 * Rexec program for system have fork() as vfork() with foreground option
 *
 * Copyright (C) Vladimir N. Oleynik <dzo@simtreas.ru>
 * Copyright (C) 2003 Russ Dill <Russ.Dill@asu.edu>
 *
 * daemon() portion taken from uClibc:
 *
 * Copyright (c) 1991, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * Modified for uClibc by Erik Andersen <andersee@debian.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <paths.h>
#include "libbb.h"

/* This does a fork/exec in one call, using vfork().  Returns PID of new child,
 * -1 for failure.  Runs argv[0], searching path if that has no / in it. */
pid_t spawn(char **argv)
{
	/* Compiler should not optimize stores here */
	volatile int failed;
	pid_t pid;

	// Be nice to nommu machines.
	failed = 0;
	pid = vfork();
	if (pid < 0) /* error */
		return pid;
	if (!pid) { /* child */
		/* Don't use BB_EXECVP tricks here! */
		execvp(argv[0], argv);

		/* We are (maybe) sharing a stack with blocked parent,
		 * let parent know we failed and then exit to unblock parent
		 * (but don't run atexit() stuff, which would screw up parent.)
		 */
		failed = errno;
		_exit(0);
	}
	/* parent */
	/* Unfortunately, this is not reliable: vfork()
	 * can be equivalent to fork() according to standards */
	if (failed) {
		errno = failed;
		return -1;
	}
	return pid;
}

/* Die with an error message if we can't spawn a child process. */
pid_t xspawn(char **argv)
{
	pid_t pid = spawn(argv);
	if (pid < 0) bb_perror_msg_and_die("%s", *argv);
	return pid;
}



#if 0 //ndef BB_NOMMU
// Die with an error message if we can't daemonize.
void xdaemon(int nochdir, int noclose)
{
	if (daemon(nochdir, noclose))
		bb_perror_msg_and_die("daemon");
}
#endif

#if 0 // def BB_NOMMU
void vfork_daemon_rexec(int nochdir, int noclose, char **argv)
{
	int fd;

	/* Maybe we are already re-execed and come here again? */
	if (re_execed)
		return;

	setsid();

	if (!nochdir)
		xchdir("/");

	if (!noclose) {
		/* if "/dev/null" doesn't exist, bail out! */
		fd = xopen(bb_dev_null, O_RDWR);
		dup2(fd, STDIN_FILENO);
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);
		while (fd > 2)
			close(fd--);
	}

	switch (vfork()) {
	case 0: /* child */
		/* Make certain we are not a session leader, or else we
		 * might reacquire a controlling terminal */
		if (vfork())
			_exit(0);
		/* High-order bit of first char in argv[0] is a hidden
		 * "we have (alrealy) re-execed, don't do it again" flag */
		argv[0][0] |= 0x80;
		execv(CONFIG_BUSYBOX_EXEC_PATH, argv);
		bb_perror_msg_and_die("exec %s", CONFIG_BUSYBOX_EXEC_PATH);
	case -1: /* error */
		bb_perror_msg_and_die("vfork");
	default: /* parent */
		exit(0);
	}
}
#endif /* BB_NOMMU */

#ifdef BB_NOMMU
static void daemon_or_rexec(char **argv)
{
	pid_t pid;
	/* Maybe we are already re-execed and come here again? */
	if (re_execed)
		return;

	pid = vfork();
	if (pid < 0) /* wtf? */
		bb_perror_msg_and_die("vfork");
	if (pid) /* parent */
		exit(0);
	/* child - re-exec ourself */
	/* high-order bit of first char in argv[0] is a hidden
	 * "we have (alrealy) re-execed, don't do it again" flag */
	argv[0][0] |= 0x80;
	execv(CONFIG_BUSYBOX_EXEC_PATH, argv);
	bb_perror_msg_and_die("exec %s", CONFIG_BUSYBOX_EXEC_PATH);
}
#else
static void daemon_or_rexec(void)
{
	pid_t pid;
	pid = fork();
	if (pid < 0) /* wtf? */
		bb_perror_msg_and_die("fork");
	if (pid) /* parent */
		exit(0);
	/* child */
}
#define daemon_or_rexec(argv) daemon_or_rexec()
#endif


/* Due to a #define in libbb.h on MMU systems we actually have 1 argument -
 * char **argv "vanishes" */
void bb_daemonize_or_rexec(int flags, char **argv)
{
	int fd;

	fd = xopen(bb_dev_null, O_RDWR);

	if (flags & DAEMON_CHDIR_ROOT)
		xchdir("/");

	if (flags & DAEMON_DEVNULL_STDIO) {
		close(0);
		close(1);
		close(2);
	}

	while ((unsigned)fd < 2)
		fd = dup(fd); /* have 0,1,2 open at least to /dev/null */

	if (!(flags & DAEMON_ONLY_SANITIZE)) {
		daemon_or_rexec(argv);
		/* if daemonizing, make sure we detach from stdio */
		setsid();
		dup2(fd, 0);
		dup2(fd, 1);
		dup2(fd, 2);
	}
	if (fd > 2)
		close(fd--);
	if (flags & DAEMON_CLOSE_EXTRA_FDS)
		while (fd > 2)
			close(fd--); /* close everything after fd#2 */
}

void bb_sanitize_stdio(void)
{
	bb_daemonize_or_rexec(DAEMON_ONLY_SANITIZE, NULL);
}
