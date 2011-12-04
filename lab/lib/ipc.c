// User-level IPC library routines

#include <inc/lib.h>
#include <inc/pincl.h>

// Receive a value via IPC and return it.
// If 'pg' is nonnull, then any page sent by the sender will be mapped at
//	that address.
// If 'from_env_store' is nonnull, then store the IPC sender's envid in
//	*from_env_store.
// If 'perm_store' is nonnull, then store the IPC sender's page permission
//	in *perm_store (this is nonzero iff a page was successfully
//	transferred to 'pg').
// If the system call fails, then store 0 in *fromenv and *perm (if
//	they're nonnull) and return the error.
// Otherwise, return the value sent by the sender
//
// Hint:
//   Use 'env' to discover the value and who sent it.
//   If 'pg' is null, pass sys_ipc_recv a value that it will understand
//   as meaning "no page".  (Zero is not the right value, since that's
//   a perfectly valid place to map a page.)
int32_t
ipc_recv(envid_t *from_env_store, void *pg, int *perm_store)
{
	// LAB 4: Your code here.
	//panic("ipc_recv not implemented");
	int rc;

	if (from_env_store) {
		*from_env_store = 0;
	}
	if (pg == NULL) {
		pg = (void *) UTOP;
	}
	if (perm_store) {
		*perm_store = 0;
	}

	rc = sys_ipc_recv(pg);
	if (rc < 0) {
		return rc;
	}

	assert(env != NULL);
	if (from_env_store) {
		*from_env_store = env->env_ipc_from;
	}
	if (perm_store) {
		*perm_store = env->env_ipc_perm;
	}
	return (int32_t) env->env_ipc_value;
	//return 0;
}

// Send 'val' (and 'pg' with 'perm', if 'pg' is nonnull) to 'toenv'.
// This function keeps trying until it succeeds.
// It should panic() on any error other than -E_IPC_NOT_RECV.
//
// Hint:
//   Use sys_yield() to be CPU-friendly.
//   If 'pg' is null, pass sys_ipc_recv a value that it will understand
//   as meaning "no page".  (Zero is not the right value.)
void
ipc_send(envid_t to_env, uint32_t val, void *pg, int perm)
{
	// LAB 4: Your code here.
	//panic("ipc_send not implemented");
	int rc = -E_IPC_NOT_RECV;

	if (pg == NULL) {
		pg = (void *) UTOP;
	}

	while (rc == -E_IPC_NOT_RECV) {
		rc = sys_ipc_try_send(to_env, val, pg, perm);
		sys_yield();
	}
	if (rc < 0) {
		panic("ipc_send: sys_ipc_try_send FAILED: %e", rc);
	}
}
