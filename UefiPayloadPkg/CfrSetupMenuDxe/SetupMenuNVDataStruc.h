/** @file
  A Setup Menu for configuring boot options defined by bootloader CFR.

  Copyright (c) 2023, 9elements GmbH.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _SETUPMENUNVDATASTRUC_H_
#define _SETUPMENUNVDATASTRUC_H_

#define SETUP_MENU_FORMSET_GUID \
  { \
    0x93E6FCD9, 0x8E17, 0x43DF, { 0xB7, 0xF0, 0x91, 0x3E, 0x58, 0xB1, 0xA7, 0x89 } \
  }

#define FORM_MAIN_ID             0x0001
#define SETUP_SUBMENU01_FORM_ID  0x0002
#define SETUP_SUBMENU02_FORM_ID  0x0003
#define SETUP_SUBMENU03_FORM_ID  0x0004
#define SETUP_SUBMENU04_FORM_ID  0x0005
#define SETUP_SUBMENU05_FORM_ID  0x0006
#define SETUP_SUBMENU06_FORM_ID  0x0007
#define SETUP_SUBMENU07_FORM_ID  0x0008
#define SETUP_SUBMENU08_FORM_ID  0x0009
#define SETUP_SUBMENU09_FORM_ID  0x000a
#define SETUP_SUBMENU10_FORM_ID  0x000b
#define SETUP_SUBMENU11_FORM_ID  0x000c
#define SETUP_SUBMENU12_FORM_ID  0x000d
#define SETUP_SUBMENU13_FORM_ID  0x000e
#define SETUP_SUBMENU14_FORM_ID  0x000f
#define SETUP_SUBMENU15_FORM_ID  0x0010

#define CFR_COMPONENT_START  0x1000
#define LABEL_END            0xffff

#endif
