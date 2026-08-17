// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * MT7622 parallel raw NAND read-only prototype.
 *
 * This is deliberately a low-level, compile-only/read-only stage for the
 * WSR-2533DHP2 Stage 2 work. It does not register the legacy NAND MTD device
 * yet and contains no program/erase path. Register values are derived from
 * Linux v6.12 mtk_nand.c/ecc-mtk.c and the target's 2 KiB/64-byte NAND.
 * The referenced Linux files are Copyright (C) 2016 MediaTek Inc., authored
 * by Xiaolei Li and Jorge Ramirez-Ortiz, and offered as GPL-2.0 OR MIT. This
 * U-Boot adaptation selects their MIT option for derived definitions and
 * logic, retains that attribution, and distributes the combined adaptation
 * under GPL-2.0-or-later. See PROVENANCE.md for exact source paths.
 */

#include <mapmem.h>
#if CONFIG_IS_ENABLED(CMD_MT7622_NAND_RO)
#include <command.h>
#include <display_options.h>
#include <image.h>
#include <u-boot/crc.h>
#include <asm/gpio.h>
#include <asm/unaligned.h>
#include <linux/err.h>
#include <linux/libfdt.h>
#endif
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/string.h>
#include <linux/types.h>
#if CONFIG_IS_ENABLED(CMD_MT7622_NAND_RO)
#include <vsprintf.h>
#endif

/* MT7622 NFI controller */
#define MT7622_NFI_BASE             0x1100d000
#define MT7622_ECC_BASE             0x1100e000
#define MT7622_PERICFG_BASE         0x10002000

#define NFI_CNFG                    0x000
#define  CNFG_READ_EN               BIT(1)
#define  CNFG_AHB                   BIT(0)
#define  CNFG_DMA_BURST_EN          BIT(2)
#define  CNFG_BYTE_RW               BIT(6)
#define  CNFG_HW_ECC_EN             BIT(8)
#define  CNFG_AUTO_FMT_EN           BIT(9)
#define  CNFG_OP_CUSTOM             (6 << 12)
#define NFI_PAGEFMT                 0x004
#define  PAGEFMT_FDM_ECC_SHIFT      12
#define  PAGEFMT_FDM_SHIFT          8
#define  PAGEFMT_SEC_SEL_512        BIT(2)
#define  PAGEFMT_2K_4K              1
#define NFI_CON                     0x008
#define  CON_FIFO_FLUSH             BIT(0)
#define  CON_NFI_RST                BIT(1)
#define  CON_BRD                    BIT(8)
#define  CON_SEC_SHIFT              12
#define NFI_CMD                     0x020
#define NFI_ACCCON                  0x00c
#define NFI_ADDRNOB                 0x030
#define NFI_COLADDR                 0x034
#define NFI_ROWADDR                 0x038
#define NFI_STRDATA                 0x040
#define NFI_CNRNB                   0x044
#define NFI_INTR_EN                 0x010
#define  INTR_AHB_DONE_EN           BIT(6)
#define NFI_INTR_STA                0x014
#define NFI_DATAR                  0x054
#define NFI_PIO_DIRDY               0x058
#define NFI_STA                     0x060
#define  STA_CMD                    BIT(0)
#define  STA_ADDR                   BIT(1)
#define  STA_BUSY                   BIT(8)
#define  NFI_FSM_MASK               (0xf << 16)
#define  NFI_FSM_CUSTDATA           (0xe << 16)
#define NFI_CSEL                    0x090
#define NFI_ADDRCNTR                0x070
#define NFI_STRADDR                 0x080
#define NFI_BYTELEN                 0x084
#define NFI_FDML(n)                 (0x0a0 + (n) * 8)
#define NFI_FDMM(n)                 (0x0a4 + (n) * 8)
#define NFI_MASTER_STA              0x224
#define  MASTER_STA_MASK            0x0fff
#define PERI_CG_STA0                0x018
#define PERI_CG_STA1                0x01c
#define  PERI_CG_SNFI               BIT(29)
#define  PERI_CG_NFI                BIT(30)
#define  PERI_CG_NFIECC             BIT(31)

/* MT7622 BCH ECC controller */
#define ECC_DECCON                  0x100
#define ECC_DECCNFG                0x104
#define  ECC_DEC_EMPTY_EN           BIT(31)
#define  ECC_DEC_CNFG_CORRECT       (0x3 << 12)
#define ECC_DECIDLE                 0x10c
#define ECC_DECENUM0                0x114
#define ECC_DECDONE                 0x11c
#define ECC_DECIRQ_EN               0x140
#define  ECC_IDLE_MASK              BIT(0)
#define  ECC_ERR_MASK               GENMASK(4, 0)
#define  ECC_ERR_SHIFT              5
#define  ECC_MODE_NFI               1
#define  ECC_MODE_SHIFT             4
#define  ECC_STRENGTH_INDEX_4       0
#define  ECC_PARITY_BITS            13

#define MT7622_NAND_PAGE_SIZE       2048
#define MT7622_NAND_ECC_SIZE        512
#define MT7622_NAND_ECC_STRENGTH    4
#define MT7622_NAND_SECTORS         4
#define MT7622_NAND_FDM_SIZE        8
#define MT7622_NAND_FDM_ECC_SIZE    1
#define MT7622_NAND_PAGES_PER_BLOCK 64
#define MT7622_NAND_BLOCKS          1024
#define MT7622_NAND_TOTAL_PAGES     (MT7622_NAND_BLOCKS * MT7622_NAND_PAGES_PER_BLOCK)
#define MT7622_NAND_BAD_MARK_OFFSET (3 * MT7622_NAND_ECC_SIZE + 464)
#define MT7622_NAND_TIMEOUT_US      500000

struct mt7622_nand_ro {
	void __iomem *nfi;
	void __iomem *ecc;
};

static struct mt7622_nand_ro mt7622_nfc;
static unsigned int mt7622_nand_ro_last_stage;

#if CONFIG_IS_ENABLED(CMD_MT7622_NAND_RO)
/* NFI AHB data buffers must not begin in the middle of a cache/DMA line. */
static u8 mt7622_nandro_page[MT7622_NAND_PAGE_SIZE]
	__aligned(ARCH_DMA_MINALIGN);
#endif

static int mt7622_wait_clear(void __iomem *addr, u32 mask, u32 timeout_us)
{
	while (timeout_us--) {
		if (!(readl(addr) & mask))
			return 0;
		udelay(1);
	}

	return -ETIMEDOUT;
}

static int mt7622_wait_set(void __iomem *addr, u32 mask, u32 timeout_us)
{
	while (timeout_us--) {
		if (readl(addr) & mask)
			return 0;
		udelay(1);
	}

	return -ETIMEDOUT;
}

static int mt7622_nfi_reset(struct mt7622_nand_ro *nfc)
{
	writel(CON_FIFO_FLUSH | CON_NFI_RST, nfc->nfi + NFI_CON);
	if (mt7622_wait_clear(nfc->nfi + NFI_MASTER_STA,
				      MASTER_STA_MASK, 1000000))
		return -ETIMEDOUT;

	writel(CON_FIFO_FLUSH | CON_NFI_RST, nfc->nfi + NFI_CON);
	writew(0, nfc->nfi + NFI_STRDATA);
	return 0;
}

static int mt7622_nfi_command(struct mt7622_nand_ro *nfc, u8 command)
{
	writel(command, nfc->nfi + NFI_CMD);
	return mt7622_wait_clear(nfc->nfi + NFI_STA, STA_CMD,
				 MT7622_NAND_TIMEOUT_US);
}

static int mt7622_nfi_address_byte(struct mt7622_nand_ro *nfc, u8 address)
{
	writel(address, nfc->nfi + NFI_COLADDR);
	writel(0, nfc->nfi + NFI_ROWADDR);
	writew(1, nfc->nfi + NFI_ADDRNOB);
	return mt7622_wait_clear(nfc->nfi + NFI_STA, STA_ADDR,
				 MT7622_NAND_TIMEOUT_US);
}

static int mt7622_nfi_wait_ready(struct mt7622_nand_ro *nfc)
{
	return mt7622_wait_clear(nfc->nfi + NFI_STA, STA_BUSY,
				 MT7622_NAND_TIMEOUT_US);
}

static int mt7622_nfi_wait_pio(struct mt7622_nand_ro *nfc)
{
	u32 timeout_us = MT7622_NAND_TIMEOUT_US;

	while (timeout_us--) {
		if (readb(nfc->nfi + NFI_PIO_DIRDY) & BIT(0))
			return 0;
		udelay(1);
	}

	return -ETIMEDOUT;
}

static int mt7622_nfi_wait_dma(struct mt7622_nand_ro *nfc, unsigned int sectors)
{
	u32 timeout_us = MT7622_NAND_TIMEOUT_US;

	while (timeout_us--) {
		u32 done = readl(nfc->nfi + NFI_BYTELEN) >> 12;

		if (done >= sectors)
			return 0;
		udelay(1);
	}

	return -ETIMEDOUT;
}

static void mt7622_nfi_show_dma_snapshot(struct mt7622_nand_ro *nfc)
{
	printf("NFI DMA SNAP CNFG=%08x CON=%08x STRADDR=%08x BYTELEN=%08x "
	       "ADDRCNTR=%08x INTR_EN=%08x INTR_STA=%08x STA=%08x MASTER=%08x\n",
	       readl(nfc->nfi + NFI_CNFG), readl(nfc->nfi + NFI_CON),
	       readl(nfc->nfi + NFI_STRADDR), readl(nfc->nfi + NFI_BYTELEN),
	       readl(nfc->nfi + NFI_ADDRCNTR), readl(nfc->nfi + NFI_INTR_EN),
	       readl(nfc->nfi + NFI_INTR_STA), readl(nfc->nfi + NFI_STA),
	       readl(nfc->nfi + NFI_MASTER_STA));
	printf("ECC DMA SNAP DECCON=%08x DECCNFG=%08x DECIDLE=%08x "
	       "DECDONE=%08x DECENUM0=%08x\n",
	       readl(nfc->ecc + ECC_DECCON), readl(nfc->ecc + ECC_DECCNFG),
	       readl(nfc->ecc + ECC_DECIDLE), readl(nfc->ecc + ECC_DECDONE),
	       readl(nfc->ecc + ECC_DECENUM0));
}

static int mt7622_nfi_read_byte(struct mt7622_nand_ro *nfc, u8 *value)
{
	u32 state = readl(nfc->nfi + NFI_STA) & NFI_FSM_MASK;
	u16 cnfg;

	if (state != NFI_FSM_CUSTDATA) {
		cnfg = readw(nfc->nfi + NFI_CNFG);
		cnfg |= CNFG_BYTE_RW | CNFG_READ_EN;
		writew(cnfg, nfc->nfi + NFI_CNFG);
		writel((8 << CON_SEC_SHIFT) | CON_BRD, nfc->nfi + NFI_CON);
		writew(1, nfc->nfi + NFI_STRDATA);
	}

	if (mt7622_nfi_wait_pio(nfc))
		return -ETIMEDOUT;

	*value = readb(nfc->nfi + NFI_DATAR);
	return 0;
}

static int mt7622_nfi_read_buf(struct mt7622_nand_ro *nfc, u8 *buf,
				       size_t length)
{
	while (length--) {
		if (mt7622_nfi_read_byte(nfc, buf++))
			return -ETIMEDOUT;
	}

	return 0;
}

static int mt7622_ecc_wait_sector(struct mt7622_nand_ro *nfc, int sector)
{
	return mt7622_wait_set(nfc->ecc + ECC_DECDONE, BIT(sector),
				       MT7622_NAND_TIMEOUT_US);
}

static u32 mt7622_ecc_error_count(struct mt7622_nand_ro *nfc, int sector)
{
	u32 reg = readl(nfc->ecc + ECC_DECENUM0 + (sector >> 2) * 4);

	return (reg >> ((sector & 3) * ECC_ERR_SHIFT)) & ECC_ERR_MASK;
}

static void mt7622_ecc_disable(struct mt7622_nand_ro *nfc)
{
	writew(0, nfc->ecc + ECC_DECIRQ_EN);
	writel(0, nfc->ecc + ECC_DECCON);
}

static int mt7622_ecc_enable_decode(struct mt7622_nand_ro *nfc)
{
	u32 decode_bits;
	u32 config;

	if (mt7622_wait_set(nfc->ecc + ECC_DECIDLE, ECC_IDLE_MASK,
				    MT7622_NAND_TIMEOUT_US))
		return -ETIMEDOUT;

	/* sector + FDM + parity, expressed in bits */
	/* The NFI stores 8 FDM bytes, but only 1 FDM byte is ECC-covered. */
	decode_bits = (MT7622_NAND_ECC_SIZE + MT7622_NAND_FDM_ECC_SIZE) * 8 +
			  MT7622_NAND_ECC_STRENGTH * ECC_PARITY_BITS;
	config = ECC_STRENGTH_INDEX_4 |
		 (ECC_MODE_NFI << ECC_MODE_SHIFT) |
		 (decode_bits << 16) |
		 ECC_DEC_CNFG_CORRECT | ECC_DEC_EMPTY_EN;
	writel(config, nfc->ecc + ECC_DECCNFG);
	writew(0, nfc->ecc + ECC_DECIRQ_EN);
	writel(1, nfc->ecc + ECC_DECCON);
	return 0;
}

void mt7622_nand_ro_init(struct mt7622_nand_ro *nfc)
{
	nfc->nfi = map_physmem(MT7622_NFI_BASE, 0x1000, MAP_NOCACHE);
	nfc->ecc = map_physmem(MT7622_ECC_BASE, 0x1000, MAP_NOCACHE);
	/* Linux mtk_nfc_hw_init() uses this ready/busy timing value. */
	writew(0xf1, nfc->nfi + NFI_CNRNB);
	/* Leave the ECC block idle until a read operation enables it. */
	writel(0, nfc->ecc + ECC_DECCON);
	writew(0, nfc->ecc + ECC_DECIRQ_EN);
}

int mt7622_nand_ro_read_id(struct mt7622_nand_ro *nfc, u8 *id, size_t length)
{
	int ret;

	if (!length)
		return -EINVAL;

	mt7622_nand_ro_last_stage = 1;
	ret = mt7622_nfi_reset(nfc);
	if (ret)
		return ret;

	writew(CNFG_OP_CUSTOM, nfc->nfi + NFI_CNFG);
	writew(0, nfc->nfi + NFI_CSEL);
	ret = mt7622_nfi_command(nfc, 0x90);
	if (ret)
		return ret;
	ret = mt7622_nfi_address_byte(nfc, 0);
	if (ret)
		return ret;
	ret = mt7622_nfi_read_buf(nfc, id, length);
	mt7622_nfi_reset(nfc);
	return ret;
}

/*
 * Read one corrected 2 KiB page. The OOB output contains the four 8-byte
 * FDM regions; ECC parity remains in the controller and is not exported.
 * No NAND program/erase command exists in this prototype.
 */
int mt7622_nand_ro_read_page(struct mt7622_nand_ro *nfc, u32 page,
				     u8 *data, u8 *fdm)
{
	u32 pagefmt;
	dma_addr_t dma_addr;
	u16 cnfg;
	int sector, ret;

	mt7622_nand_ro_last_stage = 1;
	ret = mt7622_nfi_reset(nfc);
	if (ret)
		return ret;

	pagefmt = PAGEFMT_2K_4K | PAGEFMT_SEC_SEL_512;
	/* MT7622 spare index 0 = 16 bytes; FDM is 8 bytes, ECC-FDM is 1. */
	pagefmt |= 8 << PAGEFMT_FDM_SHIFT;
	pagefmt |= 1 << PAGEFMT_FDM_ECC_SHIFT;
	writel(pagefmt, nfc->nfi + NFI_PAGEFMT);
	writew(CNFG_OP_CUSTOM | CNFG_READ_EN | CNFG_AUTO_FMT_EN |
		       CNFG_HW_ECC_EN, nfc->nfi + NFI_CNFG);
	writew(0, nfc->nfi + NFI_CSEL);

	mt7622_nand_ro_last_stage = 2;
	ret = mt7622_nfi_command(nfc, 0x00);
	if (ret)
		return ret;
	mt7622_nand_ro_last_stage = 3;
	ret = mt7622_nfi_address_byte(nfc, 0);
	if (ret)
		return ret;
	mt7622_nand_ro_last_stage = 4;
	ret = mt7622_nfi_address_byte(nfc, 0);
	if (ret)
		return ret;
	mt7622_nand_ro_last_stage = 5;
	ret = mt7622_nfi_address_byte(nfc, page & 0xff);
	if (ret)
		return ret;
	mt7622_nand_ro_last_stage = 6;
	ret = mt7622_nfi_address_byte(nfc, (page >> 8) & 0xff);
	if (ret)
		return ret;
	mt7622_nand_ro_last_stage = 7;
	ret = mt7622_nfi_address_byte(nfc, (page >> 16) & 0xff);
	if (ret)
		return ret;
	mt7622_nand_ro_last_stage = 8;
	ret = mt7622_nfi_command(nfc, 0x30);
	if (ret)
		return ret;
	mt7622_nand_ro_last_stage = 9;
	ret = mt7622_nfi_wait_ready(nfc);
	if (ret)
		return ret;

	mt7622_nand_ro_last_stage = 10;
	ret = mt7622_ecc_enable_decode(nfc);
	if (ret)
		return ret;

	/* Full-page data uses the NFI AHB/DMA path, as in Linux mtk_nand.c. */
	mt7622_nand_ro_last_stage = 11;
	dma_addr = dma_map_single(data, MT7622_NAND_PAGE_SIZE,
				  DMA_FROM_DEVICE);
	cnfg = readw(nfc->nfi + NFI_CNFG);
	cnfg |= CNFG_AHB | CNFG_DMA_BURST_EN;
	writew(cnfg, nfc->nfi + NFI_CNFG);
	writel(MT7622_NAND_SECTORS << CON_SEC_SHIFT, nfc->nfi + NFI_CON);
	writel((u32)dma_addr, nfc->nfi + NFI_STRADDR);
	/* Linux enables the AHB-done interrupt before triggering the burst. */
	readw(nfc->nfi + NFI_INTR_STA);
	writew(INTR_AHB_DONE_EN, nfc->nfi + NFI_INTR_EN);
	writel(readl(nfc->nfi + NFI_CON) | CON_BRD,
	       nfc->nfi + NFI_CON);
	writew(1, nfc->nfi + NFI_STRDATA);

	mt7622_nand_ro_last_stage = 12;
	ret = mt7622_nfi_wait_dma(nfc, MT7622_NAND_SECTORS);
	dma_unmap_single(dma_addr, MT7622_NAND_PAGE_SIZE, DMA_FROM_DEVICE);
	if (ret) {
		mt7622_nfi_show_dma_snapshot(nfc);
		goto out;
	}

	for (sector = 0; sector < MT7622_NAND_SECTORS; sector++) {
		mt7622_nand_ro_last_stage = 20 + sector;
		ret = mt7622_ecc_wait_sector(nfc, sector);
		if (ret)
			goto out;
		if (mt7622_ecc_error_count(nfc, sector) == ECC_ERR_MASK) {
			ret = -EBADMSG;
			goto out;
		}
		if (fdm) {
			int j;
			u32 low = readl(nfc->nfi + NFI_FDML(sector));
			u32 high = readl(nfc->nfi + NFI_FDMM(sector));

			for (j = 0; j < 4; j++)
				fdm[sector * 8 + j] = low >> (j * 8);
			for (j = 0; j < 4; j++)
				fdm[sector * 8 + 4 + j] = high >> (j * 8);
		}
	}

out:
	writew(0, nfc->nfi + NFI_INTR_EN);
	mt7622_ecc_disable(nfc);
	writel(0, nfc->nfi + NFI_CON);
	return ret;
}

/* Keep normal boot non-invasive until the MTD registration layer is ready. */
void board_nand_init(void)
{
	mt7622_nand_ro_init(&mt7622_nfc);
}

#if CONFIG_IS_ENABLED(CMD_MT7622_NAND_RO)
static int mt7622_nandro_read_range(phys_addr_t dest, u32 start_page,
					    u32 count)
{
	u8 fdm[MT7622_NAND_SECTORS * MT7622_NAND_FDM_SIZE];
	void *buffer;
	u64 length;
	u32 page;
	int ret;

	if (!count || start_page >= MT7622_NAND_TOTAL_PAGES ||
		count > MT7622_NAND_TOTAL_PAGES - start_page ||
		(dest & (ARCH_DMA_MINALIGN - 1)))
		return -EINVAL;

	length = (u64)count * MT7622_NAND_PAGE_SIZE;
	if (length > ULONG_MAX)
		return -EOVERFLOW;

	buffer = map_sysmem(dest, (ulong)length);
	for (page = 0; page < count; page++) {
		u8 *page_data = (u8 *)buffer +
			page * MT7622_NAND_PAGE_SIZE;

		ret = mt7622_nand_ro_read_page(&mt7622_nfc,
						start_page + page, page_data, fdm);
		if (ret) {
			printf("nandro: read failed page=0x%x: %d stage=%u\n",
				       start_page + page, ret,
				       mt7622_nand_ro_last_stage);
			unmap_sysmem(buffer);
			return ret;
		}
		/* Repack corrected data like non-raw MTD; do not skip bad blocks. */
		page_data[MT7622_NAND_BAD_MARK_OFFSET] =
			fdm[(MT7622_NAND_SECTORS - 1) *
			    MT7622_NAND_FDM_SIZE];
	}
	unmap_sysmem(buffer);

	return 0;
}

static void mt7622_nandro_show_regs(void)
{
	void __iomem *peri;
	u32 cg0, cg1;

	mt7622_nand_ro_init(&mt7622_nfc);
	peri = map_physmem(MT7622_PERICFG_BASE, 0x100, MAP_NOCACHE);
	cg0 = readl(peri + PERI_CG_STA0);
	cg1 = readl(peri + PERI_CG_STA1);

	printf("NFI CNFG=%08x PAGEFMT=%08x ACCCON=%08x CON=%08x\n",
	       readl(mt7622_nfc.nfi + NFI_CNFG),
	       readl(mt7622_nfc.nfi + NFI_PAGEFMT),
	       readl(mt7622_nfc.nfi + NFI_ACCCON),
	       readl(mt7622_nfc.nfi + NFI_CON));
	printf("NFI CNRNB=%08x CSEL=%08x STA=%08x MASTER=%08x\n",
	       readl(mt7622_nfc.nfi + NFI_CNRNB),
	       readl(mt7622_nfc.nfi + NFI_CSEL),
	       readl(mt7622_nfc.nfi + NFI_STA),
	       readl(mt7622_nfc.nfi + NFI_MASTER_STA));
	printf("PERICFG CG_STA0=%08x (SNFI=%d NFI=%d) CG_STA1=%08x (NFIECC=%d)\n",
	       cg0, !!(cg0 & PERI_CG_SNFI), !!(cg0 & PERI_CG_NFI), cg1,
	       !!(cg1 & PERI_CG_NFIECC));
	/* This command intentionally only observes inherited pin/clock state. */
	unmap_sysmem(peri);
}

static int do_mt7622_nandro(struct cmd_tbl *cmdtp, int flag, int argc,
				    char *const argv[])
{
	u8 id[5];
	u8 fdm[MT7622_NAND_SECTORS * MT7622_NAND_FDM_SIZE];
	u8 prefix[32];
	void *data = mt7622_nandro_page;
	phys_addr_t addr;
	u32 page;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	if (!strcmp(argv[1], "regs")) {
		if (argc != 2)
			return CMD_RET_USAGE;
		mt7622_nandro_show_regs();
		return CMD_RET_SUCCESS;
	}

	mt7622_nand_ro_init(&mt7622_nfc);
	if (!strcmp(argv[1], "id")) {
		if (argc != 2)
			return CMD_RET_USAGE;
		ret = mt7622_nand_ro_read_id(&mt7622_nfc, id, sizeof(id));
		if (ret) {
			printf("nandro: read id failed: %d\n", ret);
			return CMD_RET_FAILURE;
		}
		printf("NAND ID:");
		for (ret = 0; ret < ARRAY_SIZE(id); ret++)
			printf(" %02x", id[ret]);
		printf("\n");
		return CMD_RET_SUCCESS;
	}

	if (!strcmp(argv[1], "page")) {
		if (argc != 3 && argc != 4)
			return CMD_RET_USAGE;
		page = hextoul(argv[2], NULL);
		if (argc == 4) {
			addr = hextoul(argv[3], NULL);
			data = map_sysmem(addr, MT7622_NAND_PAGE_SIZE);
		}
		ret = mt7622_nand_ro_read_page(&mt7622_nfc, page, data, fdm);
		if (!ret)
			memcpy(prefix, data, sizeof(prefix));
		if (argc == 4)
			unmap_sysmem(data);
		if (ret) {
			printf("nandro: read page 0x%x failed: %d stage=%u\n", page,
			       ret, mt7622_nand_ro_last_stage);
			return CMD_RET_FAILURE;
		}
		printf("page=0x%x offset=0x%x first 32 bytes:\n", page,
		       page * MT7622_NAND_PAGE_SIZE);
		print_buffer(0, prefix, 1, sizeof(prefix), 16);
		printf("FDM (4 x 8 bytes):\n");
		print_buffer(0, fdm, 1, sizeof(fdm), 16);
		return CMD_RET_SUCCESS;
	}

	if (!strcmp(argv[1], "bad")) {
		u32 start_block, count, block;
		u8 fdm[MT7622_NAND_SECTORS * MT7622_NAND_FDM_SIZE];
		u8 marker;
		unsigned int bad = 0;

		if (argc != 4)
			return CMD_RET_USAGE;
		start_block = hextoul(argv[2], NULL);
		count = hextoul(argv[3], NULL);
		if (!count || start_block >= MT7622_NAND_BLOCKS ||
		    count > MT7622_NAND_BLOCKS - start_block)
			return CMD_RET_USAGE;

		for (block = start_block; block < start_block + count; block++) {
			u32 page = block * MT7622_NAND_PAGES_PER_BLOCK;
			marker = 0xff;

			/* Linux MTK maps the 2 KiB-page bad marker to the corrected
			 * main-data offset 3*512 + (2048 % 528) = 0x7d0. */
			ret = mt7622_nand_ro_read_page(&mt7622_nfc, page, data, fdm);
			if (ret) {
				printf("nandro: bad scan read failed block=0x%x page=0x%x: %d stage=%u\n",
				       block, page, ret, mt7622_nand_ro_last_stage);
				return CMD_RET_FAILURE;
			}
			if (mt7622_nandro_page[MT7622_NAND_BAD_MARK_OFFSET] != 0xff) {
				bad = 1;
				marker = mt7622_nandro_page[MT7622_NAND_BAD_MARK_OFFSET];
			}

			ret = mt7622_nand_ro_read_page(&mt7622_nfc, page + 1, data, fdm);
			if (ret) {
				printf("nandro: bad scan read failed block=0x%x page=0x%x: %d stage=%u\n",
				       block, page + 1, ret, mt7622_nand_ro_last_stage);
				return CMD_RET_FAILURE;
			}
			if (mt7622_nandro_page[MT7622_NAND_BAD_MARK_OFFSET] != 0xff) {
				bad = 1;
				marker = mt7622_nandro_page[MT7622_NAND_BAD_MARK_OFFSET];
			}

			if (bad) {
				printf("bad block=0x%x marker=%02x\n", block, marker);
				bad = 0;
			}
		}

		printf("bad scan complete: start=0x%x count=0x%x\n",
		       start_block, count);
		return CMD_RET_SUCCESS;
	}

	if (!strcmp(argv[1], "range")) {
		u32 start_page, count, page;
		u32 checksum = 0;

		if (argc != 4)
			return CMD_RET_USAGE;
		start_page = hextoul(argv[2], NULL);
		count = hextoul(argv[3], NULL);
		if (!count || start_page >= MT7622_NAND_TOTAL_PAGES ||
		    count > MT7622_NAND_TOTAL_PAGES - start_page)
			return CMD_RET_USAGE;

		for (page = start_page; page < start_page + count; page++) {
			ret = mt7622_nand_ro_read_page(&mt7622_nfc, page, data, fdm);
			if (ret) {
				printf("nandro: range read failed page=0x%x: %d stage=%u\n",
				       page, ret, mt7622_nand_ro_last_stage);
				return CMD_RET_FAILURE;
			}
			/* Match Linux's non-raw per-page layout: the MTK bad-block
			 * marker is physically exposed at 0x7d0 in main data, while
			 * MTD places the corrected FDM byte there. This does not skip
			 * or remap physical eraseblocks. */
			mt7622_nandro_page[MT7622_NAND_BAD_MARK_OFFSET] =
				fdm[(MT7622_NAND_SECTORS - 1) * MT7622_NAND_FDM_SIZE];
			checksum = crc32(checksum, mt7622_nandro_page,
					MT7622_NAND_PAGE_SIZE);
		}

		printf("range complete: start=0x%x count=0x%x bytes=0x%x crc32=%08x\n",
		       start_page, count, count * MT7622_NAND_PAGE_SIZE, checksum);
		return CMD_RET_SUCCESS;
	}

	if (!strcmp(argv[1], "read")) {
		u32 start_page, count, page;
		phys_addr_t dest;
		void *buffer;
		u64 length;

		if (argc != 5)
			return CMD_RET_USAGE;
		dest = hextoul(argv[2], NULL);
		start_page = hextoul(argv[3], NULL);
		count = hextoul(argv[4], NULL);
		if (!count || start_page >= MT7622_NAND_TOTAL_PAGES ||
			count > MT7622_NAND_TOTAL_PAGES - start_page)
			return CMD_RET_USAGE;
		length = (u64)count * MT7622_NAND_PAGE_SIZE;
		if (length > ULONG_MAX ||
			(dest & (ARCH_DMA_MINALIGN - 1)))
			return CMD_RET_USAGE;

		buffer = map_sysmem(dest, (ulong)length);
		for (page = 0; page < count; page++) {
			u8 *page_data = (u8 *)buffer +
				page * MT7622_NAND_PAGE_SIZE;

			ret = mt7622_nand_ro_read_page(&mt7622_nfc,
						       start_page + page,
						       page_data, fdm);
			if (ret) {
				printf("nandro: read failed page=0x%x: %d stage=%u\n",
				       start_page + page, ret,
				       mt7622_nand_ro_last_stage);
				unmap_sysmem(buffer);
				return CMD_RET_FAILURE;
			}
			/* Repack corrected data like non-raw MTD; do not skip blocks. */
			page_data[MT7622_NAND_BAD_MARK_OFFSET] =
				fdm[(MT7622_NAND_SECTORS - 1) *
				    MT7622_NAND_FDM_SIZE];
		}
		unmap_sysmem(buffer);

		printf("read complete: dest=0x%llx start=0x%x count=0x%x bytes=0x%llx\n",
		       (unsigned long long)dest, start_page, count,
		       (unsigned long long)length);
		return CMD_RET_SUCCESS;
	}

	return CMD_RET_USAGE;
}

#if CONFIG_IS_ENABLED(CMD_WSR_SELECTOR)
#define WSRSEL_NAND_WRITESIZE	2048U
#define WSRSEL_DHP2_HEADER	0x1cU
#define WSRSEL_IMAGE1_OFFSET	0x00200000U
#define WSRSEL_IMAGE1_SIZE	0x03a00000U
#define WSRSEL_IMAGE2_OFFSET	0x03c00000U
#define WSRSEL_IMAGE2_SIZE	0x00600000U
#define WSRSEL_MAX_FIT_SIZE	0x00600000U
#define WSRSEL_PROBE_SIZE	0x00001000U
#define WSRSEL_STAGE_ADDR	0x41000000UL
#define WSRSEL_VALIDATE_ADDR	0x41800000UL
#define WSRSEL_BOOT_ADDR	0x40080000UL

enum wsrsel_expected_format {
	WSRSEL_EXPECT_DHP2,
	WSRSEL_EXPECT_RAW_FIT,
};

static const char *wsrsel_expected_name(enum wsrsel_expected_format expected)
{
	return expected == WSRSEL_EXPECT_DHP2 ? "DHP2" : "RAW_FIT";
}

static int wsrsel_sample_gpio(unsigned int pin, int *value)
{
	int ret;
	int request_ret;

	request_ret = gpio_request(pin, "wsr_selector");
	if (request_ret && request_ret != -EBUSY)
		return request_ret;
	ret = gpio_direction_input(pin);
	if (!ret)
		*value = gpio_get_value(pin);
	if (request_ret != -EBUSY)
		gpio_free(pin);
	if (ret)
		return ret;
	if (IS_ERR_VALUE(*value))
		return *value;
	return 0;
}

static int wsrsel_check_fit(const void *fit, u32 max_size, u32 *fit_size)
{
	int ret;
	u32 size;

	ret = fdt_check_header(fit);
	if (ret) {
		printf("WSRSEL format error: FIT/FDT header ret=%d\n", ret);
		return -EINVAL;
	}

	size = fdt_totalsize(fit);
	if (size < sizeof(struct fdt_header) || size > max_size ||
		size > WSRSEL_MAX_FIT_SIZE) {
		printf("WSRSEL format error: FIT totalsize=0x%x max=0x%x\n",
		       size, max_size);
		return -EFBIG;
	}

	ret = fit_check_format(fit, size);
	if (ret) {
		printf("WSRSEL format error: FIT check ret=%d\n", ret);
		return ret;
	}
	*fit_size = size;
	return 0;
}

/*
 * DHP2's 28-byte wrapper leaves the inner FIT at a 4-byte-aligned address.
 * libfdt deliberately rejects an unaligned blob with FDT_ERR_ALIGNMENT, so
 * the NAND probe reads only the two big-endian header fields here.  The full
 * FIT is copied to WSRSEL_VALIDATE_ADDR before fit_check_format() is called.
 */
static int wsrsel_probe_fit_size(const void *fit, u32 max_size, u32 *fit_size)
{
	u32 magic = get_unaligned_be32(fit);
	u32 size = get_unaligned_be32((const u8 *)fit + 4);

	if (magic != FDT_MAGIC) {
		printf("WSRSEL format error: FIT magic=0x%08x\n", magic);
		return -EPROTO;
	}
	if (size < sizeof(struct fdt_header) || size > max_size ||
		size > WSRSEL_MAX_FIT_SIZE) {
		printf("WSRSEL format error: FIT totalsize=0x%x max=0x%x\n",
		       size, max_size);
		return -EFBIG;
	}
	*fit_size = size;
	return 0;
}

static int wsrsel_dispatch(u32 flash_offset, u32 slot_size,
				   enum wsrsel_expected_format expected,
				   bool boot)
{
	u8 *probe;
	u8 *fit;
	void *stage;
	void *dst;
	u32 page;
	u32 image_length = 0;
	u32 fit_offset;
	u32 fit_size;
	u32 read_size;
	int ret;
	char bootm_cmd[32];
	void *validate;

	if (flash_offset % WSRSEL_NAND_WRITESIZE) {
		printf("WSRSEL stop: unaligned byte offset=0x%x writesize=%u\n",
		       flash_offset, WSRSEL_NAND_WRITESIZE);
		return -EINVAL;
	}
	page = flash_offset / WSRSEL_NAND_WRITESIZE;
	printf("WSRSEL dispatch expected=%s flash_offset=0x%x page=0x%x "
	       "slot_size=0x%x\n", wsrsel_expected_name(expected),
	       flash_offset, page, slot_size);

	mt7622_nand_ro_init(&mt7622_nfc);
	ret = mt7622_nandro_read_range(WSRSEL_STAGE_ADDR, page,
					      WSRSEL_PROBE_SIZE / WSRSEL_NAND_WRITESIZE);
	if (ret)
		return ret;

	probe = map_sysmem(WSRSEL_STAGE_ADDR, WSRSEL_PROBE_SIZE);
	if (expected == WSRSEL_EXPECT_DHP2) {
		if (memcmp(probe, "DHP2", 4)) {
			printf("WSRSEL stop: expected DHP2, probe magic=%08x\n",
			       get_unaligned_be32(probe));
			unmap_sysmem(probe);
			return -EPROTO;
		}
		image_length = get_unaligned_le32(probe + 4);
		if (image_length < WSRSEL_DHP2_HEADER + sizeof(struct fdt_header) ||
			image_length > slot_size ||
			get_unaligned_le32(probe + 0x10) != WSRSEL_DHP2_HEADER) {
			printf("WSRSEL stop: invalid DHP2 length=0x%x header_size=0x%x\n",
			       image_length, get_unaligned_le32(probe + 0x10));
			unmap_sysmem(probe);
			return -EINVAL;
		}
		fit_offset = WSRSEL_DHP2_HEADER;
	} else {
		if (get_unaligned_be32(probe) != FDT_MAGIC) {
			printf("WSRSEL stop: expected raw FIT, probe magic=%08x\n",
			       get_unaligned_be32(probe));
			unmap_sysmem(probe);
			return -EPROTO;
		}
		fit_offset = 0;
	}

	fit = probe + fit_offset;
	ret = wsrsel_probe_fit_size(fit, slot_size - fit_offset, &fit_size);
	if (ret) {
		unmap_sysmem(probe);
		return ret;
	}
	if (expected == WSRSEL_EXPECT_DHP2 &&
		(fit_offset + fit_size > image_length)) {
		printf("WSRSEL stop: FIT exceeds DHP2 length fit_end=0x%x "
		       "image_length=0x%x\n", fit_offset + fit_size,
		       image_length);
		unmap_sysmem(probe);
		return -EFBIG;
	}
	read_size = ALIGN(fit_offset + fit_size, WSRSEL_NAND_WRITESIZE);
	if (read_size > WSRSEL_MAX_FIT_SIZE || read_size > slot_size) {
		printf("WSRSEL stop: read_size=0x%x exceeds read workspace/slot\n",
		       read_size);
		unmap_sysmem(probe);
		return -EFBIG;
	}
	printf("WSRSEL probe ok format=%s fit_offset=0x%x fit_totalsize=0x%x "
	       "read_size=0x%x\n", wsrsel_expected_name(expected),
	       fit_offset, fit_size, read_size);
	unmap_sysmem(probe);

	if (!boot)
		return 0;

	ret = mt7622_nandro_read_range(WSRSEL_STAGE_ADDR, page,
					      read_size / WSRSEL_NAND_WRITESIZE);
	if (ret)
		return ret;

	stage = map_sysmem(WSRSEL_STAGE_ADDR, read_size);
	validate = map_sysmem(WSRSEL_VALIDATE_ADDR, fit_size);
	memcpy(validate, (u8 *)stage + fit_offset, fit_size);
	ret = wsrsel_check_fit(validate, fit_size, &fit_size);
	unmap_sysmem(validate);
	if (ret) {
		unmap_sysmem(stage);
		return ret;
	}
	dst = map_sysmem(WSRSEL_BOOT_ADDR, fit_size);
	/* Boot only the FIT; keep the DHP2/TRX wrapper in Flash untouched. */
	memcpy(dst, (u8 *)stage + fit_offset, fit_size);
	unmap_sysmem(dst);
	unmap_sysmem(stage);

	snprintf(bootm_cmd, sizeof(bootm_cmd), "bootm 0x%lx",
		 (ulong)WSRSEL_BOOT_ADDR);
	printf("WSRSEL bootm=%s (bootargs unchanged)\n", bootm_cmd);
	return run_command(bootm_cmd, 0);
}

static int do_wsrsel(struct cmd_tbl *cmdtp, int flag, int argc,
			     char *const argv[])
{
	int gpio1;
	int gpio16;
	int ret;

	if (argc == 3 && !strcmp(argv[1], "probe")) {
		if (!strcmp(argv[2], "image1"))
			return wsrsel_dispatch(WSRSEL_IMAGE1_OFFSET,
						       WSRSEL_IMAGE1_SIZE,
						       WSRSEL_EXPECT_DHP2, false);
		if (!strcmp(argv[2], "image2"))
			return wsrsel_dispatch(WSRSEL_IMAGE2_OFFSET,
						       WSRSEL_IMAGE2_SIZE,
						       WSRSEL_EXPECT_RAW_FIT, false);
		return CMD_RET_USAGE;
	}

	if (argc != 2 || strcmp(argv[1], "boot"))
		return CMD_RET_USAGE;

	ret = wsrsel_sample_gpio(1, &gpio1);
	if (ret)
		return ret;
	ret = wsrsel_sample_gpio(16, &gpio16);
	if (ret)
		return ret;
	printf("WSRSEL GPIO gpio1=%d gpio16=%d\n", gpio1, gpio16);

	if (gpio1 == 1 && gpio16 == 0) {
		printf("WSRSEL WB: NAND read skipped; staying at Console\n");
		return CMD_RET_SUCCESS;
	}
	if (gpio1 == 0 && gpio16 == 1)
		return wsrsel_dispatch(WSRSEL_IMAGE1_OFFSET,
					       WSRSEL_IMAGE1_SIZE,
					       WSRSEL_EXPECT_DHP2, true);
	if (gpio1 == 1 && gpio16 == 1)
		return wsrsel_dispatch(WSRSEL_IMAGE2_OFFSET,
					       WSRSEL_IMAGE2_SIZE,
					       WSRSEL_EXPECT_RAW_FIT, true);

	printf("WSRSEL stop: unknown GPIO combination gpio1=%d gpio16=%d\n",
	       gpio1, gpio16);
	return -EPROTO;
}

U_BOOT_LONGHELP(wsrsel,
	"boot - sample GPIO1/GPIO16 and boot the fixed expected slot\n"
	"probe image1 - read/validate Image1 DHP2 and stop\n"
	"probe image2 - read/validate Image2 raw FIT and stop\n"
	"  read-only: no NAND write/erase, repair, fallback, saveenv, or sync");

U_BOOT_CMD(wsrsel, 3, 0, do_wsrsel,
	   "WSR-2533DHP2 fixed-format read-only boot selector",
	   wsrsel_help_text
);
#endif

U_BOOT_LONGHELP(nandro,
	"regs - print inherited NFI and clock-gate state\n"
	"nandro id - read the 5-byte NAND ID\n"
	"nandro page <page> [addr] - read one corrected 2 KiB page\n"
	"nandro bad <block> <count> - read first two pages/block and report markers\n"
	"nandro range <page> <count> - read corrected physical pages and CRC32\n"
	"nandro read <addr> <page> <count> - read corrected physical pages to RAM\n"
	"  only a 32-byte prefix and 32-byte FDM are printed; no write path");

U_BOOT_CMD(nandro, 5, 1, do_mt7622_nandro,
	   "MT7622 raw NAND read-only diagnostic",
	   nandro_help_text
);
#endif
