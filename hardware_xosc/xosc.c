#include "hardware/structs/xosc.h"
#include "hardware/xosc.h"
#include "pico.h"

void xosc_init() {
	xosc_hw->ctrl = XOSC_CTRL_FREQ_RANGE_VALUE_1_15MHZ;
	xosc_hw->startup = 8192 - 1;
	hw_set_bits(&xosc_hw->ctrl,
				XOSC_CTRL_ENABLE_VALUE_ENABLE << XOSC_CTRL_ENABLE_LSB);

	while(!(xosc_hw->status & XOSC_STATUS_STABLE_BITS))
		tight_loop_contents();
}