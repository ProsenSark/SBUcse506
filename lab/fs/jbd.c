#include "jbd.h"

#include <inc/assert.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/lib.h>
#include <inc/pincl.h>

#define debug 0

struct Journal *jsuper;
//uint32_t *atbmap;

bool trans_replay_ip;

static const char * const oper_str[JBD_MAXOP_P1] =
{
	NULL,
	"JBD_CREAT",
	"JBD_WRITE",
	"JBD_TRUNC",
	"JBD_DELET",
};

// Find the disk block number slot for the 'handleno'th handle in
// transaction 't'. Set '*ppdiskbno' to point to that slot.
// When 'alloc' is set, this function will allocate a handle indirect block
// if necessary.
//
// Returns:
//	0 on success (but note that *ppdiskbno might equal 0).
//	-E_NOT_FOUND if the function needed to allocate a handle block, but
//		alloc was 0.
//	-E_NO_DISK if there's no space on the disk for a handle block.
//	-E_INVAL if handleno is out of range (it's >= MAX_NHANDLES_INTRANS).
//
// Analogy: This is like pgdir_walk for transactions.
// Hint: Don't forget to clear any block you allocate.
static int
transaction_handle_walk(Transaction_t *t, uint32_t handleno,
		uint32_t **ppdiskbno, bool alloc)
{
	int rc;
	uint32_t *va;

	if (handleno >= MAX_NHANDLES_INTRANS) {
		return -E_INVAL;
	}

	assert(t != NULL);
	if (t->t_handlei == 0) {
		if (!alloc) {
			return -E_NOT_FOUND;
		}
		//rc = alloc_block(0);
		rc = alloc_block(1);
		if (rc < 0) {
			return rc;
		}
		memset(diskaddr(rc), 0, BLKSIZE);
		t->t_handlei = rc;
		//flush_sector(t);
		//occupy_block(rc);
	}
	else {
		assert(!block_is_free(t->t_handlei));
	}
	va = diskaddr(t->t_handlei);
	*ppdiskbno = &(va[handleno]);

	return 0;
}

// Set *pph to point at the handleno'th handle in transaction 't'.
// Allocate the handle block if it doesn't yet exist.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_NO_DISK if a block needed to be allocated but the disk is full.
//	-E_INVAL if handleno is out of range.
//
// Hint: Use transaction_handle_walk and alloc_block.
int
transaction_get_handle(Transaction_t *t, uint32_t handleno,
		Handle_t **pph, bool alloc)
{
	int rc;
	uint32_t *pdiskbno;

	if (handleno >= MAX_NHANDLES_INTRANS) {
		return -E_INVAL;
	}

	rc = transaction_handle_walk(t, handleno, &pdiskbno, alloc);
	if (rc < 0) {
		return rc;
	}

	if (*pdiskbno == 0) {
		if (!alloc) {
			return -E_NOT_FOUND;
		}
		rc = alloc_block(1);
		if (rc < 0) {
			return rc;
		}
		memset(diskaddr(rc), 0, BLKSIZE);
		*pdiskbno = rc;

		// Write out handle indirect to disk
		//flush_block(diskaddr(t->t_handlei));
	}
	else {
		assert(!block_is_free(*pdiskbno));
	}
	*pph = diskaddr(*pdiskbno);

	return 0;
}

static int
transaction_free_handle(Transaction_t *t, uint32_t handleno)
{
	int rc;
	uint32_t *pdiskbno;

	rc = transaction_handle_walk(t, handleno, &pdiskbno, 0);
	if (rc < 0) {
		return rc;
	}
	if (*pdiskbno) {
		free_block(*pdiskbno);
		*pdiskbno = 0;
	}

	return 0;
}

void
start_transaction(envid_t envid)
{
	Transaction_t *t;
	int i, first, next;

	if (trans_replay_ip) {
		return;
	}

	//t = env->env_trans;
	t = envs[ENVX(envid)].env_trans;
	//assert(t == NULL);
	if (t != NULL) {
		return;
	}

	dclog(debug, "envid = %x", envid);
	// FIXME: need to persist first & last in 1st sector of journal
	// superblock, and read from it
	first = -1;
	next = -1;

	for (i = 0; i < MAX_NTRANS; ++i) {
		//if (jsuper->j_trans[i].t_active > 0) {
		if (jsuper->j_trans[i].t_envid > 0) {
			if (first < 0) {
				first = i;
			}
			next = i;
		}
	}
	++next;
	if (next >= MAX_NTRANS) {
		panic("start_transaction: failed to allocate journal Transaction id");
	}

	// set transaction reference in target Env
	sys_env_set_transaction(envid, &jsuper->j_trans[next]);
	t = envs[ENVX(envid)].env_trans;

	// activate Transaction in block-cache only
	//t->t_active = 1;
	t->t_envid = envid;

	assert(t->t_nhandles == 0);
	assert(t->t_handlei == 0);
}

void
add_handle_to_transaction(envid_t envid, Handle_t *h)
{
	uint32_t hno, count;
	Handle_t *pdh;
	Transaction_t *t;
	off_t off1, off2;
	int r;

	if (trans_replay_ip) {
		return;
	}

	dclog(debug, "h_oper = %u", h->h_hdr.h_oper);
	t = envs[ENVX(envid)].env_trans;
	assert(t != NULL);

	// NOTE: kludge for caching multiple smaller writes to the same file
	// by the same Env
	if ( (h->h_hdr.h_oper == JBD_WRITE) && (t->t_nhandles > 0) ) {
		for (hno = t->t_nhandles - 1; hno >= 0; --hno) {
			pdh = NULL;
			r = transaction_get_handle(t, hno, &pdh, 0);
			if (r < 0) {
				clog("transaction_get_handle failed: %e", r);
				//continue;
				return;
			}
			assert(pdh != NULL);

			if ( (pdh->h_hdr.h_oper == JBD_WRITE) &&
					(strcmp(h->h_hdr.h_filename, pdh->h_hdr.h_filename) == 0) ) {
				off1 = pdh->h_params.write.p_off;
				off2 = h->h_params.write.p_off;
				assert(off2 >= off1);
				count = h->h_params.write.p_dsize;
				if ((off2 - off1 + count) <= sizeof(h->h_params.write.p_data)) {
					memmove(pdh->h_params.write.p_data + off2 - off1,
							h->h_params.write.p_data, count);
					pdh->h_params.write.p_dsize += count;
					return;
				}
				break;
			}
		}
	}

	// increment no. of handles
	hno = t->t_nhandles++;
	//flush_sector(t);

	pdh = NULL;
	r = transaction_get_handle(t, hno, &pdh, 1);
	if (r < 0) {
		panic("add_handle_to_transaction: transaction_get_handle failed: %e",
				r);
	}
	assert(pdh != NULL);

	// Copy from in-memory Handle_t to on-disk Handle_t
	memmove(pdh, h, sizeof(Handle_t));
	pdh->h_hdr.h_tid = t->t_id;

	// Write out handle to disk
	//flush_block(pdh);
	dclog(debug, "envid = %x, hno = %u, nh = %u", envid, hno,
			t->t_nhandles);
}

void
commit_transaction(envid_t envid, bool create_new)
{
	uint32_t hno;
	Handle_t *pdh;
	Transaction_t *t;
	int r;

	if (trans_replay_ip) {
		return;
	}

	t = envs[ENVX(envid)].env_trans;
	if ((t == NULL) && create_new) {
		start_transaction(envid);
		t = envs[ENVX(envid)].env_trans;
	}

	dclog(debug, "envid = %x", envid);
	assert(t != NULL);
	if (t->t_nhandles == 0) {
		assert(t->t_handlei == 0);
		return;
	}

	// 1st, flush handle blocks
	for (hno = 0; hno < t->t_nhandles; ++hno) {
		pdh = NULL;
		r = transaction_get_handle(t, hno, &pdh, 0);
		if (r < 0) {
			clog("transaction_get_handle failed: %e", r);
			//continue;
			return;
		}
		assert(pdh != NULL);

		flush_block(pdh);
	}

	// 2nd, flush handle indirect block
	flush_block(diskaddr(t->t_handlei));

	// 3rd, flush activate Transaction
	t->t_active = 1;
	flush_sector(t);
}

void
clear_transaction(Transaction_t *t)
{
	uint32_t hno;
	int r;

	assert(t != NULL);

	// 1st, de-activate Transaction
	t->t_active = 0;
	t->t_envid = 0;
	flush_sector(t);

	// 2nd, de-allocate handle blocks
	for (hno = 0; hno < t->t_nhandles; ++hno) {
		r = transaction_free_handle(t, hno);
		if (r < 0) {
			clog("transaction_free_handle failed: %e", r);
		}
	}

	// 3rd, de-allocate handle indirect block
	if (t->t_handlei) {
		free_block(t->t_handlei);
	}

	// 4th, some further changes to Transaction
	t->t_handlei = 0;
	t->t_nhandles = 0;
	flush_sector(t);
}

void
end_transaction(envid_t envid)
{
	Transaction_t *t;

	if (trans_replay_ip) {
		return;
	}

	t = envs[ENVX(envid)].env_trans;
	dclog(debug, "envid = %x", envid);
	assert(t != NULL);

	clear_transaction(t);

	// reset transaction reference in target Env
	sys_env_set_transaction(envid, NULL);
}

int
handle_replay(Handle_t *h, uint32_t handleno)
{
	struct File *f;
	int r;
	bool crash = 0;

	clog("h_tid = %u, hno = %u, h_oper = %s", h->h_hdr.h_tid, handleno,
			oper_str[h->h_hdr.h_oper]);

	f = NULL;
	if (h->h_hdr.h_oper == JBD_CREAT) {
		r = file_create(h->h_hdr.h_filename, &f, h->h_params.creat.p_filetype,
				crash);
		if (r < 0) {
			clog("file_create failed: %e", r);
			return r;
		}
		assert(f != NULL);

		file_flush(f, crash);
		return 0;
	}
	else {
		r = file_open(h->h_hdr.h_filename, &f);
		if (r < 0) {
			clog("file_open failed: %e", r);
			return r;
		}
		assert(f != NULL);
	}

	switch (h->h_hdr.h_oper) {
		case JBD_WRITE:
			//clog("wp1: %d, %s", h->h_params.write.p_dsize,
			//		h->h_params.write.p_data);
			r = file_write(f, h->h_params.write.p_data,
					h->h_params.write.p_dsize, h->h_params.write.p_off);
			if (r < 0) {
				clog("file_write failed: %e", r);
				return r;
			}
			file_flush(f, crash);
			break;
		case JBD_TRUNC:
			r = file_set_size(f, h->h_params.trunc.p_off);
			if (r < 0) {
				clog("file_set_size failed: %e", r);
				return r;
			}
			file_flush(f, crash);
			break;
		case JBD_DELET:
			// NOTE: This is the only case without FD/File
			r = file_remove(h->h_hdr.h_filename, crash);
			if (r < 0) {
				clog("file_remove failed: %e", r);
				return r;
			}
			break;

		default:
			clog("invalid JBD oper = %u", h->h_hdr.h_oper);
	}

	return 0;
}

void
transaction_replay(Transaction_t *t)
{
	uint32_t hno;
	Handle_t *pdh;
	int r;

	dclog(debug, "t_id = %u", t->t_id);

	assert(t != NULL);
	if (t->t_handlei == 0) {
		clog("invalid handle indirect block!");
		return;
	}

	for (hno = 0; hno < t->t_nhandles; ++hno) {
		pdh = NULL;
		r = transaction_get_handle(t, hno, &pdh, 0);
		if (r < 0) {
			clog("transaction_get_handle failed: %e", r);
			//continue;
			return;
		}
		assert(pdh != NULL);

		r = handle_replay(pdh, hno);
		if (r < 0) {
			//continue;
			return;
		}
	}
}

void
check_journal()
{
	if (jsuper->j_magic != JBD_MAGIC) {
		panic("bad journal magic number");
	}

	if (jsuper->j_nactive > MAX_NTRANS) {
		panic("inconsistent journal, j_nactive = %u", jsuper->j_nactive);
	}

	cprintf("journal superblock is good\n");
}

void
journal_init()
{
	int i;

	// perform journal audit
	trans_replay_ip = 1;
	for (i = 0; i < MAX_NTRANS; ++i) {
		if (jsuper->j_trans[i].t_active > 0) {
			transaction_replay(&jsuper->j_trans[i]);
		}
		clear_transaction(&jsuper->j_trans[i]);
	}
	trans_replay_ip = 0;
}

