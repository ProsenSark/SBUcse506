#include "ns.h"
#include <inc/pincl.h>

extern union Nsipc nsipcbuf;
static struct jif_pkt *pkt = (struct jif_pkt*) REQVA;

void
input(envid_t ns_envid)
{
	int r;

	binaryname = "ns_input";

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.
	while (1) {
		r = sys_page_alloc(0, pkt, PTE_U | PTE_W | PTE_P);
		if (r < 0) {
			panic("input: sys_page_alloc FAILED: %e", r);
		}

		r = -E_NO_DATA;
		while (r == -E_NO_DATA) {
			r = sys_net_rx_pkt(pkt->jp_data, ETH_PKTSZ_MAX);
			sys_yield();
		}
		if (r < 0) {
			panic("input: sys_net_rx_pkt FAILED: %e", r);
		}
		pkt->jp_len = r;

		ipc_send(ns_envid, NSREQ_INPUT, pkt, PTE_U | PTE_W | PTE_P);

		sys_page_unmap(0, pkt);
	}
}
