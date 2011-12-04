#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>


// Choose a user environment to run and run it.
void
sched_yield(void)
{
	// Implement simple round-robin scheduling.
	// Search through 'envs' for a runnable environment,
	// in circular fashion starting after the previously running env,
	// and switch to the first such environment found.
	// It's OK to choose the previously running env if no other env
	// is runnable.
	// But never choose envs[0], the idle environment,
	// unless NOTHING else is runnable.

	// LAB 4: Your code here.
#if LAB4A_SCHED_PRI
	int cur_index=0, next_index=0, priority_index=0,selected_index=0;
	int  priority_selected = 0 ;

	if(NULL != curenv){
		cur_index = ENVX(curenv->env_id);
	}

	next_index = (cur_index + 1)%NENV;

	for(;next_index != cur_index; next_index=(next_index+1)%NENV){
		if((envs[next_index].env_status == ENV_RUNNABLE) && (next_index != 0)){

			if(next_index & 0x01){
				priority_index = next_index;
				priority_selected = 1;
			}else{
				if(selected_index == 0)
				{
					selected_index=next_index;

				}
			}
			//          env_run(&envs[next_index]);
		}
	}
	if(priority_selected){
		env_run(&envs[priority_index]);
	}else{
		env_run(&envs[selected_index]);
	}
#else
	struct Env *penv, *pstart;
	int i = 0;

	if ((curenv == NULL) || (curenv == &envs[NENV - 1])) {
		// Skip envs[0]
		pstart = &envs[1];
	}
	else {
		pstart = curenv + 1;
	}
	penv = pstart;
	while (1) {
		++i;
		if ((penv == pstart) && (i > 1)) {
			// We've come back to start full-circle, time to break out
			break;
		}

		if (penv->env_status == ENV_RUNNABLE) {
			env_run(penv);
			break;
		}

		if (penv == &envs[NENV - 1]) {
			// Skip envs[0]
			penv = &envs[1];
		}
		else {
			++penv;
		}
	}
#endif

	// Run the special idle environment when nothing else is runnable.
	if (envs[0].env_status == ENV_RUNNABLE)
		env_run(&envs[0]);
	else {
		cprintf("Destroyed all environments - nothing more to do!\n");
		while (1)
			monitor(NULL);
	}
}
