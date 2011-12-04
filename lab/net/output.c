#include "ns.h"

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	envid_t env;
	int perm;
	int r;

	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
	while (1) {
		r = ipc_recv(&env, &nsipcbuf.pkt, &perm);
		assert(r == NSREQ_OUTPUT);
		assert(env == ns_envid);
		assert((perm & PTE_W) != 0);

		r = -E_NO_BUFS;
		while (r == -E_NO_BUFS) {
			r = sys_net_tx_pkt(nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len);
			sys_yield();
		}
		if (r < 0) {
			panic("output: sys_net_tx_pkt FAILED: %e", r);
		}
	}
}
