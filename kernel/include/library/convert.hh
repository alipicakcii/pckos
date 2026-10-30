// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef LIB_ARC_CONVERT_H
#define LIB_ARC_CONVERT_H
namespace lib
{
  // işaretli int değerini ASCII metnine çevirir
  void signed_itoa(int val, char *buffer, int radix);
  // işaretsiz int değerini ASCII metnine çevirir
  void unsigned_itoa(unsigned int val, char *buffer, int radix);
  // işaretsiz long long değerini ASCII metnine çevirir
  void itoa_long_long(unsigned long long val, char *buffer, int radix);

} // namespace lib

#endif
