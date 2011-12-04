#ifndef JOS_KERN_E100_H
#define JOS_KERN_E100_H

#include <inc/ns.h>

#include <kern/pci.h>


#define	VENDOR_ID_82559ER				0x8086
#define	PRODUCT_ID_82559ER				0x1209

#define HWADDR_LEN_82559ER				6

#define E100_CSR_SCB_SW(base)			(base + 0x0)
#define E100_CSR_SCB_CW(base)			(base + 0x2)
#define E100_CSR_SCB_GP(base)			(base + 0x4)
#define E100_CSR_PORT(base)				(base + 0x8)
#define E100_CSR_EEPROM_CNTL(base)		(base + 0xe)

/* ===== SCB related values ===== */

#define E100_SCB_SW_NULL				0x0000
#define E100_SCB_SW_CU_DMASK			0x00c0
#define E100_SCB_SW_CU_IDLE				0x0000
#define E100_SCB_SW_CU_SUSP				0x0040
#define E100_SCB_SW_RU_DMASK			0x003c
#define E100_SCB_SW_RU_IDLE				0x0000
#define E100_SCB_SW_RU_SUSP				0x0004

#define E100_SCB_CW_NULL				0x0000
//#define E100_SCB_CW_INTR				0xbe00
#define E100_SCB_CW_INTR				0x0100
#define E100_SCB_CW_CU_DMASK			0x00f0
#define E100_SCB_CW_CU_START			0x0010
#define E100_SCB_CW_CU_RESUME			0x0020
#define E100_SCB_CW_RU_DMASK			0x0007
#define E100_SCB_CW_RU_START			0x0001
#define E100_SCB_CW_RU_RESUME			0x0002

/* =====  ===== */

#define E100_PORT_FN_SW_RESET			0x0

#define E100_CBL_SIZE					(5 * PGSIZE / sizeof(struct E100_crb))
#define E100_RFA_SIZE					(10 * PGSIZE / sizeof(struct E100_crb))

/* ===== TCB related values ===== */

#define E100_TCB_SW_NULL				0x0
#define E100_TCB_SW_U					0x1000
#define E100_TCB_SW_OK					0x2000
#define E100_TCB_SW_C					0x8000

#define E100_TCB_CW_NULL				0x0
#define E100_TCB_CW_CMD					0x4
#define E100_TCB_CW_SF					0x8
#define E100_TCB_CW_NC					0x10
#define E100_TCB_CW_I					0x2000
#define E100_TCB_CW_S					0x4000
#define E100_TCB_CW_EL					0x8000

#define E100_TCB_TBD_NULL				0xffffffff
#define E100_TCB_TX_THRESHOLD			0xe0
#define E100_TCB_TBD_CNT				0x00

/* =====  ===== */
/* ===== RFD related values ===== */

#define E100_RFD_SW_NULL				0x0
#define E100_RFD_SW_OK					0x2000
#define E100_RFD_SW_C					0x8000

#define E100_RFD_CW_NULL				0x0
#define E100_RFD_CW_CMD					0x4
#define E100_RFD_CW_SF					0x8
#define E100_RFD_CW_H					0x10
#define E100_RFD_CW_S					0x4000
#define E100_RFD_CW_EL					0x8000

#define E100_RFD_RSV_NULL				0xffffffff
#define E100_RFD_AC_NULL				0x0000
#define E100_RFD_AC_DMASK				0x3fff
#define E100_RFD_AC_F					0x4000
#define E100_RFD_AC_EOF					0x8000

/* =====  ===== */
/* ===== EEPROM CR related values ===== */

#define E100_EEPROM_REG_WIDTH			16

#define E100_EC_EESK_SHIFT				0
#define E100_EC_EESK_DMASK				(1u << E100_EC_EESK_SHIFT)
#define E100_EC_EECS_SHIFT				1
#define E100_EC_EECS_DMASK				(1u << E100_EC_EECS_SHIFT)
#define E100_EC_EEDI_SHIFT				2
#define E100_EC_EEDI_DMASK				(1u << E100_EC_EEDI_SHIFT)
#define E100_EC_EEDO_SHIFT				3
#define E100_EC_EEDO_DMASK				(1u << E100_EC_EEDO_SHIFT)

#define E100_EC_OP_LEN					3
#define E100_EC_OP_READ					0x6
#define E100_EC_OP_WRITE				0x5

/* =====  ===== */


struct E100_txcb {
	uint32_t tbd_addr;
	uint16_t tcb_bcount;
	uint8_t tx_thrs;
	uint8_t tbd_count;
	char data[ETH_PKTSZ_MAX];
} __attribute__((packed));

struct E100_rxcb {
	uint32_t rfd_rsv;
	volatile uint16_t rfd_count;
	uint16_t rfd_size;
	char data[ETH_PKTSZ_MAX];
} __attribute__((packed));

struct E100_crb {
	volatile uint16_t status;
	uint16_t cmd;
	uint32_t link;
	union E100_cmd_data {
		struct E100_txcb tx_data;
		struct E100_rxcb rx_data;
	} data;
} __attribute__((packed));

#if 0
struct E100_rfd {
	//volatile uint16_t status;
} __attribute__((packed));
#endif

int pci_network_8255x_attach(struct pci_func *pcif);
int e100_get_hw_addr(void *buf, size_t len);
int e100_tx_pkt(const void *buf, size_t len);
int e100_rx_pkt(void *buf, size_t len);

extern uint8_t e100_irq_line;

#endif	// JOS_KERN_E100_H
