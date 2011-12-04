#ifndef JOS_KERN_MONITOR_H
#define JOS_KERN_MONITOR_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

struct Trapframe;

// Activate the kernel monitor,
// optionally providing a trap frame indicating the current state
// (NULL if none).
void monitor(struct Trapframe *tf);

// Functions implementing monitor commands.
int mon_help(int argc, char **argv, struct Trapframe *tf);
int mon_kerninfo(int argc, char **argv, struct Trapframe *tf);
int mon_backtrace(int argc, char **argv, struct Trapframe *tf);
int mon_alloc_page(int argc, char **argv, struct Trapframe *tf);
int mon_page_status(int argc, char **argv, struct Trapframe *tf);
int mon_free_page(int argc, char **argv, struct Trapframe *tf);
int mon_set_page_perms(int argc, char **argv, struct Trapframe *tf);
int mon_showmapping(int argc, char **argv, struct Trapframe *tf);
int mon_dumpva(int argc, char **argv, struct Trapframe *tf);
int mon_dumppa(int argc, char **argv, struct Trapframe *tf);
int mon_si(int argc, char **argv, struct Trapframe *tf);
int mon_brkpt(int argc, char **argv, struct Trapframe *tf);

// Utility function
void  print_fun_name(const char *fn_ptr, int lindex);

//User defined macro
#define VA_ADDR_MASK 0xFFFFF000

#endif	// !JOS_KERN_MONITOR_H
