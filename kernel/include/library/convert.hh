// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef LIB_ARC_CONVERT_H
#define LIB_ARC_CONVERT_H
namespace lib
{
  // signed int to ascii
  void signed_itoa(int val, char *buffer, int radix);
  // unsigned int to ascii
  void unsigned_itoa(unsigned int val, char *buffer, int radix);
  // unsingned long long to ascii
  void itoa_long_long(unsigned long long val, char *buffer, int radix);

} // namespace lib

#endif
