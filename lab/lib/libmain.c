// Called from entry.S to get us going.
// entry.S already took care of defining envs, pages, vpd, and vpt.

#include <inc/lib.h>

extern void umain(int argc, char **argv);

volatile struct Env *env;
char *binaryname = "(PROGRAM NAME UNKNOWN)";

static int
envid2env(envid_t envid, volatile struct Env **env_store)
{
	volatile struct Env *e;

	assert(env_store != NULL);
	if (envid == 0) {
		*env_store = NULL;
		return -E_BAD_ENV;
	}

	e = &envs[ENVX(envid)];
	if (e->env_status == ENV_FREE || e->env_id != envid) {
		*env_store = NULL;
		return -E_BAD_ENV;
	}

	*env_store = e;
	return 0;
}

volatile struct Env *
getenv()
{
	envid_t my_eid;
	volatile struct Env *e = NULL;

	my_eid = sys_getenvid();
	assert(envid2env(my_eid, &e) == 0);
	assert(e != NULL);

	return e;
}

void
libmain(int argc, char **argv)
{
	// set env to point at our env structure in envs[].
	// LAB 3: Your code here.
	//envid_t my_eid;

	env = 0;
	//cprintf("libmain: my_eid = %d\n", my_eid);
	env = getenv();

	// save the name of the program so that panic() can use it
	if (argc > 0)
		binaryname = argv[0];

	// call user main routine
	umain(argc, argv);

	// exit gracefully
	exit();
}

