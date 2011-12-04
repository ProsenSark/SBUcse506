#ifndef JOS_INC_PINCL_H
#define JOS_INC_PINCL_H

#include <inc/stdio.h>

#define clog(fmt, ...) \
	cprintf("%s:%d: %s: " fmt "\n", __FILE__, __LINE__, \
			__func__, ##__VA_ARGS__)

#endif /* !JOS_INC_PINCL_H */
