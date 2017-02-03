/* Pi-hole: A black hole for Internet advertisements
*  (c) 2017 Pi-hole, LLC (https://pi-hole.net)
*  Network-wide ad blocking via your own hardware.
*
*  FTL Engine
*  Daemon routines
*
*  This file is copyright under the latest version of the EUPL.
*  Please see LICENSE file for your rights under this license. */

#include "FTL.h"

bool test_singularity(void)
{
	FILE *f;
	if((f = fopen(FTLfiles.pid, "r")) == NULL)
	{
		logg("WARNING: Unable to read PID from file.");
		logg("         Cannot test if another FTL process is running!");
		return true;
	}
	// Test if any process has the given PID
	// We use getpgid() since we are allowed to inspect the
	// process group ID even for processes that don't belong to us
	int pid;
	fscanf(f,"%d",&pid);
	if (getpgid(pid) >= 0) {
		// Other process is running
		printf("FATAL: Another FTL process is already running! Exiting...\n");
		logg("FATAL: Another FTL process is already running! Exiting...");
		return false;
	}
	// No other process found
	return true;
}

void go_daemon(void)
{
	pid_t process_id = 0;
	pid_t sid = 0;

	if(!test_singularity())
		exit(1);

	// Create child process
	process_id = fork();

	// Indication of fork() failure
	if (process_id < 0)
	{
		printf("fork failed!\n");
		// Return failure in exit status
		exit(1);
	}

	// PARENT PROCESS. Need to kill it.
	if (process_id > 0)
	{
		// return success in exit status
		exit(0);
	}

	//unmask the file mode
	umask(0);

	//set new session
	sid = setsid();
	if(sid < 0)
	{
		// Return failure
		exit(1);
	}
	savepid(sid);

	// Change the current working directory to root.
	chdir("/etc/pihole");
	// Close stdin, stdout and stderr
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
}

struct timeval t0, t1;

void timer_start(void)
{
	gettimeofday(&t0, 0);
}

float timer_elapsed_msec(void)
{
	gettimeofday(&t1, 0);
	return (t1.tv_sec - t0.tv_sec) * 1000.0f + (t1.tv_usec - t0.tv_usec) / 1000.0f;
}

void savepid(pid_t sid)
{
	FILE *f;
	if((f = fopen("/etc/pihole/FTL.pid", "w+")) == NULL)
	{
		logg("WARNING: Unable to write PID to file.");
		logg("         Continuing anyway...");
	}
	else
	{
		fprintf(f, "%i", (int)sid);
		fclose(f);
	}
	logg_int("PID of FTL process: ", (int)sid);
}