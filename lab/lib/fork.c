// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>
#include <inc/pincl.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at vpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if ( !(err & FEC_WR) ) {
		panic("pgfault: error code = %u, not %u, addr = %p", err, FEC_WR,
				addr);
	}
	if ( !(vpt[VPN(addr)] & PTE_COW) ) {
		panic("pgfault: not a COW page, addr = %p", addr);
	}

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.

	// LAB 4: Your code here.
	//clog("wp1: addr = %p", addr);
	addr = ROUNDDOWN(addr, PGSIZE);
	r = sys_page_alloc(0, PFTEMP, PTE_W | PTE_U | PTE_P);
	if (r < 0) {
		panic("pgfault: sys_page_alloc FAILED: %e", r);
	}
	memmove(PFTEMP, addr, PGSIZE);
	r = sys_page_map(0, PFTEMP, 0, addr, PTE_W | PTE_U | PTE_P);
	if (r < 0) {
		panic("pgfault: sys_page_map FAILED: %e", r);
	}
	r = sys_page_unmap(0, PFTEMP);
	if (r < 0) {
		panic("pgfault: sys_page_unmap FAILED: %e", r);
	}

	//panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
// 
static int
duppage(envid_t envid, unsigned pn)
{
	int r;
	void *addr = (void *) (pn * PGSIZE);
	pte_t pte = vpt[pn];

	// LAB 4: Your code here.
	//panic("duppage not implemented");
	if ((pte & (PTE_SHARE | PTE_W)) == (PTE_SHARE | PTE_W)) {
		r = sys_page_map(0, addr, envid, addr, pte & PTE_USER);
		if (r < 0) {
			panic("duppage: sys_page_map FAILED: %e", r);
		}
	}
	else if (pte & (PTE_COW | PTE_W)) {
		r = sys_page_map(0, addr, envid, addr, PTE_COW | PTE_U | PTE_P);
		if (r < 0) {
			panic("duppage: sys_page_map FAILED: %e", r);
		}
		r = sys_page_map(0, addr, 0, addr, PTE_COW | PTE_U | PTE_P);
		if (r < 0) {
			panic("duppage: sys_page_map FAILED: %e", r);
		}
	}
	else {
		//clog("wp1: %p, vpt[%u] = %x", addr, pn, pte);
		assert((pte & PTE_USER) == (PTE_U | PTE_P));
		r = sys_page_map(0, addr, envid, addr, PTE_U | PTE_P);
		if (r < 0) {
			panic("duppage: sys_page_map FAILED: %e", r);
		}
	}

	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use vpd, vpt, and duppage.
//   Remember to fix "env" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	//panic("fork not implemented");
	envid_t envid;
	void *addr;
	int r;
	pde_t pde;
	pte_t pte;

	set_pgfault_handler(pgfault);
	//clog("wp1: vpt = %p, vpd = %p", vpt, vpd);
	//clog("wp2: pgfault = %p", pgfault);

	envid = sys_exofork();
	if (envid < 0) {
		panic("fork: sys_exofork FAILED: %e", envid);
	}
	if (envid == 0) {
		// Child process, fix "env"
		env = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	for (addr = (void *) UTEXT; (uintptr_t) addr < UTOP; addr += PGSIZE) {
		// skip Exception stack
		if ((uintptr_t) addr == (UXSTACKTOP - PGSIZE)) {
			continue;
		}

		pde = vpd[VPD(addr)];
		if ( !(pde & PTE_P) ) {
			addr += PTSIZE - PGSIZE;
			continue;
		}
		pte = vpt[VPN(addr)];
		if ( (pte & (PTE_U | PTE_P)) != (PTE_U | PTE_P) ) {
			continue;
		}

		// (re)map Writeable pages as COW, both in child & parent
		// and copy Shared page mappings to child
		duppage(envid, VPN(addr));
	}

	r = sys_page_alloc(envid, (void *) (UXSTACKTOP - PGSIZE),
			PTE_W | PTE_U | PTE_P);
	if (r < 0) {
		panic("fork: sys_page_alloc FAILED: %e", r);
	}
	r = sys_env_set_pgfault_upcall(envid, env->env_pgfault_upcall);
	if (r < 0) {
		panic("fork: sys_env_set_pgfault_upcall FAILED: %e", r);
	}

	r = sys_env_set_status(envid, ENV_RUNNABLE);
	if (r < 0) {
		panic("fork: sys_env_set_status FAILED: %e", r);
	}

	return envid;
}

// Challenge!
int
sfork(void)
{
	//panic("sfork not implemented");
	envid_t envid;
	void *addr;
	int r;
	pde_t pde;
	pte_t pte;
	volatile struct Env *penv = NULL;

	env = NULL;
	penv = getenv();
	assert(penv != NULL);

	set_pgfault_handler(pgfault);

	envid = sys_exofork();
	if (envid < 0) {
		panic("sfork: sys_exofork FAILED: %e", envid);
	}
	if (envid == 0) {
		// Child process, no need to fix "env"
		return 0;
	}

	for (addr = (void *) UTEXT; (uintptr_t) addr < UTOP; addr += PGSIZE) {
		// skip Exception stack
		if ((uintptr_t) addr == (UXSTACKTOP - PGSIZE)) {
			continue;
		}
		// skip Normal stack
		if ((uintptr_t) addr == (USTACKTOP - PGSIZE)) {
			continue;
		}

		pde = vpd[VPD(addr)];
		if ( !(pde & PTE_P) ) {
			addr += PTSIZE - PGSIZE;
			continue;
		}
		pte = vpt[VPN(addr)];
		if ( (pte & (PTE_U | PTE_P)) != (PTE_U | PTE_P) ) {
			continue;
		}

		// (re)map W pages onto child, shared-write by parent also
		if (pte & PTE_W) {
			r = sys_page_map(0, addr, envid, addr, PTE_W | PTE_U | PTE_P);
			if (r < 0) {
				panic("sfork: sys_page_map FAILED: %e", r);
			}
		}
	}
	// (re)map Normal stack as COW, both in child & parent
	duppage(envid, VPN(USTACKTOP - PGSIZE));

	r = sys_page_alloc(envid, (void *) (UXSTACKTOP - PGSIZE),
			PTE_W | PTE_U | PTE_P);
	if (r < 0) {
		panic("sfork: sys_page_alloc FAILED: %e", r);
	}
	r = sys_env_set_pgfault_upcall(envid, penv->env_pgfault_upcall);
	if (r < 0) {
		panic("sfork: sys_env_set_pgfault_upcall FAILED: %e", r);
	}

	r = sys_env_set_status(envid, ENV_RUNNABLE);
	if (r < 0) {
		panic("sfork: sys_env_set_status FAILED: %e", r);
	}

	//return -E_INVAL;
	return envid;
}
