/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

SECTIONS
{
  . = ALIGN(0x1000);
  .text 0xfe000 : AT(0x1000)
  {
    *(.text)
    *(.text.*)
  }

  . = ALIGN(0x1000);
  .rodata : ALIGN(0x1000)
  {
    *(.rodata)
    *(.rodata.*)
  }

  . = ALIGN(0x1000);
  .dynamic : ALIGN(0x1000)
  {
    *(.dynamic)
  }
}
