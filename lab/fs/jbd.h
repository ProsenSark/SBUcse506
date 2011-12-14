#ifndef JOS_FS_JBD_H
#define JOS_FS_JBD_H

#include "fs.h"
#include <inc/env.h>

#define JBD_MAGIC				0x4A424421U	// 'JBD!'

#define MAX_NTRANS				((BLKSIZE - SECTSIZE) / sizeof (Transaction_t))
#define MAX_NHANDLES_INTRANS	(BLKSIZE / 4)

#define HANDLE_HDRSIZE			sizeof(struct Handleheader)


enum {
	JBD_CREAT = 1,	// FSREQ_OPEN with O_CREAT
	JBD_WRITE,		// FSREQ_WRITE
	JBD_TRUNC,		// FSREQ_SET_SIZE
	JBD_DELET,		// FSREQ_REMOVE
	JBD_MAXOP_P1
};

struct Handleheader {
	uint32_t h_tid;
	uint32_t h_oper;
	char h_filename[MAXPATHLEN];
} __attribute__((packed));

union Handleparams {
	struct Hp_create {
		uint32_t p_filetype;
	} __attribute__((packed)) creat;
	struct Hp_write {
		off_t p_off;
		uint32_t p_dsize;
		char p_data[BLKSIZE - HANDLE_HDRSIZE - 8];
	} __attribute__((packed)) write;
	struct Hp_truncate {
		off_t p_off;
	} __attribute__((packed)) trunc;
	struct Hp_delete {
	} __attribute__((packed)) delet;
};

struct Handle {
	struct Handleheader h_hdr;
	union Handleparams h_params;
} __attribute__((packed));
typedef struct Handle Handle_t;

// NOTE: sizeof(struct Transaction) must be a power of 2
struct Transaction {
	uint32_t t_active;
	envid_t t_envid;
	uint32_t t_id;
	uint32_t t_nhandles;
	uint32_t t_handlei;
	uint32_t t_ref;

	uint8_t t_pad[32 - 24];
} __attribute__((packed));
typedef struct Transaction Transaction_t;

struct Journal {
	uint32_t j_magic;

	uint32_t j_nactive;
	uint32_t j_first;
	uint32_t j_last;

	uint8_t j_pad[SECTSIZE - 16];

	Transaction_t j_trans[MAX_NTRANS];

} __attribute__((packed));
typedef struct Journal Journal_t;

extern struct Journal *jsuper;		// journal superblock
//extern uint32_t *atbmap;			// active transaction bitmap block

// Whether transaction replay is in progress, typically set temporarily just
// after boot-up
extern bool trans_replay_ip;


void start_transaction(envid_t envid);
void add_handle_to_transaction(envid_t envid, Handle_t *h);
void commit_transaction(envid_t envid, bool create_new);
void end_transaction(envid_t envid);

void check_journal();
void journal_init();

#endif /* !JOS_FS_JBD_H */

