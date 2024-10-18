/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Dolu1990 <charles.papon.90@gmail.com>
 *
 */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_encoding.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_const.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_platform.h>
#include <sbi/sbi_domain.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/fdt/fdt_fixup.h>
#include <sbi_utils/ipi/aclint_mswi.h>
#include <sbi_utils/irqchip/plic.h>
#include <sbi_utils/serial/uart8250.h>
#include <sbi_utils/timer/aclint_mtimer.h>

#ifndef ADSOC_DEFAULT_HART_COUNT
#define ADSOC_DEFAULT_HART_COUNT	2
#endif

#define ADSOC_UART0_ADDR        0x10000000
#define ADSOC_UART_FREQ         25000000    /*25 MHz*/
#define ADSOC_UART_BAUDRATE     520800

#define ADSOC_DEFAULT_PLATFORM_FEATURES	SBI_PLATFORM_HAS_MFAULTS_DELEGATION
#define ADSOC_DEFAULT_UART_ADDR                 ADSOC_UART0_ADDR
#define ADSOC_DEFAULT_PLIC_ADDR                 0x0C000000
#define ADSOC_DEFAULT_PLIC_NUM_SOURCES          39
#define ADSOC_DEFAULT_PLIC_SIZE			        0x300000
#define ADSOC_DEFAULT_CLINT_ADDR                0x02000000
#define ADSOC_DEFAULT_ACLINT_MTIMER_FREQ        12500000    /*12.5MHz */
#define ADSOC_DEFAULT_ACLINT_MSWI_ADDR	\
		(ADSOC_DEFAULT_CLINT_ADDR + CLINT_MSWI_OFFSET)
#define ADSOC_DEFAULT_ACLINT_MTIMER_ADDR	\
		(ADSOC_DEFAULT_CLINT_ADDR + CLINT_MTIMER_OFFSET)
#define ADSOC_DEFAULT_HART_STACK_SIZE           8192

/* clang-format on */

static struct plic_data plic = {
	.addr = ADSOC_DEFAULT_PLIC_ADDR,
	.num_src = ADSOC_DEFAULT_PLIC_NUM_SOURCES,
	.size = ADSOC_DEFAULT_PLIC_SIZE,
};

static struct aclint_mswi_data mswi = {
	.addr = ADSOC_DEFAULT_ACLINT_MSWI_ADDR,
	.size = ACLINT_MSWI_SIZE,
	.first_hartid = 0,
	.hart_count = ADSOC_DEFAULT_HART_COUNT,
};

static struct aclint_mtimer_data mtimer = {
	.mtime_freq = ADSOC_DEFAULT_ACLINT_MTIMER_FREQ,
	.mtime_addr = ADSOC_DEFAULT_ACLINT_MTIMER_ADDR +
			  ACLINT_DEFAULT_MTIME_OFFSET,
	.mtime_size = ACLINT_DEFAULT_MTIME_SIZE,
	.mtimecmp_addr = ADSOC_DEFAULT_ACLINT_MTIMER_ADDR +
			  ACLINT_DEFAULT_MTIMECMP_OFFSET,
	.mtimecmp_size = ACLINT_DEFAULT_MTIMECMP_SIZE,
	.first_hartid = 0,
	.hart_count = ADSOC_DEFAULT_HART_COUNT,
	.has_64bit_mmio = true,
};

#define CSR_MNSTATUS 0x744

static int adsoc_nascent_init(void)
{
	csr_write(CSR_MNSTATUS, 0x8);

	return 0;
}

/*
 * ADSoC platform early initialization.
 */
static int adsoc_early_init(bool cold_boot)
{
	if (!cold_boot)
		return 0;

	return uart8250_init(ADSOC_DEFAULT_UART_ADDR, ADSOC_UART_FREQ,
                 ADSOC_UART_BAUDRATE, 2, 4, 0);
}

/*
 * ADSoC platform final initialization.
 */
static int adsoc_final_init(bool cold_boot)
{
	void *fdt;

	if (!cold_boot)
		return 0;


	fdt = (void *)fdt_get_address();
	fdt_fixups(fdt);

	return 0;
}

static int adsoc_domains_init(void) {
	int rc;
	struct sbi_domain_memregion reg, *preg;

	sbi_domain_for_each_memregion(&root, preg) {
		if((preg->base == 0) && (preg->order == __riscv_xlen)) {
			sbi_domain_memregion_init(0x80000000UL, 0xFFFFFFFFUL-0x80000000UL, (SBI_DOMAIN_MEMREGION_SU_RWX), preg);
			break;
		}
	}

	sbi_domain_memregion_init(0x0000000100000000ULL, 0xFFFFFFFFFFFFFFFFULL-0x0000000100000000ULL, (SBI_DOMAIN_MEMREGION_ENF_PERMISSIONS), &reg);
	rc = sbi_domain_root_add_memregion(&reg);
	if (rc)
		return rc;

	return 0;
}

/*
 * Initialize the ADSoC interrupt controller for current HART.
 */
static int adsoc_irqchip_init(bool cold_boot)
{
	int rc;
	u32 hartid = current_hartid();

	if (cold_boot) {
		rc = plic_cold_irqchip_init(&plic);
		if (rc)
			return rc;
	}

	return plic_warm_irqchip_init(&plic, hartid * 4, hartid * 4 + 1);

}

/*
 * Initialize IPI for current HART.
 */
static int adsoc_ipi_init(bool cold_boot)
{
	int rc;

	if (cold_boot) {
		rc = aclint_mswi_cold_init(&mswi);
		if (rc)
			return rc;
	}

	return aclint_mswi_warm_init();
}

/*
 * Initialize ADSoC timer for current HART.
 */
static int adsoc_timer_init(bool cold_boot)
{
	int rc;
	if (cold_boot) {
		rc = aclint_mtimer_cold_init(&mtimer, NULL); /* Timer has no reference */
		if (rc)
			return rc;
	}

	return aclint_mtimer_warm_init();
}

/*
 * Platform descriptor.
 */
const struct sbi_platform_operations platform_ops = {
	.nascent_init = adsoc_nascent_init,
	.early_init = adsoc_early_init,
	.final_init = adsoc_final_init,
	.domains_init = adsoc_domains_init,
	.irqchip_init = adsoc_irqchip_init,
	.ipi_init = adsoc_ipi_init,
	.timer_init = adsoc_timer_init
};

const struct sbi_platform platform = {
	.opensbi_version = OPENSBI_VERSION,
	.platform_version = SBI_PLATFORM_VERSION(0x0, 0x01),
	.name = "DENSO / ADSoC",
	.features = ADSOC_DEFAULT_PLATFORM_FEATURES,
	.hart_count = ADSOC_DEFAULT_HART_COUNT,
	.hart_stack_size = ADSOC_DEFAULT_HART_STACK_SIZE,
	.heap_size =
		SBI_PLATFORM_DEFAULT_HEAP_SIZE(ADSOC_DEFAULT_HART_COUNT),
	.platform_ops_addr = (unsigned long)&platform_ops
};
