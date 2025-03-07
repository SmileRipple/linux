/*
 * Copyright 2016 IBM Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/gpio/machine.h>
#include <dt-bindings/gpio/aspeed-gpio.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

typedef void (init_fnc_t) (void);
extern void __init ast_add_device_pwm_fan(void);
init_fnc_t __initdata *init_all_device[] = {
#ifdef CONFIG_PWM_AST_ASPEED
	ast_add_device_pwm_fan,
#endif
	NULL,
};

static void __init ast_add_all_devices(void)
{
	init_fnc_t **init_fnc_ptr;

	for (init_fnc_ptr = init_all_device; *init_fnc_ptr; ++init_fnc_ptr) {
		(*init_fnc_ptr)();
	}

	return;
}

static void __init aspeed_dt_init(void)
{
	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);
	ast_add_all_devices();
}


#define AST_IO_VA	0xf0000000
#define AST_IO_PA	0x1e600000
#define AST_IO_SZ	0x00200000

#define AST_IO(__pa)	((void __iomem *)(((__pa) & 0x001fffff) | AST_IO_VA))

#define AST_BASE_LPC		0x1E789000 /* LPC Controller */
#define AST_BASE_SPI		0x1E630000 /* SPI Memory Controller */
#define AST_BASE_MAC0		0X1E660000 /* MAC 1 */
#define AST_BASE_SCU		0x1E6E2000 /* System Control Unit (SCU) */
#define AST_BASE_GPIO		0x1E780000 /* GPIO Controller */

static struct map_desc aspeed_io_desc[] __initdata __maybe_unused = {
	{
		.virtual	=  AST_IO_VA,
		.pfn		= __phys_to_pfn(AST_IO_PA),
		.length		= AST_IO_SZ,
		.type		= MT_DEVICE
	},
};

static void __init do_common_setup(void)
{
	/* Enable LPC FWH cycles, Enable LPC to AHB bridge */
	writel(0x00000500, AST_IO(AST_BASE_LPC | 0x80));

	/* Set UART routing */
	writel(0x00000000, AST_IO(AST_BASE_LPC | 0x9c));

	/* Setup scratch registers */
	writel(0x00000042, AST_IO(AST_BASE_LPC | 0x170));
	writel(0x00008000, AST_IO(AST_BASE_LPC | 0x174));
}

static void __init do_barreleye_setup(void)
{
	u32 reg;

	do_common_setup();

	/* Setup PNOR address mapping for 64M flash */
	writel(0x30000C00, AST_IO(AST_BASE_LPC | 0x88));
	writel(0xFC0003FF, AST_IO(AST_BASE_LPC | 0x8C));

	/* GPIO setup */
	writel(0x9E82FCE7, AST_IO(AST_BASE_GPIO | 0x00));
	writel(0x0370E677, AST_IO(AST_BASE_GPIO | 0x04));

	/*
	 * Do read/modify/write on power gpio to prevent resetting power on
	 * reboot
	 */
	reg = readl(AST_IO(AST_BASE_GPIO | 0x20));
	reg |= 0xCFC8F7FD;
	writel(reg, AST_IO(AST_BASE_GPIO | 0x20));
	writel(0xC738F20A, AST_IO(AST_BASE_GPIO | 0x24));
	writel(0x0031FFAF, AST_IO(AST_BASE_GPIO | 0x80));

	/* Select TIMER3 as debounce timer */
	writel(0x00000001, AST_IO(AST_BASE_GPIO | 0x48));
	writel(0x00000001, AST_IO(AST_BASE_GPIO | 0x4C));

	/* Set debounce timer to 480000 cycles, with a pclk of 48MHz,
	 * corresponds to 20 ms. This time was found by experimentation */
	writel(0x000EA600, AST_IO(AST_BASE_GPIO | 0x58));
}

static void __init do_palmetto_setup(void)
{
	do_common_setup();

	/* Setup PNOR address mapping for 32M flash */
	writel(0x30000E00, AST_IO(AST_BASE_LPC | 0x88));
	writel(0xFE0001FF, AST_IO(AST_BASE_LPC | 0x8C));
}

static void __init do_firestone_setup(void)
{
	do_common_setup();

	/* Setup PNOR address mapping for 64M flash */
	writel(0x30000C00, AST_IO(AST_BASE_LPC | 0x88));
	writel(0xFC0003FF, AST_IO(AST_BASE_LPC | 0x8C));

	/* Override serial destination to use the dedicated serial port */
	writel(0x00004000, AST_IO(AST_BASE_LPC | 0x174));

	/* Workaround to make hostboot run into default SUART init */
	/* Otherwise it fails to boot */
	writel(0x00000000, AST_IO(AST_BASE_LPC | 0x170));
}

static void __init do_garrison_setup(void)
{
	do_common_setup();

	/* Setup PNOR address mapping for 64M flash */
	writel(0x30000C00, AST_IO(AST_BASE_LPC | 0x88));
	writel(0xFC0003FF, AST_IO(AST_BASE_LPC | 0x8C));
}

static void __init do_ast2500evb_setup(void)
{
	/* Set strap to RGMII for dedicated PHY networking */
	writel(BIT(6) | BIT(7), AST_IO(AST_BASE_SCU | 0x70));
}

static void __init do_zaius_setup(void)
{
	unsigned long reg;
	unsigned long board_rev;
	/* D3 in GPIOA/B/C/D direction and data registers */
	unsigned long phy_reset_mask = BIT(27);

	do_common_setup();

	/* Read BOARD_REV[4:0] fuses from GPIOM[7:3] */
	reg = readl(AST_IO(AST_BASE_GPIO | 0x78));
	board_rev = (reg >> 3) & 0x1F;
	printk(KERN_INFO "Zaius platform board revision: 0x%02lx\n", board_rev);

	/* EVT boards have different FSI pin mappings */
	if (board_rev < 0x08) {
		static struct gpiod_lookup_table fsi_evt_gpio_lookup = {
			.dev_id = "gpio-fsi",
			.table = {
				GPIO_LOOKUP("1e780000.gpio", ASPEED_GPIO(C, 3),
					    "clock", GPIO_ACTIVE_HIGH),
				GPIO_LOOKUP("1e780000.gpio", ASPEED_GPIO(C, 2),
					    "data", GPIO_ACTIVE_HIGH),
				{ },
			},
		};
		gpiod_add_lookup_table(&fsi_evt_gpio_lookup);
	} else {
		static struct gpiod_lookup_table fsi_gpio_lookup = {
			.dev_id = "gpio-fsi",
			.table = {
				GPIO_LOOKUP("1e780000.gpio", ASPEED_GPIO(G, 0),
					    "clock", GPIO_ACTIVE_HIGH),
				GPIO_LOOKUP("1e780000.gpio", ASPEED_GPIO(G, 1),
					    "data", GPIO_ACTIVE_HIGH),
				{ },
			},
		};
		gpiod_add_lookup_table(&fsi_gpio_lookup);
	}

	/* Set GPIOL5 for BMC Ready*/
	reg = readl(AST_IO(AST_BASE_GPIO | 0x70));
	reg &= ~(BIT(29));
	writel(reg  , AST_IO(AST_BASE_GPIO | 0x70));
	reg = readl(AST_IO(AST_BASE_GPIO | 0x74));
	writel(reg | BIT(29) , AST_IO(AST_BASE_GPIO | 0x74));
        /* Enable USB power*/
        reg = readl(AST_IO(AST_BASE_GPIO | 0x7c));
        writel(reg | BIT(14) , AST_IO(AST_BASE_GPIO | 0x7c));

        reg = readl(AST_IO(AST_BASE_GPIO | 0x78));
        writel(reg | BIT(14) , AST_IO(AST_BASE_GPIO | 0x78));



	/* EVT1 hacks */
	if (board_rev == 0) {
		/* Disable GPIO I, G/AB pulldowns due to weak driving buffers */
		reg = readl(AST_IO(AST_BASE_SCU | 0x8C));
		writel(reg | BIT(24) | BIT(22), AST_IO(AST_BASE_SCU | 0x8C));
	}

	/* Assert MAC2 PHY hardware reset */
	/* Set pin low */
	reg = readl(AST_IO(AST_BASE_GPIO | 0x00));
	writel(reg & ~phy_reset_mask, AST_IO(AST_BASE_GPIO | 0x00));
	/* Enable pin for output */
	reg = readl(AST_IO(AST_BASE_GPIO | 0x04));
	writel(reg | phy_reset_mask, AST_IO(AST_BASE_GPIO | 0x04));
	udelay(3);
	/* Set pin high */
	reg = readl(AST_IO(AST_BASE_GPIO | 0x00));
	writel(reg | phy_reset_mask, AST_IO(AST_BASE_GPIO | 0x00));
	
	/* Disable default behavior of UART1 being held in reset by LPCRST#.
	 * By releasing UART1 from being controlled by LPC reset, it becomes
	 * immediately available regardless of the host being up.
	 */
	reg = readl(AST_IO(AST_BASE_LPC | 0x98));
	/* Clear "Enable UART1 reset source from LPC" */
	writel(reg & ~BIT(4), AST_IO(AST_BASE_LPC | 0x98));

        /* USB HOST port 1 */
        reg = readl(AST_IO(AST_BASE_SCU | 0x90));
        writel(reg | BIT(29), AST_IO(AST_BASE_SCU | 0x90));


        reg = readl(AST_IO(AST_BASE_SCU | 0x04));
        writel(reg | BIT(14), AST_IO(AST_BASE_SCU | 0x04));


        reg = readl(AST_IO(AST_BASE_SCU | 0x0C));
        writel(reg | BIT(14), AST_IO(AST_BASE_SCU | 0x0C));
	mdelay(10);

        reg = readl(AST_IO(AST_BASE_SCU | 0x04));
        writel(reg & ~BIT(14), AST_IO(AST_BASE_SCU | 0x04));

        /* USB HOST port 1 */
        reg = readl(AST_IO(AST_BASE_SCU | 0x94));
        writel(reg & ~BIT(13), AST_IO(AST_BASE_SCU | 0x94));

        reg = readl(AST_IO(AST_BASE_SCU | 0x94));
        writel(reg | BIT(14), AST_IO(AST_BASE_SCU | 0x94));

        reg = readl(AST_IO(AST_BASE_SCU | 0x04));
        writel(reg | BIT(3), AST_IO(AST_BASE_SCU | 0x04));


        reg = readl(AST_IO(AST_BASE_SCU | 0x0C));
        writel(reg & ~BIT(7), AST_IO(AST_BASE_SCU | 0x0C));
        mdelay(10);

        reg = readl(AST_IO(AST_BASE_SCU | 0x04));
        writel(reg & ~BIT(3), AST_IO(AST_BASE_SCU | 0x04));

        /* USB HOST port 1 */
        reg = readl(AST_IO(AST_BASE_SCU | 0x04));
        writel(reg | BIT(15), AST_IO(AST_BASE_SCU | 0x04));


        reg = readl(AST_IO(AST_BASE_SCU | 0x0C));
        writel(reg & ~BIT(9), AST_IO(AST_BASE_SCU | 0x0C));
        mdelay(10);

        reg = readl(AST_IO(AST_BASE_SCU | 0x04));
        writel(reg & ~BIT(15), AST_IO(AST_BASE_SCU | 0x04));
}

static void __init do_witherspoon_setup(void)
{
	unsigned long reg;

	do_common_setup();

	/* Reset tolerance for BMC_POWER_UP (GPIOD1) */
	reg = readl(AST_IO(AST_BASE_GPIO | 0x01c));
	writel(reg | BIT(25), AST_IO(AST_BASE_GPIO | 0x01c));

	/* Reset tolerance for SOFTWARE_PGOOD (GPIOR1) */
	reg = readl(AST_IO(AST_BASE_GPIO | 0x12c));
	writel(reg | BIT(9), AST_IO(AST_BASE_GPIO | 0x12c));
}

static void __init do_romulus_setup(void)
{
	unsigned long reg;

	do_common_setup();

	/* Reset tolerance for BMC_POWER_UP (GPIOD1) */
	reg = readl(AST_IO(AST_BASE_GPIO | 0x01c));
	writel(reg | BIT(25), AST_IO(AST_BASE_GPIO | 0x01c));

	/* Reset tolerance for SOFTWARE_PGOOD (GPIOR1) and SEQ_CONT (GPIOS7) */
	reg = readl(AST_IO(AST_BASE_GPIO | 0x12c));
	writel(reg | BIT(9) | BIT(23), AST_IO(AST_BASE_GPIO | 0x12c));
}

static void __init do_lanyang_setup(void)
{
	do_common_setup();
}

static void __init do_mellanox_setup(void)
{
	unsigned long reg;

	do_common_setup();

	/* Set strapping for MAC1 to RMII/NCSI and MAC2 to RGMII. */
	reg = readl(AST_IO(AST_BASE_SCU | 0x70));
	reg |= BIT(7);
	reg &= ~BIT(6);
	writel(reg, AST_IO(AST_BASE_SCU | 0x70));

	/* Enable RMII1 50MHz RCLK output. */
	reg = readl(AST_IO(AST_BASE_SCU | 0x48));
	reg |= BIT(29);
	writel(reg, AST_IO(AST_BASE_SCU | 0x48));
}


#define SCU_PASSWORD	0x1688A8A8

static void __init aspeed_init_early(void)
{
	u32 reg;

	/* Unlock SCU */
	writel(SCU_PASSWORD, AST_IO(AST_BASE_SCU));

	/* Reset AHB bridges. Do not modify the JTAG configuration bit */
	reg = readl(AST_IO(AST_BASE_SCU | 0x04)) & BIT(22);
	writel(reg | 0x02, AST_IO(AST_BASE_SCU | 0x04));

	/* Enables all the clocks except D2CLK, USB1.1 Host, USB1.1, LHCLK */
	writel(0x10CC5E80, AST_IO(AST_BASE_SCU | 0x0c));

	/*
	 * Ensure all IPs except GPIO and LPC are reset on watchdog expiry
	 */
	writel(0x001fdff3, AST_IO(AST_BASE_SCU | 0x9c));

	/* Disable default behavior of UART1 being held in reset by LPCRST#.
	 * By releasing UART1 from being controlled by LPC reset, it becomes
	 * immediately available regardless of the host being up.
	 */
	reg = readl(AST_IO(AST_BASE_LPC | 0x98));
	/* Clear "Enable UART reset source from LPC" for all four UARTs */
	writel(reg & ~GENMASK(7, 4), AST_IO(AST_BASE_LPC | 0x98));

	/*
	 * Temporary setup of AST registers until pinmux driver is complete
	 */
	if (of_machine_is_compatible("rackspace,barreleye-bmc"))
		do_barreleye_setup();
	if (of_machine_is_compatible("tyan,palmetto-bmc"))
		do_palmetto_setup();
	if (of_machine_is_compatible("ibm,firestone-bmc"))
		do_firestone_setup();
	if (of_machine_is_compatible("ibm,garrison-bmc"))
		do_garrison_setup();
	if (of_machine_is_compatible("aspeed,ast2500-evb"))
		do_ast2500evb_setup();
	if (of_machine_is_compatible("ingrasys,zaius-bmc"))
		do_zaius_setup();
	if (of_machine_is_compatible("ibm,witherspoon-bmc"))
		do_witherspoon_setup();
	if (of_machine_is_compatible("ibm,romulus-bmc"))
		do_romulus_setup();
	if (of_machine_is_compatible("inventec,lanyang-bmc"))
		do_lanyang_setup();
	if (of_machine_is_compatible("mellanox,msn-bmc"))
		do_mellanox_setup();
}

static void __init aspeed_map_io(void)
{
	iotable_init(aspeed_io_desc, ARRAY_SIZE(aspeed_io_desc));
	debug_ll_io_init();

	printk("SOC Rev: %08x\n", readl(AST_IO(AST_BASE_SCU | 0x7c)));
}

static const char *const aspeed_dt_match[] __initconst = {
		"aspeed,ast2400",
		"aspeed,ast2500",
		NULL,
};

DT_MACHINE_START(aspeed_dt, "ASpeed SoC")
	.init_early	= aspeed_init_early,
	.dt_compat	= aspeed_dt_match,
        .init_machine	= aspeed_dt_init,
	.map_io		= aspeed_map_io,
MACHINE_END
