/*
 * Copyright (c) 2017 I-SENSE group of ICCS
 *
 * SPDX-License-Identifier: MIT
 */

#include <kernel.h>
#include <device.h>
#include <init.h>
#include <drivers/gpio.h>
#include <sys/sys_io.h>
#include <devicetree.h>
#include <pinmux/stm32/pinmux_stm32.h>

static int pinmux_stm32_init(struct device *port)
{
	ARG_UNUSED(port);

	struct device *p0 = device_get_binding("GPIO_0");
	struct device *p1 = device_get_binding("GPIO_1");

#if IS_DEFINED(BOARD_NRFMICRO_13)
	// enable EXT_VCC (use 0 for nRFMicro 1.3, use 1 for nRFMicro 1.1)
	gpio_pin_configure(p1, 9, GPIO_OUTPUT);
	gpio_pin_set(p1, 9, 0);

	// enable charger (nRFMicro 1.3 only)
	gpio_pin_configure(p0, 5, GPIO_OUTPUT);
	gpio_pin_set(p0, 5, 0);
#else
    // enable EXT_VCC (use 0 for nRFMicro 1.3, use 1 for nRFMicro 1.1)
	gpio_pin_configure(p1, 9, GPIO_OUTPUT);
	gpio_pin_set(p1, 9, 1);
#endif
	return 0;
}

SYS_INIT(pinmux_stm32_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);