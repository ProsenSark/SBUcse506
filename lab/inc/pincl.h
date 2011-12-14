#ifndef JOS_INC_PINCL_H
#define JOS_INC_PINCL_H

#include <inc/stdio.h>

#define clog(fmt, ...) \
	cprintf("%s:%d: %s: " fmt "\n", __FILE__, __LINE__, \
			__func__, ##__VA_ARGS__)

#define dclog(dbg, fmt, ...) \
	if (dbg) clog(fmt, ##__VA_ARGS__)

// Profile utility macros
#define PROFILE_START() \
	unsigned __time_start, __time_end; \
 \
	if (profile) __time_start = sys_time_msec()

#define PROFILE_END() \
	do { \
		if (profile) { \
			__time_end = sys_time_msec(); \
			clog("PROFILE: time taken = %u msecs", \
					(__time_end - __time_start)); \
		} \
	} while (0)

#endif /* !JOS_INC_PINCL_H */
