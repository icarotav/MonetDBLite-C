/*
 * The contents of this file are subject to the MonetDB Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.monetdb.org/Legal/MonetDBLicense
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the MonetDB Database System.
 *
 * The Initial Developer of the Original Code is CWI.
 * Portions created by CWI are Copyright (C) 1997-July 2008 CWI.
 * Copyright August 2008-2011 MonetDB B.V.
 * All Rights Reserved.
 */

/**
 * monetdb
 * Fabian Groffen
 * MonetDB Database Administrator's Toolkit
 *
 * A group of MonetDB servers in a dbfarm can be under control of
 * Merovingian, a daemon which by itself does not allow any user
 * interaction.  The monetdb utility is designed to be the interface for
 * the DBA to the dbfarm and its vincinity.  Creating or deleting
 * databases next to retrieving status information about them are the
 * primary goals of this tool.
 */

#define TOOLKIT_VERSION   "0.8"

#include "monetdb_config.h"
#include "utils.h"
#include "properties.h"
#include "glob.h"
#include "database.h"
#include "control.h"
#include <msabaoth.h>
#include <mutils.h>
#include <stdlib.h> /* exit, getenv, qsort */
#include <stdarg.h>	/* variadic stuff */
#include <stdio.h> /* fprintf, rename */
#include <string.h> /* strerror */
#include <sys/stat.h> /* mkdir, stat, umask */
#include <sys/types.h> /* mkdir, readdir */
#include <dirent.h> /* readdir */
#include <unistd.h> /* stat, rmdir, unlink, ioctl */
#include <time.h> /* strftime */
#include <sys/socket.h> /* socket */
#ifdef HAVE_SYS_UN_H
#include <sys/un.h> /* sockaddr_un */
#endif
#ifdef HAVE_STROPTS_H
#include <stropts.h> /* ioctl */
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_TERMIOS_H
#include <termios.h> /* TIOCGWINSZ/TIOCSWINSZ */
#endif
#include <errno.h>

static char *mero_host = NULL;
static int mero_port = -1;
static char *mero_pass = NULL;
static char monetdb_quiet = 0;
static int TERMWIDTH = 80;  /* default to classic terminal width */

static void
command_help(int argc, char *argv[])
{
	if (argc < 2) {
		printf("Usage: monetdb [options] command [command-options-and-arguments]\n");
		printf("  where command is one of:\n");
		printf("    create, destroy, lock, release\n");
		printf("    status, start, stop, kill\n");
		printf("    set, get, inherit\n");
		printf("    discover, help, version\n");
		printf("  options can be:\n");
		printf("    -q       suppress status output\n");
		printf("    -h host  hostname to contact (remote merovingian)\n");
		printf("    -p port  port to contact\n");
		printf("    -P pass  password to use to login at remote merovingian\n");
		printf("  use the help command to get help for a particular command\n");
	} else if (strcmp(argv[1], "create") == 0) {
		printf("Usage: monetdb create database [database ...]\n");
		printf("  Initialises a new database in the MonetDB Server.  A\n");
		printf("  database created with this command makes it available\n");
		printf("  for use, however in maintenance mode (see monetdb lock).\n");
	} else if (strcmp(argv[1], "destroy") == 0) {
		printf("Usage: monetdb destroy [-f] database [database ...]\n");
		printf("  Removes the given database, including all its data and\n");
		printf("  logfiles.  Once destroy has completed, all data is lost.\n");
		printf("  Be careful when using this command.\n");
		printf("Options:\n");
		printf("  -f  do not ask for confirmation, destroy right away\n");
	} else if (strcmp(argv[1], "lock") == 0) {
		printf("Usage: monetdb lock database [database ...]\n");
		printf("  Puts the given database in maintenance mode.  A database\n");
		printf("  under maintenance can only be connected to by the DBA.\n");
		printf("  A database which is under maintenance is not started\n");
		printf("  automatically.  Use the \"release\" command to bring\n");
		printf("  the database back for normal usage.\n");
	} else if (strcmp(argv[1], "release") == 0) {
		printf("Usage: monetdb release database [database ...]\n");
		printf("  Brings back a database from maintenance mode.  A released\n");
		printf("  database is available again for normal use.  Use the\n");
		printf("  \"lock\" command to take a database under maintenance.\n");
	} else if (strcmp(argv[1], "status") == 0) {
		printf("Usage: monetdb status [-lc] [expression ...]\n");
		printf("  Shows the state of a given glob-style database match, or\n");
		printf("  all known if none given.  Instead of the normal mode, a\n");
		printf("  long and crash mode control what information is displayed.\n");
		printf("Options:\n");
		printf("  -l  extended information listing\n");
		printf("  -c  crash statistics listing\n");
		printf("  -s  only show databases matching a state, combination\n");
		printf("      possible from r (running), s (stopped), c (crashed)\n");
		printf("      and l (locked).\n");
	} else if (strcmp(argv[1], "start") == 0) {
		printf("Usage: monetdb start [-a] database [database ...]\n");
		printf("  Starts the given database, if the MonetDB Database Server\n");
		printf("  is running.\n");
		printf("Options:\n");
		printf("  -a  start all known databases\n");
	} else if (strcmp(argv[1], "stop") == 0) {
		printf("Usage: monetdb stop [-a] database [database ...]\n");
		printf("  Stops the given database, if the MonetDB Database Server\n");
		printf("  is running.\n");
		printf("Options:\n");
		printf("  -a  stop all known databases\n");
	} else if (strcmp(argv[1], "kill") == 0) {
		printf("Usage: monetdb kill [-a] database [database ...]\n");
		printf("  Kills the given database, if the MonetDB Database Server\n");
		printf("  is running.  Note: killing a database should only be done\n");
		printf("  as last resort to stop a database.  A database being\n");
		printf("  killed may end up with data loss.\n");
		printf("Options:\n");
		printf("  -a  kill all known databases\n");
	} else if (strcmp(argv[1], "set") == 0) {
		printf("Usage: monetdb set property=value database [database ...]\n");
		printf("  sets property to value for the given database\n");
		printf("  for a list of properties, use `monetdb get all`\n");
	} else if (strcmp(argv[1], "get") == 0) {
		printf("Usage: monetdb get <\"all\" | property,...> [database ...]\n");
		printf("  gets value for property for the given database, or\n");
		printf("  retrieves all properties for the given database\n");
	} else if (strcmp(argv[1], "inherit") == 0) {
		printf("Usage: monetdb inherit property database [database ...]\n");
		printf("  unsets property, reverting to its inherited value from\n");
		printf("  the default configuration for the given database\n");
	} else if (strcmp(argv[1], "discover") == 0) {
		printf("Usage: monetdb discover [expression]\n");
		printf("  Lists the remote databases discovered by the MonetDB\n");
		printf("  Database Server.  Databases in this list can be connected\n");
		printf("  to as well.  If expression is given, all entries are\n");
		printf("  matched against a limited glob-style expression.\n");
	} else if (strcmp(argv[1], "help") == 0) {
		printf("Yeah , help on help, how desparate can you be? ;)\n");
	} else if (strcmp(argv[1], "version") == 0) {
		printf("Usage: monetdb version\n");
		printf("  prints the version of this monetdb utility\n");
	} else {
		printf("help: unknown command: %s\n", argv[1]);
	}
}

static void
command_version(void)
{
	printf("MonetDB Database Server Toolkit v%s (%s)\n",
			TOOLKIT_VERSION, MONETDB_RELEASE);
}

static int
cmpsabdb(const void *p1, const void *p2)
{
	const sabdb *q1 = *(sabdb* const*)p1;
	const sabdb *q2 = *(sabdb* const*)p2;

	return strcmp(q1->dbname, q2->dbname);
}

/**
 * Helper function to perform the equivalent of
 * msab_getStatus(&stats, x) but over the network.
 */
static char *
MEROgetStatus(sabdb **ret, char *database)
{
	sabdb *orig;
	sabdb *stats;
	sabdb *w = NULL;
	size_t swlen = 50;
	size_t swpos = 0;
	sabdb **sw = malloc(sizeof(sabdb *) * swlen);
	char *p;
	char *buf;
	char *e;
	
	if (database == NULL)
		database = "#all";

	e = control_send(&buf, mero_host, mero_port,
			database, "status", 1, mero_pass);
	if (e != NULL)
		return(e);

	orig = NULL;
	if ((p = strtok(buf, "\n")) != NULL) {
		if (strcmp(p, "OK") != 0) {
			p = strdup(p);
			free(buf);
			return(p);
		}
		for (swpos = 0; (p = strtok(NULL, "\n")) != NULL; swpos++) {
			e = msab_deserialise(&stats, p);
			if (e != NULL) {
				printf("WARNING: failed to parse response from "
						"merovingian: %s\n", e);
				free(e);
				continue;
			}
			if (swpos == swlen)
				sw = realloc(sw, sizeof(sabdb *) * (swlen = swlen * 2));
			sw[swpos] = stats;
		}
	}

	free(buf);

	if (swpos > 1) {
		qsort(sw, swpos, sizeof(sabdb *), cmpsabdb);
		orig = w = sw[0];
		for (swlen = 1; swlen < swpos; swlen++)
			w = w->next = sw[swlen];
	} else if (swpos == 1) {
		orig = sw[0];
		orig->next = NULL;
	}

	free(sw);

	*ret = orig;
	return(NULL);
}

static void
printStatus(sabdb *stats, int mode, int twidth)
{
	sabuplog uplog;
	char *e;

	if ((e = msab_getUplogInfo(&uplog, stats)) != NULL) {
		fprintf(stderr, "status: internal error: %s\n", e);
		free(e);
		return;
	}

	if (mode == 1) {
		/* short one-line (default) mode */
		char *state;
		char uptime[12];
		char avg[8];
		char *crash;
		char *dbname;

		switch (stats->state) {
			case SABdbRunning:
				state = "running";
			break;
			case SABdbCrashed:
				state = "crashed";
			break;
			case SABdbInactive:
				state = "stopped";
			break;
			default:
				state = "unknown";
			break;
		}
		/* override if locked for brevity */
		if (stats->locked == 1)
			state = "locked ";

		if (uplog.lastcrash == -1) {
			crash = "-";
		} else {
			struct tm *t;
			crash = alloca(sizeof(char) * 20);
			t = localtime(&uplog.lastcrash);
			strftime(crash, 20, "%Y-%m-%d %H:%M:%S", t);
		}

		if (stats->state != SABdbRunning) {
			uptime[0] = '\0';
		} else {
			secondsToString(uptime, time(NULL) - uplog.laststart, 3);
		}

		/* cut too long database names */
		dbname = alloca(sizeof(char) * (twidth + 1));
		abbreviateString(dbname, stats->dbname, twidth);
		/* dbname | state | uptime | health */
		secondsToString(avg, uplog.avguptime, 1);
			printf("%-*s  %s %12s", twidth,
					dbname, state, uptime);
		if (uplog.startcntr)
			printf("  %3d%%, %3s  %s",
					100 - (uplog.crashcntr * 100 / uplog.startcntr),
					avg, crash);
		printf("\n");
	} else if (mode == 2) {
		/* long mode */
		char *state;
		sablist *entry;
		char up[32];
		struct tm *t;

		switch (stats->state) {
			case SABdbRunning:
				state = "running";
			break;
			case SABdbCrashed:
				state = "crashed";
			break;
			case SABdbInactive:
				state = "stopped";
			break;
			default:
				state = "unknown";
			break;
		}

		printf("%s:\n", stats->dbname);
		printf("  location: %s\n", stats->path);
		printf("  database name: %s\n", stats->dbname);
		printf("  state: %s\n", state);
		printf("  locked: %s\n", stats->locked == 1 ? "yes" : "no");
		entry = stats->scens;
		printf("  scenarios:");
		if (entry == NULL) {
			printf(" (none)");
		} else while (entry != NULL) {
			printf(" %s", entry->val);
			entry = entry->next;
		}
		printf("\n");
		entry = stats->conns;
		printf("  connections:");
		if (entry == NULL) {
			printf(" (none)");
		} else while (entry != NULL) {
			printf(" %s", entry->val);
			entry = entry->next;
		}
		printf("\n");
		printf("  start count: %d\n  stop count: %d\n  crash count: %d\n",
				uplog.startcntr, uplog.stopcntr, uplog.crashcntr);
		if (stats->state == SABdbRunning) {
			secondsToString(up, time(NULL) - uplog.laststart, 999);
			printf("  current uptime: %s\n", up);
		}
		secondsToString(up, uplog.avguptime, 999);
		printf("  average uptime: %s\n", up);
		secondsToString(up, uplog.maxuptime, 999);
		printf("  maximum uptime: %s\n", up);
		secondsToString(up, uplog.minuptime, 999);
		printf("  minimum uptime: %s\n", up);
		if (uplog.lastcrash != -1) {
			t = localtime(&uplog.lastcrash);
			strftime(up, 32, "%Y-%m-%d %H:%M:%S", t);
		} else {
			sprintf(up, "(unknown)");
		}
		printf("  last start with crash: %s\n", up);
		if (uplog.laststart != -1) {
			t = localtime(&uplog.laststart);
			strftime(up, 32, "%Y-%m-%d %H:%M:%S", t);
		} else {
			sprintf(up, "(unknown)");
		}
		printf("  last start: %s\n", up);
		printf("  average of crashes in the last start attempt: %d\n",
				uplog.crashavg1);
		printf("  average of crashes in the last 10 start attempts: %.2f\n",
				uplog.crashavg10);
		printf("  average of crashes in the last 30 start attempts: %.2f\n",
				uplog.crashavg30);
	} else {
		/* this shows most used properties, and is shown also for modes
		 * that are added but we don't understand (yet) */
		char buf[64];
		char min[8], avg[8], max[8];
		struct tm *t;
		/* dbname, status -- since, crash averages */

		switch (stats->state) {
			case SABdbRunning: {
				char up[32];
				t = localtime(&uplog.laststart);
				strftime(buf, 64, "up since %Y-%m-%d %H:%M:%S, ", t);
				secondsToString(up, time(NULL) - uplog.laststart, 999);
				strcat(buf, up);
			} break;
			case SABdbCrashed:
				t = localtime(&uplog.lastcrash);
				strftime(buf, 64, "crashed on %Y-%m-%d %H:%M:%S", t);
			break;
			case SABdbInactive:
				snprintf(buf, 64, "not running");
			break;
			default:
				snprintf(buf, 64, "unknown");
			break;
		}
		if (stats->locked == 1)
			strcat(buf, ", locked");
		printf("database %s, %s\n", stats->dbname, buf);
		printf("  crash average: %d.00 %.2f %.2f (over 1, 15, 30 starts) "
				"in total %d crashes\n",
				uplog.crashavg1, uplog.crashavg10, uplog.crashavg30,
				uplog.crashcntr);
		secondsToString(min, uplog.minuptime, 1);
		secondsToString(avg, uplog.avguptime, 1);
		secondsToString(max, uplog.maxuptime, 1);
		printf("  uptime stats (min/avg/max): %s/%s/%s over %d runs\n",
				min, avg, max, uplog.stopcntr);
	}
}

static sabdb *
globMatchDBS(int argc, char *argv[], sabdb **orig, char *cmd)
{
	sabdb *w = NULL;
	sabdb *top = NULL;
	sabdb *prev;
	sabdb *stats;
	int i;
	char matched;

	for (i = 1; i < argc; i++) {
		matched = 0;
		if (argv[i] != NULL) {
			prev = NULL;
			for (stats = *orig; stats != NULL; stats = stats->next) {
				if (glob(argv[i], stats->dbname)) {
					matched = 1;
					/* move out of orig into w, such that we can't
					 * get double matches in the same output list
					 * (as side effect also avoids a double free
					 * lateron) */
					if (w == NULL) {
						top = w = stats;
					} else {
						w = w->next = stats;
					}
					if (prev == NULL) {
						*orig = stats->next;
						/* little hack to revisit the now top of the
						 * list */
						w->next = *orig;
						stats = w;
						continue;
					} else {
						prev->next = stats->next;
						stats = prev;
					}
				}
				prev = stats;
			}
			if (w != NULL)
				w->next = NULL;
			if (matched == 0) {
				fprintf(stderr, "%s: no such database: %s\n", cmd, argv[i]);
				argv[i] = NULL;
			}
		}
	}
	return(top);
}

/**
 * Helper function to run over the sabdb list and perform merocmd for
 * the value and reporting status on the performed command.  Either a
 * message is printed when success, or when premsg is not NULL, premsg
 * is printed before the action, and "done" printed afterwards.
 */
static void
simple_argv_cmd(char *cmd, sabdb *dbs, char *merocmd,
		char *successmsg, char *premsg)
{
	int state = 0;        /* return status */
	int hadwork = 0;      /* if we actually did something */
	char *ret;
	char *out;

	/* do for each listed database */
	for (; dbs != NULL; dbs = dbs->next) {
		if (premsg != NULL && !monetdb_quiet) {
			printf("%s '%s'... ", premsg, dbs->dbname);
			fflush(stdout);
		}

		ret = control_send(&out, mero_host, mero_port,
				dbs->dbname, merocmd, 0, mero_pass);

		if (ret != NULL) {
			if (premsg != NULL && !monetdb_quiet)
				printf("FAILED\n");
			fprintf(stderr, "%s: failed to perform command: %s\n",
					cmd, ret);
			free(ret);
			exit(2);
		}

		if (strcmp(out, "OK") == 0) {
			if (!monetdb_quiet) {
				if (premsg != NULL) {
					printf("done\n");
				} else {
					printf("%s: %s\n", successmsg, dbs->dbname);
				}
			}
		} else {
			if (premsg != NULL && !monetdb_quiet)
				printf("FAILED\n");
			fprintf(stderr, "%s: %s\n", cmd, out);
			free(out);

			state |= 1;
		}

		hadwork = 1;
	}

	if (hadwork == 0) {
		char *argv[2] = { "monetdb", cmd };
		command_help(2, argv);
		exit(1);
	}

	if (state != 0)
		exit(state);
}

/**
 * Helper function for commands in their most general form: no option
 * flags and just pushing all (database) arguments over to merovingian
 * for performing merocmd action.
 */
static void
simple_command(int argc, char *argv[], char *merocmd, char *successmsg, char glob)
{
	int i;
	sabdb *orig = NULL;
	sabdb *stats = NULL;
	char *e;

	if (argc == 1) {
		/* print help message for this command */
		command_help(2, &argv[-1]);
		exit(1);
	}
	
	/* walk through the arguments and hunt for "options" */
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--") == 0) {
			argv[i] = NULL;
			break;
		}
		if (argv[i][0] == '-') {
			fprintf(stderr, "%s: unknown option: %s\n", argv[0], argv[i]);
			command_help(argc + 1, &argv[-1]);
			exit(1);
		}
	}

	if (glob) {
		if ((e = MEROgetStatus(&orig, NULL)) != NULL) {
			fprintf(stderr, "%s: internal error: %s\n", argv[0], e);
			free(e);
			exit(2);
		}
		stats = globMatchDBS(argc, argv, &orig, argv[0]);
		msab_freeStatus(&orig);
		orig = stats;

		if (orig == NULL)
			exit(1);
	} else {
		for (i = 1; i < argc; i++) {
			if (argv[i] != NULL) {
				/* maintain input order */
				if (orig == NULL) {
					stats = orig = malloc(sizeof(sabdb));
				} else {
					stats = stats->next = malloc(sizeof(sabdb));
				}
				memset(stats, 0, sizeof(sabdb));
				stats->dbname = strdup(argv[i]);
			}
		}
	}

	simple_argv_cmd(argv[0], orig, merocmd, successmsg, NULL);
	msab_freeStatus(&orig);
}

static void
command_status(int argc, char *argv[])
{
	int doall = 1; /* we default to showing all */
	int mode = 1;  /* 0=crash, 1=short, 2=long */
	char *state = "rscl"; /* contains states to show */
	int i;
	char *p;
	char *e;
	sabdb *stats;
	sabdb *orig;
	int t;
	int dbwidth = 0;
	int twidth = TERMWIDTH;

	if (argc == 0) {
		exit(2);
	}

	/* time to collect some option flags */
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			for (p = argv[i] + 1; *p != '\0'; p++) {
				switch (*p) {
					case 'c':
						mode = 0;
					break;
					case 'l':
						mode = 2;
					break;
					case 's':
						if (*(p + 1) != '\0') {
							state = ++p;
						} else if (i + 1 < argc && argv[i + 1][0] != '-') {
							state = argv[++i];
						} else {
							fprintf(stderr, "status: -s needs an argument\n");
							command_help(2, &argv[-1]);
							exit(1);
						}
						for (p = state; *p != '\0'; p++) {
							switch (*p) {
								case 'r': /* running (started) */
								case 's': /* stopped */
								case 'c': /* crashed */
								case 'l': /* locked */
								break;
								default:
									fprintf(stderr, "status: unknown flag for -s: -%c\n", *p);
									command_help(2, &argv[-1]);
									exit(1);
								break;
							}
						}
						p--;
					break;
					case '-':
						if (p[1] == '\0') {
							if (argc - 1 > i) 
								doall = 0;
							i = argc;
							break;
						}
					default:
						fprintf(stderr, "status: unknown option: -%c\n", *p);
						command_help(2, &argv[-1]);
						exit(1);
					break;
				}
			}
			/* make this option no longer available, for easy use
			 * lateron */
			argv[i] = NULL;
		} else {
			doall = 0;
		}
	}

	if ((e = MEROgetStatus(&orig, NULL)) != NULL) {
		fprintf(stderr, "status: internal error: %s\n", e);
		free(e);
		exit(2);
	}

	/* look at the arguments and evaluate them based on a glob (hence we
	 * listed all databases before) */
	if (doall != 1) {
		stats = globMatchDBS(argc, argv, &orig, "status");
		msab_freeStatus(&orig);
		orig = stats;
	}
	/* calculate width, BUG: SABdbState selection is only done at
	 * printing */
	for (stats = orig; stats != NULL; stats = stats->next) {
		if ((t = strlen(stats->dbname)) > dbwidth)
			dbwidth = t;
	}

	if (mode == 1 && orig != NULL) {
		/* print header for short mode, state -- last crash = 54 chars */
		twidth -= 54;
		if (twidth < 6)
			twidth = 6;
		if (dbwidth < 14)
			dbwidth = 14;
		if (dbwidth < twidth)
			twidth = dbwidth;
		printf("%*sname%*s  ",
				twidth - 4 /* name */ - ((twidth - 4) / 2), "",
				(twidth - 4) / 2, "");
		printf(" state     uptime       health       last crash\n");
	}

	for (p = state; *p != '\0'; p++) {
		int curLock = 0;
		SABdbState curMode = SABdbIllegal;
		switch (*p) {
			case 'r':
				curMode = SABdbRunning;
			break;
			case 's':
				curMode = SABdbInactive;
			break;
			case 'c':
				curMode = SABdbCrashed;
			break;
			case 'l':
				curLock = 1;
			break;
		}
		stats = orig;
		while (stats != NULL) {
			if (stats->locked == curLock &&
					(curLock == 1 || 
					 (curLock == 0 && stats->state == curMode)))
				printStatus(stats, mode, twidth);
			stats = stats->next;
		}
	}

	if (orig != NULL)
		msab_freeStatus(&orig);
}

static int
cmpurl(const void *p1, const void *p2)
{
	const char *q1 = *(char* const*)p1;
	const char *q2 = *(char* const*)p2;

	if (strncmp("mapi:monetdb://", q1, 15) == 0)
		q1 += 15;
	if (strncmp("mapi:monetdb://", q2, 15) == 0)
		q2 += 15;
	return strcmp(q1, q2);
}

static void
command_discover(int argc, char *argv[])
{
	char path[8096];
	char *buf;
	char *p, *q;
	size_t twidth = TERMWIDTH;
	char location[twidth + 1];
	char *match = NULL;
	size_t numlocs = 50;
	size_t posloc = 0;
	size_t loclen = 0;
	char **locations = malloc(sizeof(char*) * numlocs);

	if (argc == 0) {
		exit(2);
	} else if (argc > 2) {
		/* print help message for this command */
		command_help(2, &argv[-1]);
		exit(1);
	} else if (argc == 2) {
		match = argv[1];
	}

 	/* Send the pass phrase to unlock the information available in
	 * merovingian.  Anelosimus eximius is a social species of spiders,
	 * which help each other, just like merovingians do among each
	 * other. */
	p = control_send(&buf, mero_host, mero_port,
			"anelosimus", "eximius", 1, mero_pass);
	if (p != NULL) {
		printf("%s: %s\n", argv[0], p);
		free(p);
		exit(2);
	}

	if ((p = strtok(buf, "\n")) != NULL) {
		if (strcmp(p, "OK") != 0) {
			fprintf(stderr, "%s: %s\n", argv[0], p);
			exit(1);
		}
		while ((p = strtok(NULL, "\n")) != NULL) {
			if ((q = strchr(p, '\t')) == NULL) {
				/* doesn't look correct */
				printf("%s: WARNING: discarding incorrect line: %s\n",
						argv[0], p);
				continue;
			}
			*q++ = '\0';

			snprintf(path, sizeof(path), "%s%s", q, p);

			if (match == NULL || glob(match, path)) {
				/* cut too long location name */
				abbreviateString(location, path, twidth);
				/* store what we found */
				if (posloc == numlocs)
					locations = realloc(locations,
							sizeof(char *) * (numlocs = numlocs * 2));
				locations[posloc++] = strdup(location);
				if (strlen(location) > loclen)
					loclen = strlen(location);
			}
		}
	}

	free(buf);

	if (posloc > 0) {
		printf("%*slocation\n",
				(int)(loclen - 8 /* "location" */ - ((loclen - 8) / 2)), "");
		qsort(locations, posloc, sizeof(char *), cmpurl);
		for (loclen = 0; loclen < posloc; loclen++) {
			printf("%s\n", locations[loclen]);
			free(locations[loclen]);
		}
	}

	free(locations);
}

typedef enum {
	START = 0,
	STOP,
	KILL
} startstop;

static void
command_startstop(int argc, char *argv[], startstop mode)
{
	int doall = 0;
	int i;
	char *e;
	sabdb *orig = NULL;
	sabdb *stats;
	sabdb *prev;
	char *type = NULL;
	char *action = NULL;
	char *p;

	switch (mode) {
		case START:
			type = "start";
			action = "starting database";
		break;
		case STOP:
			type = "stop";
			action = "stopping database";
		break;
		case KILL:
			type = "kill";
			action = "killing database";
		break;
	}

	if (argc == 1) {
		/* print help message for this command */
		command_help(2, &argv[-1]);
		exit(1);
	} else if (argc == 0) {
		exit(2);
	}

	/* time to collect some option flags */
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			for (p = argv[i] + 1; *p != '\0'; p++) {
				switch (*p) {
					case 'a':
						doall = 1;
					break;
					case '-':
						if (p[1] == '\0') {
							if (argc - 1 > i) 
								doall = 0;
							i = argc;
							break;
						}
					default:
						fprintf(stderr, "%s: unknown option: -%c\n", type, *p);
						command_help(2, &argv[-1]);
						exit(1);
					break;
				}
			}
			/* make this option no longer available, for easy use
			 * lateron */
			argv[i] = NULL;
		}
	}

	if ((e = MEROgetStatus(&orig, NULL)) != NULL) {
		fprintf(stderr, "%s: internal error: %s\n", type, e);
		free(e);
		exit(2);
	}
	if (doall != 1) {
		stats = globMatchDBS(argc, argv, &orig, type);
		msab_freeStatus(&orig);
		orig = stats;
	}

	argv = alloca(sizeof(char *) * 64);
	i = 0;
	argv[i++] = type;

	stats = orig;
	prev = NULL;
	while (stats != NULL) {
		/* When -a was given, we're supposed to start all known
		 * databases.  In this mode we should omit starting already
		 * started databases, so we need to check first. */

		if (doall == 1 && (
				((mode == STOP || mode == KILL) && stats->state != SABdbRunning)
				|| (mode == START && stats->state == SABdbRunning)))
		{
			/* needs not to be started/stopped, remove from list */
			if (prev == NULL) {
				orig = stats->next;
			} else {
				prev->next = stats->next;
			}
			stats->next = NULL;
			msab_freeStatus(&stats);
			if (prev == NULL) {
				stats = orig;
				continue;
			}
			stats = prev;
		}
		prev = stats;
		stats = stats->next;
	}

	if (orig != NULL) {
		simple_argv_cmd(argv[0], orig, type, NULL, action);
		msab_freeStatus(&orig);
	}

	return;
}

typedef enum {
	SET = 0,
	INHERIT
} meroset;

static void
command_set(int argc, char *argv[], meroset type)
{
	char *p = NULL;
	char property[24] = "";
	int i;
	int state = 0;
	char *res;
	char *out;
	sabdb *orig = NULL;
	sabdb *stats = NULL;
	char *e;

	if (argc >= 1 && argc <= 2) {
		/* print help message for this command */
		command_help(2, &argv[-1]);
		exit(1);
	} else if (argc == 0) {
		exit(2);
	}

	/* time to collect some option flags */
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			for (p = argv[i] + 1; *p != '\0'; p++) {
				switch (*p) {
					case '-':
						if (p[1] == '\0') {
							i = argc;
							break;
						}
					default:
						fprintf(stderr, "%s: unknown option: -%c\n",
								argv[0], *p);
						command_help(2, &argv[-1]);
						exit(1);
					break;
				}
			}
			/* make this option no longer available, for easy use
			 * lateron */
			argv[i] = NULL;
		} else if (property[0] == '\0') {
			/* first non-option is property, rest is database */
			p = argv[i];
			if (type == SET) {
				if ((p = strchr(argv[i], '=')) == NULL) {
					fprintf(stderr, "set: need property=value\n");
					command_help(2, &argv[-1]);
					exit(1);
				}
				*p = '\0';
				snprintf(property, sizeof(property), "%s", argv[i]);
				*p++ = '=';
				p = argv[i];
			} else {
				snprintf(property, sizeof(property), "%s", argv[i]);
			}
			argv[i] = NULL;
		}
	}

	if (property[0] == '\0') {
		fprintf(stderr, "%s: need a property argument\n", argv[0]);
		command_help(2, &argv[-1]);
		exit(1);
	}

	if ((e = MEROgetStatus(&orig, NULL)) != NULL) {
		fprintf(stderr, "%s: internal error: %s\n", argv[0], e);
		free(e);
		exit(2);
	}
	stats = globMatchDBS(argc, argv, &orig, argv[0]);
	msab_freeStatus(&orig);
	orig = stats;

	/* handle rename separately due to single argument constraint */
	if (strcmp(property, "name") == 0) {
		if (type == INHERIT) {
			fprintf(stderr, "inherit: cannot default to a database name\n");
			exit(1);
		}

		if (orig->next != NULL) {
			fprintf(stderr, "%s: cannot rename multiple databases to "
					"the same name\n", argv[0]);
			exit(1);
		}

		out = control_send(&res, mero_host, mero_port,
				orig->dbname, p, 0, mero_pass);
		if (out != NULL || strcmp(res, "OK") != 0) {
			res = out == NULL ? res : out;
			fprintf(stderr, "%s: %s\n", argv[0], res);
			state |= 1;
		}
		free(res);

		msab_freeStatus(&orig);
		exit(state);
	}

	for (stats = orig; stats != NULL; stats = stats->next) {
		if (type == INHERIT) {
			strncat(property, "=", sizeof(property));
			p = property;
		}
		out = control_send(&res, mero_host, mero_port,
				stats->dbname, p, 0, mero_pass);
		if (out != NULL || strcmp(res, "OK") != 0) {
			res = out == NULL ? res : out;
			fprintf(stderr, "%s: %s\n", argv[0], res);
			state |= 1;
		}
		free(res);
	}

	msab_freeStatus(&orig);
	exit(state);
}

static void
command_get(int argc, char *argv[])
{
	char doall = 1;
	char *p;
	char *property = NULL;
	char *buf;
	char *e;
	int i;
	sabdb *orig, *stats;
	int twidth = TERMWIDTH;
	char *source, *value;
	confkeyval *kv;
	confkeyval *defprops = getDefaultProps();
	confkeyval *props = getDefaultProps();

	if (argc == 1) {
		/* print help message for this command */
		command_help(2, &argv[-1]);
		exit(1);
	} else if (argc == 0) {
		exit(2);
	}

	/* time to collect some option flags */
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			for (p = argv[i] + 1; *p != '\0'; p++) {
				switch (*p) {
					case '-':
						if (p[1] == '\0') {
							if (argc - 1 > i) 
								doall = 0;
							i = argc;
							break;
						}
					default:
						fprintf(stderr, "get: unknown option: -%c\n", *p);
						command_help(2, &argv[-1]);
						exit(1);
					break;
				}
			}
			/* make this option no longer available, for easy use
			 * lateron */
			argv[i] = NULL;
		} else if (property == NULL) {
			/* first non-option is property, rest is database */
			property = argv[i];
			argv[i] = NULL;
			if (strcmp(property, "all") == 0) {
				size_t off = 0;
				property = alloca(sizeof(char) * 512);
				kv = defprops;
				off += snprintf(property, 512, "name");
				while (kv->key != NULL) {
					off += snprintf(property + off, 512 - off, ",%s", kv->key);
					kv++;
				}
			}
		} else {
			doall = 0;
		}
	}

	if (property == NULL) {
		fprintf(stderr, "get: need a property argument\n");
		command_help(2, &argv[-1]);
		exit(1);
	}

	e = control_send(&buf, mero_host, mero_port,
			"#defaults", "get", 1, mero_pass);
	if (e != NULL) {
		fprintf(stderr, "get: internal error: %s\n", e);
		free(e);
		exit(2);
	} else if (strncmp(buf, "OK\n", 3) != 0) {
		fprintf(stderr, "get: %s\n", buf);
		free(buf);
		exit(1);
	}
	readPropsBuf(defprops, buf + 3);
	free(buf);

	if ((e = MEROgetStatus(&orig, NULL)) != NULL) {
		fprintf(stderr, "get: internal error: %s\n", e);
		free(e);
		exit(2);
	}

	/* look at the arguments and evaluate them based on a glob (hence we
	 * listed all databases before) */
	if (doall != 1) {
		stats = globMatchDBS(argc, argv, &orig, "get");
		msab_freeStatus(&orig);
		orig = stats;
	}

	/* suppress header when there are no results */
	if (orig == NULL) {
		free(props);
		return;
	}

	/* name = 15 */
	/* prop = 8 */
	/* source = 7 */
	twidth -= 15 + 2 + 8 + 2 + 7 + 2;
	if (twidth < 6)
		twidth = 6;
	value = alloca(sizeof(char) * twidth + 1);
	printf("     name          prop     source           value\n");
	while ((p = strtok(property, ",")) != NULL) {
		property = NULL;
		stats = orig;
		while (stats != NULL) {
			/* special virtual case */
			if (strcmp(p, "name") == 0) {
				source = "-";
				abbreviateString(value, stats->dbname, twidth);
			} else {
				e = control_send(&buf, mero_host, mero_port,
						stats->dbname, "get", 1, mero_pass);
				if (e != NULL) {
					fprintf(stderr, "get: internal error: %s\n", e);
					free(e);
					exit(2);
				} else if (strncmp(buf, "OK\n", 3) != 0) {
					fprintf(stderr, "get: %s\n", buf);
					free(buf);
					exit(1);
				}
				readPropsBuf(props, buf + 3);
				free(buf);
				kv = findConfKey(props, p);
				if (kv == NULL) {
					fprintf(stderr, "get: no such property: %s\n", p);
					stats = NULL;
					continue;
				}
				if (kv->val == NULL) {
					kv = findConfKey(defprops, p);
					source = "default";
					abbreviateString(value,
							kv != NULL && kv->val != NULL ? kv->val : "<unknown>", twidth);
				} else {
					source = "local";
					abbreviateString(value, kv->val, twidth);
				}
			}
			printf("%-15s  %-8s  %-7s  %s\n",
					stats->dbname, p, source, value);
			freeConfFile(props);
			stats = stats->next;
		}
	}

	msab_freeStatus(&orig);
	free(props);
}

static void
command_create(int argc, char *argv[])
{
	simple_command(argc, argv, "create", "created database in maintenance mode", 0);
}

static void
command_destroy(int argc, char *argv[])
{
	int i;
	int force = 0;    /* ask for confirmation */
	char *e;
	sabdb *orig = NULL;
	sabdb *stats = NULL;

	if (argc == 1) {
		/* print help message for this command */
		command_help(argc + 1, &argv[-1]);
		exit(1);
	}

	/* walk through the arguments and hunt for "options" */
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--") == 0) {
			argv[i] = NULL;
			break;
		}
		if (argv[i][0] == '-') {
			if (argv[i][1] == 'f') {
				force = 1;
				argv[i] = NULL;
			} else {
				fprintf(stderr, "destroy: unknown option: %s\n", argv[i]);
				command_help(argc + 1, &argv[-1]);
				exit(1);
			}
		}
	}

	if ((e = MEROgetStatus(&orig, NULL)) != NULL) {
		fprintf(stderr, "destroy: internal error: %s\n", e);
		free(e);
		exit(2);
	}
	stats = globMatchDBS(argc, argv, &orig, "destroy");
	msab_freeStatus(&orig);
	orig = stats;

	if (orig == NULL)
		exit(1);

	if (force == 0) {
		char answ;
		printf("you are about to remove database%s ", orig->next != NULL ? "s" : "");
		for (stats = orig; stats != NULL; stats = stats->next)
			printf("%s'%s'", stats != orig ? ", " : "", stats->dbname);
		printf("\nALL data in %s will be lost, are you sure? [y/N] ",
				orig->next != NULL ? "these databases" : "this database");
		if (scanf("%c", &answ) >= 1 &&
				(answ == 'y' || answ == 'Y'))
		{
			/* do it! */
		} else {
			printf("aborted\n");
			exit(1);
		}
	}

	simple_argv_cmd(argv[0], orig, "destroy", "destroyed database", NULL);
	msab_freeStatus(&orig);
}

static void
command_lock(int argc, char *argv[])
{
	simple_command(argc, argv, "lock", "put database under maintenance", 1);
}

static void
command_release(int argc, char *argv[])
{
	simple_command(argc, argv, "release", "taken database out of maintenance mode", 1);
}


int
main(int argc, char *argv[])
{
	char buf[1024];
	int i;
#ifdef TIOCGWINSZ
	struct winsize ws;

	if (ioctl(fileno(stdin), TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
		TERMWIDTH = ws.ws_col;
#endif

	/* Start handling the arguments.
	 * monetdb [monetdb_options] command [options] [database [...]]
	 * this means we first scout for monetdb_options which stops as soon
	 * as we find a non-option argument, which then must be command */
	
	/* first handle the simple no argument case */
	if (argc <= 1) {
		command_help(0, NULL);
		return(1);
	}
	
	/* handle monetdb_options */
	for (i = 1; argc > i && argv[i][0] == '-'; i++) {
		switch (argv[i][1]) {
			case 'v':
				command_version();
			return(0);
			case 'q':
				monetdb_quiet = 1;
			break;
			case 'h':
				if (strlen(&argv[i][2]) > 0) {
					mero_host = &argv[i][2];
				} else {
					if (i + 1 < argc) {
						mero_host = argv[++i];
					} else {
						fprintf(stderr, "monetdb: -h needs an argument\n");
						return(1);
					}
				}
			break;
			case 'p':
				if (strlen(&argv[i][2]) > 0) {
					mero_port = atoi(&argv[i][2]);
				} else {
					if (i + 1 < argc) {
						mero_port = atoi(argv[++i]);
					} else {
						fprintf(stderr, "monetdb: -p needs an argument\n");
						return(1);
					}
				}
			break;
			case 'P':
				/* take care we remove the password from argv so it
				 * doesn't show up in e.g. ps -ef output */
				if (strlen(&argv[i][2]) > 0) {
					mero_pass = strdup(&argv[i][2]);
					memset(&argv[i][2], 0, strlen(mero_pass));
				} else {
					if (i + 1 < argc) {
						mero_pass = strdup(argv[++i]);
						memset(argv[i], 0, strlen(mero_pass));
					} else {
						fprintf(stderr, "monetdb: -P needs an argument\n");
						return(1);
					}
				}
			break;
			case '-':
				/* skip -- */
				if (argv[i][2] == '\0')
					break;
				if (strcmp(&argv[i][2], "version") == 0) {
					command_version();
					return(0);
				} else if (strcmp(&argv[i][2], "help") == 0) {
					command_help(0, NULL);
					return(0);
				}
			default:
				fprintf(stderr, "monetdb: unknown option: %s\n", argv[i]);
				command_help(0, NULL);
				return(1);
			break;
		}
	}

	/* check consistency of -h -p and -P args */
	if (mero_pass != NULL && (mero_host == NULL || *mero_host == '/')) {
		fprintf(stderr, "monetdb: -P requires -h to be used with a TCP hostname\n");
		exit(1);
	} else if (mero_host != NULL && *mero_host != '/' && mero_pass == NULL) {
		fprintf(stderr, "monetdb: -h requires -P to be used\n");
		exit(1);
	}

	/* see if we still have arguments at this stage */
	if (i >= argc) {
		command_help(0, NULL);
		return(1);
	}
	
	/* commands that do not need merovingian to be running */
	if (strcmp(argv[i], "help") == 0) {
		command_help(argc - i, &argv[i]);
		return(0);
	} else if (strcmp(argv[i], "version") == 0) {
		command_version();
		return(0);
	}

	/* use UNIX socket if no hostname given */
	if (mero_host == NULL || *mero_host == '/') {
		/* a socket looks like /tmp/.s.merovingian.<tcpport>, try
		 * finding such port.  If mero_host is set, it is the location
		 * where we should search, which defaults to '/tmp' */
		if (mero_host == NULL)
			mero_host = "/tmp";
		do {
			/* first try the port given (or else its default) */
			snprintf(buf, sizeof(buf), "%s/.s.merovingian.%d",
					mero_host, mero_port == -1 ? 50001 : mero_port);
			if (control_ping(buf, -1, NULL) == 0) {
				mero_host = buf;
				break;
			}

			/* if port wasn't given, we can try and search for available
			 * sockets */
			if (mero_port == -1) {
				DIR *d;
				struct dirent *e;
				struct stat s;

				d = opendir(mero_host);
				if (d == NULL) {
					fprintf(stderr, "monetdb: cannot find a control socket, use -h and/or -p\n");
					exit(1);
				}
				while ((e = readdir(d)) != NULL) {
					if (strncmp(e->d_name, ".s.merovingian.", 15) != 0)
						continue;
					snprintf(buf, sizeof(buf), "%s/%s", mero_host, e->d_name);
					if (stat(buf, &s) == -1)
						continue;
					if (S_ISSOCK(s.st_mode)) {
						if (control_ping(buf, -1, NULL) == 0) {
							mero_host = buf;
							break;
						}
					}
				}
				closedir(d);
			}
		} while(0);

		if (mero_host != buf) {
			fprintf(stderr, "monetdb: cannot find a control socket, use -h and/or -p\n");
			exit(1);
		}
		/* don't confuse control_send lateron */
		mero_port = -1;
	}
	/* for TCP connections */
	if (mero_host != NULL && *mero_host != '/' && mero_port == -1)
		mero_port = 50001;

	/* handle regular commands */
	if (strcmp(argv[i], "create") == 0) {
		command_create(argc - i, &argv[i]);
	} else if (strcmp(argv[i], "destroy") == 0) {
		command_destroy(argc - i, &argv[i]);
	} else if (strcmp(argv[i], "lock") == 0) {
		command_lock(argc - i, &argv[i]);
	} else if (strcmp(argv[i], "release") == 0) {
		command_release(argc - i, &argv[i]);
	} else if (strcmp(argv[i], "status") == 0) {
		command_status(argc - i, &argv[i]);
	} else if (strcmp(argv[i], "start") == 0) {
		command_startstop(argc - i, &argv[i], START);
	} else if (strcmp(argv[i], "stop") == 0) {
		command_startstop(argc - i, &argv[i], STOP);
	} else if (strcmp(argv[i], "kill") == 0) {
		command_startstop(argc - i, &argv[i], KILL);
	} else if (strcmp(argv[i], "set") == 0) {
		command_set(argc - i, &argv[i], SET);
	} else if (strcmp(argv[i], "get") == 0) {
		command_get(argc - i, &argv[i]);
	} else if (strcmp(argv[i], "inherit") == 0) {
		command_set(argc - i, &argv[i], INHERIT);
	} else if (strcmp(argv[i], "discover") == 0) {
		command_discover(argc - i, &argv[i]);
	} else {
		fprintf(stderr, "monetdb: unknown command: %s\n", argv[i]);
		command_help(0, NULL);
	}

	if (mero_pass != NULL)
		free(mero_pass);

	return(0);
}

/* vim:set ts=4 sw=4 noexpandtab: */
