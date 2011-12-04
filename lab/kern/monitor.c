// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>
#include <inc/error.h>
#include <inc/pincl.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>
#include <kern/pmap.h>
#include <kern/env.h>
#include <kern/syscall.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display the backtrace of function call stack", mon_backtrace},
	{ "alloc_page", "Allocate a page of physical memory", mon_alloc_page},
	{ "free_page", "Free a page of physical memory", mon_free_page},
	{ "page_status", "Find current status of a page of physical memory", mon_page_status},
	{ "set_page_perms", "(Re)set permissions on a page of virtual memory", mon_set_page_perms},
	{ "sm", "Show page mapping using virtual addresses", mon_showmapping},
	{ "dumpva", "Show virtual memory content", mon_dumpva},
	{ "dumppa", "Show physical memory content", mon_dumppa},
	{ "si", "Single-step next instruction", mon_si},
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

unsigned read_eip();
/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start %08x (virt)  %08x (phys)\n", _start, _start - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		(end-_start+1023)/1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
        struct Eipdebuginfo  ipdebuginfo;
	uint32_t             ebp_val = read_ebp();
	uint32_t*            ebp_ptr = (uint32_t *)ebp_val;
	uint32_t             eip = read_eip();
	int count = 0;

	cprintf("Stack backtrace:\n");	
	while(0x0!=ebp_val) {
	
		cprintf("  ebp %08x  eip %08x  args %08x %08x %08x %08x %08x\n", ebp_val, ebp_ptr[1], ebp_ptr[2], ebp_ptr[3], ebp_ptr[4], ebp_ptr[5],ebp_ptr[6]);

                debuginfo_eip(eip, &ipdebuginfo);
		
//              debuginfo_eip(ebp_ptr[1], &ipdebuginfo);
		cprintf("\t%s:%u: ", ipdebuginfo.eip_file, ipdebuginfo.eip_line);
                print_fun_name(ipdebuginfo.eip_fn_name,ipdebuginfo.eip_fn_namelen);
                cprintf("+%u\n",(eip - ipdebuginfo.eip_fn_addr));
		eip=ebp_ptr[1];
		ebp_val = ebp_ptr[0];
		if ((ebp_val == 0x0) && (tf != NULL) && (count <= 0)) {
			ebp_val = tf->tf_regs.reg_ebp;
			++count;
			//clog("wp1: %p", ebp_val);
		}
		ebp_ptr = (uint32_t *)ebp_val;
	}
	return 0;
}



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL && (tf->tf_trapno != T_DEBUG))
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}

// return EIP of caller.
// does not work if inlined.
// putting at the end of the file seems to prevent inlining.
unsigned
read_eip()
{
	uint32_t callerpc;
	__asm __volatile("movl 4(%%ebp), %0" : "=r" (callerpc));
	return callerpc;
}

//Print the pruned function name
void  
print_fun_name(const char *fn_ptr, int lindex)
{
    int i;
    for(i=0; i<lindex; i++)
    {
	putclrch((int)*(fn_ptr+i),0x0,0xd);
    }
 
}

int 
mon_alloc_page(int argc, char **argv, struct Trapframe *tf)
{
	struct Page *allocated_page;
	if(!page_alloc(&allocated_page)){
		allocated_page->pp_ref++;
		cprintf("0x%08lx\n",page2pa(allocated_page));
		return R_SUCCESS;
	}else{
		cprintf("[ERROR]:Allocation failed\n");
		return R_ERROR;
	}
}

int 
mon_page_status(int argc, char **argv, struct Trapframe *tf)
{
	if(2 == argc){
		physaddr_t page_addr = (physaddr_t) strtol(argv[1], NULL, 0);
		struct Page *page_status = pa2page(page_addr);

		if(page_status->pp_ref >0){
			cprintf("\t Allocated\n");
		}else if (page_status->pp_ref == 0){
			cprintf("\t free\n");
		}else{
			cprintf("\t Unknown status\n");
		}

		return R_SUCCESS;
	}else{
		cprintf("Command/> page_status 0x0f234 \n");
		return R_ERROR;
	}
}

int 
mon_free_page(int argc, char **argv, struct Trapframe *tf)
{
	if(2 == argc){
		physaddr_t page_addr = (physaddr_t) strtol(argv[1], NULL, 0);
		struct Page *page_status = pa2page(page_addr);

		if(page_status != NULL){
			page_decref(page_status);
		}else{
			cprintf("Page doesn't exist\n");
			return R_ERROR;
		}

		return R_SUCCESS;
	}else{
		cprintf("Command/> page_status 0x0f234 \n");
		return R_ERROR;
	}

}

int
mon_set_page_perms(int argc, char **argv, struct Trapframe *tf)
{
	if (argc == 3) {
		extern pde_t* boot_pgdir;
		uintptr_t virtual_addr = (uintptr_t) strtol(argv[1], NULL, 0);
		int perms = (int) strtol(argv[2], NULL, 0);
		struct Page *pp;

		pp = page_lookup(boot_pgdir, (void *) virtual_addr, NULL);
		if (pp == NULL) {
			cprintf("Page mapping doesn't exist\n");
			return R_ERROR;
		}

		page_insert(boot_pgdir, pp, (void *) virtual_addr, perms);

		return R_SUCCESS;
	}
	else {
		cprintf("Command/> set_page_perms 0x0f234 <permission flags>\n");
		cprintf("\t flags must be a bitwise-ORed value of the following:\n");
		cprintf("\t\t WRITEABLE = 0x002, USER = 0x004\n");
		return R_ERROR;
	}
}

int
mon_showmapping(int argc, char **argv, struct Trapframe *tf)
{
	if(3 == argc){
		uintptr_t virtual_addr = 0;
		uintptr_t page_range_start = (uintptr_t) strtol(argv[1],NULL,0);
		uintptr_t page_range_end = (uintptr_t) strtol(argv[2],NULL,0);
		extern pde_t* boot_pgdir; 
		pde_t 	*pgdir_entry;
		pte_t	*pgtable_entry;
		pte_t	*pgtable;
		int		atleast_one_mapping_present = 0;

		for(virtual_addr=page_range_start;
				virtual_addr<=page_range_end;
				virtual_addr+=PGSIZE){

			pgdir_entry = &boot_pgdir[PDX(virtual_addr)];
			if(!(*pgdir_entry & PTE_P)){
				continue ;
			}

			pgtable = (pte_t*) KADDR(PTE_ADDR(*pgdir_entry));
			pgtable_entry = &pgtable[PTX(virtual_addr)];

			if(!(*pgtable_entry & PTE_P)){
				continue;
			}

			//if available then only show it other wise show smoething else
			atleast_one_mapping_present = 1;

			cprintf("VA( %08p ) PA( %08p ) ", (virtual_addr), PTE_ADDR(*pgtable_entry));


			if(*pgtable_entry & PTE_AVAIL){
				cprintf("|AVAIL|");
			}else{
				cprintf("|NOT AVAIL|");
			}
			if(*pgtable_entry & PTE_D){
				cprintf("|DIRTY|");
			}else{
				cprintf("|NOT DIRTY|");
			}
			if(*pgtable_entry & PTE_A){
				cprintf("|ACCESSED|");
			}else{
				cprintf("|NOT ACCESSED|");
			}
			if(*pgtable_entry & PTE_U){
				cprintf("|USER|");
			}else{
				cprintf("|SUPER|");
			}
			if(*pgtable_entry & PTE_W){
				cprintf("|WRITEABLE|");
			}else{
				cprintf("|READABLE|");
			}
			if(*pgtable_entry & PTE_P){
				cprintf("|PRESENT|\n");
			}else{
				cprintf("|NOT PRESENT|\n");
			}

		}//end of for loop

		if (!atleast_one_mapping_present) {
			cprintf("No mapping present\n");
		}

		return R_SUCCESS;
	}else{
		cprintf("Command/> sm 0x12345678 0x87654321 \n");
		return R_ERROR;
	}
}

int 
mon_dumpva(int argc, char **argv, struct Trapframe *tf)
{

	if(argc == 3){
		extern pde_t* boot_pgdir;
		uintptr_t virtual_addr=0;
		uintptr_t start_va_addr = (uintptr_t) strtol(argv[1],NULL,0);
		uintptr_t end_va_addr = (uintptr_t) strtol(argv[2],NULL,0);
		virtual_addr = /*start_va_addr >= KERNBASE ? KERNBASE :*/ start_va_addr;
		for(;virtual_addr <= MAXVAL(uintptr_t) && virtual_addr <= end_va_addr; virtual_addr +=4){
			if (page_lookup(boot_pgdir, (void *) virtual_addr, NULL) == NULL) continue;
			cprintf(" va: %08p value: 0x%08lx \n", virtual_addr, *((uint32_t*)virtual_addr));
		} 
		return R_SUCCESS;

	}else{
		cprintf("Command/> dumpva 0x12345678 0x87654321\n");
		return R_ERROR;
	}
	return R_SUCCESS;
}

int 
mon_dumppa(int argc, char **argv, struct Trapframe *tf)
{
	extern size_t npage;

	if(argc == 3){
		uintptr_t ph_addr=0;
		uintptr_t start_ph_addr = (uintptr_t) strtol(argv[1],NULL,0);
		uintptr_t end_ph_addr = (uintptr_t) strtol(argv[2],NULL,0);

		for(ph_addr=start_ph_addr; ph_addr<= end_ph_addr && ph_addr <= npage*PGSIZE; ph_addr +=4){
			cprintf(" pa: %08p kva: %08p value: 0x%08lx \n", ph_addr, KADDR(ph_addr), *((uint32_t*)KADDR(ph_addr)));
		}
		return R_SUCCESS;

	}else{
		cprintf("Command/> dumppa 0x12345678 0x87654321\n");
		return R_ERROR;
	}
	return R_SUCCESS;
}

int
mon_si(int argc, char **argv, struct Trapframe *tf)
{
	if (!tf) {
		cprintf("ERROR: No breakpoint encountered yet!\n");
		return R_ERROR;
	}

	if ((tf->tf_trapno != T_BRKPT) && (tf->tf_trapno != T_DEBUG)) {
		cprintf("ERROR: Not in any breakpoint context currently!\n");
		return R_ERROR;
	}

#if !(NO_FAST_SYSCALL)
	extern void sysenter_handler();
	if (tf->tf_eip == (uintptr_t) sysenter_handler) {
		syscall(SYS_env_destroy, curenv->env_id, 0, 0, 0, 0);
	}
#endif

	//Setting the TF bit in the EFLAGS
	cprintf("Trap # %u, EIP -> 0x%08x\n", tf->tf_trapno, tf->tf_eip);

	tf->tf_eflags = tf->tf_eflags | FL_TF;

	env_run(curenv);

	return R_SUCCESS;
}

