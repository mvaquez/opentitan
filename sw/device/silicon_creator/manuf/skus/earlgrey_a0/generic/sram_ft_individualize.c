// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>

#include "sw/device/lib/arch/device.h"
#include "sw/device/lib/dif/dif_flash_ctrl.h"
#include "sw/device/lib/dif/dif_lc_ctrl.h"
#include "sw/device/lib/dif/dif_otp_ctrl.h"
#include "sw/device/lib/runtime/hart.h"
#include "sw/device/lib/runtime/log.h"
#include "sw/device/lib/runtime/print.h"
#include "sw/device/lib/testing/lc_ctrl_testutils.h"
#include "sw/device/lib/testing/otp_ctrl_testutils.h"
#include "sw/device/lib/testing/pinmux_testutils.h"
#include "sw/device/lib/testing/test_framework/check.h"
#include "sw/device/lib/testing/test_framework/ottf_console.h"
#include "sw/device/lib/testing/test_framework/ottf_test_config.h"
#include "sw/device/silicon_creator/manuf/lib/flash_info_fields.h"
#include "sw/device/silicon_creator/manuf/lib/individualize.h"
#include "sw/device/silicon_creator/manuf/lib/individualize_sw_cfg.h"
#include "sw/device/silicon_creator/manuf/lib/otp_fields.h"
#include "sw/device/silicon_creator/manuf/skus/earlgrey_a0/generic/flash_info_permissions.h"

#include "hw/top_earlgrey/sw/autogen/top_earlgrey.h"

OTTF_DEFINE_TEST_CONFIG(.enable_uart_flow_control = true);

static dif_flash_ctrl_state_t flash_ctrl_state;
static dif_lc_ctrl_t lc_ctrl;
static dif_otp_ctrl_t otp_ctrl;
static dif_pinmux_t pinmux;

/**
 * Initializes all DIF handles used in this SRAM program.
 */
static status_t peripheral_handles_init(void) {
  TRY(dif_flash_ctrl_init_state(
      &flash_ctrl_state,
      mmio_region_from_addr(TOP_EARLGREY_FLASH_CTRL_CORE_BASE_ADDR)));
  TRY(dif_lc_ctrl_init(mmio_region_from_addr(TOP_EARLGREY_LC_CTRL_BASE_ADDR),
                       &lc_ctrl));
  TRY(dif_otp_ctrl_init(
      mmio_region_from_addr(TOP_EARLGREY_OTP_CTRL_CORE_BASE_ADDR), &otp_ctrl));
  TRY(dif_pinmux_init(mmio_region_from_addr(TOP_EARLGREY_PINMUX_AON_BASE_ADDR),
                      &pinmux));
  return OK_STATUS();
}

static status_t provision(void) {
  LOG_INFO("FT SRAM provisioning start.");
  TRY(manuf_individualize_device_creator_sw_cfg(&otp_ctrl));
  TRY(manuf_individualize_device_owner_sw_cfg(&otp_ctrl));
  TRY(manuf_individualize_device_hw_cfg(&flash_ctrl_state, &otp_ctrl,
                                        kFlashInfoPage0Permissions));
  LOG_INFO("FT SRAM provisioning done.");
  return OK_STATUS();
}

bool sram_main(void) {
  CHECK_STATUS_OK(peripheral_handles_init());
  pinmux_testutils_init(&pinmux);
  ottf_console_init();

  // Check we are in in TEST_UNLOCKED1.
  CHECK_STATUS_OK(
      lc_ctrl_testutils_check_lc_state(&lc_ctrl, kDifLcCtrlStateTestUnlocked1));

  // Process provisioning commands.
  CHECK_STATUS_OK(provision());

  // Halt the CPU here to enable JTAG to perform an LC transition to mission
  // mode, as ROM execution should be active now.
  abort();

  return true;
}
