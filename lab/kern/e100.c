// LAB 6: Your driver code here
#include <inc/x86.h>
#include <inc/mmu.h>
#include <inc/string.h>
#include <inc/error.h>
#include <inc/pincl.h>

#include <kern/e100.h>
#include <kern/pci.h>
#include <kern/pmap.h>
#include <kern/picirq.h>

static uint32_t csr_base;
static uint32_t csr_size;
uint8_t e100_irq_line;

static struct E100_crb cbl[E100_CBL_SIZE];
static struct E100_crb rfa[E100_RFA_SIZE];
static int last_tx_id;
static int last_rx_id;

static uint8_t eeprom_size;
static uint8_t eeprom_hw_addr[HWADDR_LEN_82559ER];

static __inline void
udelay(int micros)
{
	int i;

	for (i = 0; i < micros; ++i) {
		inb(0x84);
	}
}

static void
init_dma_rings()
{
	int i;

	// Initialize CBL
	for (i = 0; i < E100_CBL_SIZE; ++i) {
		cbl[i].status = E100_TCB_SW_NULL;
		cbl[i].cmd = E100_TCB_CW_NULL;
		if (i == (E100_CBL_SIZE - 1)) {
			// Last in the ring
			cbl[i].link = PADDR(&cbl[0]);
		}
		else {
			cbl[i].link = PADDR(&cbl[i + 1]);
		}
		cbl[i].data.tx_data.tbd_addr = E100_TCB_TBD_NULL;
		cbl[i].data.tx_data.tbd_count = E100_TCB_TBD_CNT;
		cbl[i].data.tx_data.tx_thrs = E100_TCB_TX_THRESHOLD;
	}

	assert((ETH_PKTSZ_MAX % 2) == 0);

	// Initialize RFA
	for (i = 0; i < E100_RFA_SIZE; ++i) {
		rfa[i].status = E100_RFD_SW_NULL;
		if (i == (E100_RFA_SIZE - 1)) {
			// Last in the ring
			rfa[i].cmd = E100_RFD_CW_S;
			rfa[i].link = PADDR(&rfa[0]);
		}
		else {
			rfa[i].cmd = E100_RFD_CW_CMD;
			rfa[i].link = PADDR(&rfa[i + 1]);
		}
		rfa[i].data.rx_data.rfd_rsv = E100_RFD_RSV_NULL;
		rfa[i].data.rx_data.rfd_count = E100_RFD_AC_NULL;
		rfa[i].data.rx_data.rfd_size = ETH_PKTSZ_MAX;
	}
}

static int
restart_cu()
{
	uint16_t scb_cmd;
	uint16_t scb_status;

	scb_cmd = E100_SCB_CW_CU_DMASK | E100_SCB_CW_RU_DMASK;
	while ((scb_cmd & (E100_SCB_CW_CU_DMASK | E100_SCB_CW_RU_DMASK)) != 0x0) {
		//return -E_BUSY;
		scb_cmd = inw(E100_CSR_SCB_CW(csr_base));
		udelay(1);
	}

	scb_status = inw(E100_CSR_SCB_SW(csr_base));
	if ((scb_status & E100_SCB_SW_CU_DMASK) == E100_SCB_SW_CU_IDLE) {
		// Idle, so do CU start
		outl(E100_CSR_SCB_GP(csr_base), PADDR(&cbl[0]));
		outw(E100_CSR_SCB_CW(csr_base),
				(E100_SCB_CW_INTR | E100_SCB_CW_CU_START));
	}
	else if ((scb_status & E100_SCB_SW_CU_DMASK) == E100_SCB_SW_CU_SUSP) {
		// Suspended, so do CU resume
		outw(E100_CSR_SCB_CW(csr_base),
				(E100_SCB_CW_INTR | E100_SCB_CW_CU_RESUME));
	}
	else {
		//panic("scb_status = %u", scb_status);
	}

	return 0;
}

static int
restart_rx()
{
	uint16_t scb_cmd;
	uint16_t scb_status;

	scb_cmd = E100_SCB_CW_CU_DMASK | E100_SCB_CW_RU_DMASK;
	while ((scb_cmd & (E100_SCB_CW_CU_DMASK | E100_SCB_CW_RU_DMASK)) != 0x0) {
		//return -E_BUSY;
		scb_cmd = inw(E100_CSR_SCB_CW(csr_base));
		udelay(1);
	}

	scb_status = inw(E100_CSR_SCB_SW(csr_base));
	if ((scb_status & E100_SCB_SW_RU_DMASK) == E100_SCB_SW_RU_IDLE) {
		// Idle, so do RU start
		outl(E100_CSR_SCB_GP(csr_base), PADDR(&rfa[0]));
		outw(E100_CSR_SCB_CW(csr_base),
				(E100_SCB_CW_INTR | E100_SCB_CW_RU_START));
	}
	else if ((scb_status & E100_SCB_SW_RU_DMASK) == E100_SCB_SW_RU_SUSP) {
		// Suspended, so do RU resume
		outw(E100_CSR_SCB_CW(csr_base),
				(E100_SCB_CW_INTR | E100_SCB_CW_RU_RESUME));
	}
	else {
		//panic("scb_status = %u", scb_status);
	}

	return 0;
}

static void
hexdump(const char *prefix, const void *data, int len)
{
	int i;
	char buf[80];
	char *end = buf + sizeof(buf);
	char *out = NULL;
	int mlen = len < (end - buf) ? len : (end - buf);
	for (i = 0; i < mlen; i++) {
		if (i % 16 == 0)
			cprintf("%s%04x   ", prefix, i);
		cprintf("%02x", ((uint8_t*)data)[i]);
		if (i % 2 == 1)
			cprintf(" ");
		if (i % 16 == 15 || i == len - 1)
			cprintf("\n");
		if (i % 16 == 7)
			cprintf(" ");
	}
}

static void
print_dma_ring_rx()
{
	int i;

	clog("");
	for (i = 0; i < E100_RFA_SIZE; ++i) {
		cprintf("%d: %04x %04x %08x, %08x %04x %04x\n", i, rfa[i].status,
				rfa[i].cmd, rfa[i].link, rfa[i].data.rx_data.rfd_rsv,
				rfa[i].data.rx_data.rfd_count, rfa[i].data.rx_data.rfd_size);
	}
}

static void
eeprom_write_bit(uint8_t bval)
{
	int port;
	uint8_t b;

	assert((bval == 0) || (bval == 1));
	port = E100_CSR_EEPROM_CNTL(csr_base);
	b = bval ? E100_EC_EEDI_DMASK : 0;

	/* bit */
	outb(port, E100_EC_EECS_DMASK | b);
	/* clock HI */
	outb(port, E100_EC_EECS_DMASK | b | E100_EC_EESK_DMASK);
	udelay(3);
	/* clock LO */
	outb(port, E100_EC_EECS_DMASK | b);
	udelay(3);
}

static uint8_t
eeprom_read_bit()
{
	int port;
	uint8_t b, bval;

	port = E100_CSR_EEPROM_CNTL(csr_base);
	bval = 0;

	/* clock HI */
	outb(port, E100_EC_EECS_DMASK | E100_EC_EESK_DMASK);
	udelay(3);
	/* bit */
	b = inb(port);
	if (b & E100_EC_EEDO_DMASK) {
		bval = 1;
	}
	/* clock LO */
	outb(port, E100_EC_EECS_DMASK);
	udelay(3);

	assert((bval == 0) || (bval == 1));

	return bval;
}

static uint8_t
eeprom_get_size()
{
	int port;
	uint8_t size = 0;
	uint8_t v;
	int i;

	port = E100_CSR_EEPROM_CNTL(csr_base);

	// Activate EEPROM
	outb(port, E100_EC_EECS_DMASK);

	// Write READ opcode, one bit at a time
	v = E100_EC_OP_READ;
	for (i = (E100_EC_OP_LEN - 1); i >= 0; --i) {
		eeprom_write_bit((v >> i) & 0x1);
	}

	// Write address field, one bit at a time
	v = E100_EC_EEDO_DMASK;
	while ((v & E100_EC_EEDO_DMASK) != 0) {
		eeprom_write_bit(0);
		v = inb(port);
		++size;
		if (size == 0) {
			break;
		}
	}
	if (size == 0) {
		panic("eeprom_get_size: FAILED");
	}

	// Read a EEPROM register, one bit at a time
	//		Discard the value read though
	for (i = (E100_EEPROM_REG_WIDTH - 1); i >= 0; --i) {
		eeprom_read_bit();
	}

	// Deactivate EEPROM
	outb(port, 0);
	udelay(1);

	return size;
}

static uint16_t
eeprom_get_word(uint8_t address)
{
	int port;
	uint16_t val = 0;
	uint8_t v;
	int i;

	port = E100_CSR_EEPROM_CNTL(csr_base);

	// Activate EEPROM
	outb(port, E100_EC_EECS_DMASK);

	// Write READ opcode, one bit at a time
	v = E100_EC_OP_READ;
	for (i = (E100_EC_OP_LEN - 1); i >= 0; --i) {
		eeprom_write_bit((v >> i) & 0x1);
	}

	// Write address field, one bit at a time
	v = address;
	for (i = (eeprom_size - 1); i >= 0; --i) {
		eeprom_write_bit((v >> i) & 0x1);
	}

	// Read a EEPROM register, one bit at a time
	for (i = (E100_EEPROM_REG_WIDTH - 1); i >= 0; --i) {
		v = eeprom_read_bit();
		val |= v << i;
	}

	// Deactivate EEPROM
	outb(port, 0);
	udelay(1);

	return val;
}

static void
eeprom_get_hw_addr()
{
	uint16_t val;
	int i, f;

	assert(sizeof(eeprom_hw_addr[0]) == sizeof(uint8_t));
	f = sizeof(uint16_t) / sizeof(uint8_t);
	for (i = 0; i < (HWADDR_LEN_82559ER / f); ++i) {
		val = eeprom_get_word(i);
		// lower byte
		eeprom_hw_addr[(i * f) + 0] = val & 0xff;
		// upper byte
		eeprom_hw_addr[(i * f) + 1] = val >> 8;
	}
}

int
pci_network_8255x_attach(struct pci_func *pcif)
{
	pci_func_enable(pcif);
	//clog("CSR Mem Map: %p %x", pcif->reg_base[0], pcif->reg_size[0]);
	clog("CSR I/O Map: %p %x", pcif->reg_base[1], pcif->reg_size[1]);
	//clog("Flash Mem Map: %p %x", pcif->reg_base[2], pcif->reg_size[2]);
	clog("IRQ: %d", pcif->irq_line);
	csr_base = pcif->reg_base[1];
	csr_size = pcif->reg_size[1];
	e100_irq_line = pcif->irq_line;

	outl(E100_CSR_PORT(csr_base), E100_PORT_FN_SW_RESET);
	udelay(10);

	eeprom_size = eeprom_get_size();
	clog("EEPROM size = %u, i.e., %u registers", eeprom_size,
			(1u << eeprom_size));
	eeprom_get_hw_addr();
	clog("EEPROM HW Address = %02x:%02x:%02x:%02x:%02x:%02x",
			eeprom_hw_addr[0], eeprom_hw_addr[1], eeprom_hw_addr[2],
			eeprom_hw_addr[3], eeprom_hw_addr[4], eeprom_hw_addr[5]);

#if 0
	clog("wp1: sizeof(struct E100_crb) = %u", sizeof(struct E100_crb));
	clog("wp2: sizeof(struct E100_txcb) = %u", sizeof(struct E100_txcb));
	clog("wp2: sizeof(struct E100_rxcb) = %u", sizeof(struct E100_rxcb));
#endif
	init_dma_rings();

	irq_setmask_8259A(irq_mask_8259A & ~(1 << e100_irq_line));

	last_tx_id = E100_CBL_SIZE - 1;
	last_rx_id = E100_RFA_SIZE - 1;
	restart_rx();

	return 1;
}

int
e100_get_hw_addr(void *buf, size_t len)
{
	assert(len == HWADDR_LEN_82559ER);

	memmove(buf, eeprom_hw_addr, len);

	return len;
}

int
e100_tx_pkt(const void *buf, size_t len)
{
	int i, busy = -1, next = -1;
	struct E100_crb *pcb = NULL;

#if 0
	for (i = 0; i < E100_CBL_SIZE; ++i) {
		if (cbl[i].cmd == E100_TCB_CW_NULL) {
			if (next < 0) {
				next = i;
			}
		}
		else {
			busy = i;
		}
	}
	if (busy != (E100_CBL_SIZE - 1)) {
		next = busy + 1;
	}

	if (next < 0) {
		return -E_NO_BUFS;
	}

	pcb = &cbl[next];
#endif

	if (last_tx_id == E100_CBL_SIZE - 1) {
		i = 0;
	}
	else {
		i = last_tx_id + 1;
	}
	if (cbl[i].cmd == E100_TCB_CW_NULL) {
		assert(cbl[i].status == E100_TCB_SW_NULL);
		pcb = &cbl[i];
	}

	if (!pcb) {
		return -E_NO_BUFS;
	}

	memmove(pcb->data.tx_data.data, buf, len);
	pcb->data.tx_data.tcb_bcount = len;
	pcb->cmd = E100_TCB_CW_S | E100_TCB_CW_CMD;
	last_tx_id = i;

	for (i = 0; i < E100_CBL_SIZE; ++i) {
		if ((cbl[i].status & (E100_TCB_SW_C | E100_TCB_SW_OK)) ==
				(E100_TCB_SW_C | E100_TCB_SW_OK)) {
			cbl[i].cmd = E100_TCB_CW_NULL;
			cbl[i].status = E100_TCB_SW_NULL;
		}
	}

	restart_cu();

	return len;
}

int
e100_rx_pkt(void *buf, size_t len)
{
	int i;
	struct E100_crb *prfd = NULL;
	size_t ac_len;

	if (last_rx_id == E100_RFA_SIZE - 1) {
		i = 0;
	}
	else {
		i = last_rx_id + 1;
	}
	if ((rfa[i].status & (E100_RFD_SW_C | E100_RFD_SW_OK))
			== (E100_RFD_SW_C | E100_RFD_SW_OK)) {
		assert((rfa[i].data.rx_data.rfd_count & (E100_RFD_AC_EOF
						| E100_RFD_AC_F)) == (E100_RFD_AC_EOF
						| E100_RFD_AC_F));
		prfd = &rfa[i];
	}

	if (!prfd) {
		return -E_NO_DATA;
	}

	//print_dma_ring_rx();
	ac_len = prfd->data.rx_data.rfd_count & E100_RFD_AC_DMASK;
	assert(ac_len <= len);
	//hexdump("e100_rx_pkt: ", prfd->data.rx_data.data, ac_len);
	memmove(buf, prfd->data.rx_data.data, ac_len);
	prfd->data.rx_data.rfd_count = E100_RFD_AC_NULL;
	prfd->status = E100_RFD_SW_NULL;
	last_rx_id = i;
	//clog("wp1: last_rx_id = %d, ac_len = %u", last_rx_id, ac_len);

	restart_rx();

	return ac_len;
}

