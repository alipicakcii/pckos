// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <library/convert.hh>

namespace lib
{

	void signed_itoa(int value, char *buffer, int radix)
	{

		// Negatif sayı kontrolü
		int negatif = 0;
		if (value < 0)
		{
			negatif = 1;
			value = -value;
		}

		int index = 0;

		// Rakamları ters sırayla buffer'a ekleme
		do
		{
			int digit = value % radix;
			buffer[index++] = (digit < 10) ? digit + '0' : digit - 10 + 'A'; // Hex için 'A'-'F' eklemesi
			value /= radix;
		} while (value > 0);

		// Negatif sayı ise eksi işaretini ekleme
		if (negatif)
		{
			buffer[index++] = '-';
		}

		// Buffer'ı ters çevirme
		for (int i = 0, j = index - 1; i < j; ++i, --j)
		{
			char temp = buffer[i];
			buffer[i] = buffer[j];
			buffer[j] = temp;
		}

		// Null karakter ekleme
		buffer[index] = '\0';
	}

	void unsigned_itoa(unsigned int value, char *buffer, int radix)
	{

		// Negatif sayı kontrolü
		int negatif = 0;

		int index = 0;

		// Rakamları ters sırayla buffer'a ekleme
		do
		{
			int digit = value % radix;
			buffer[index++] = (digit < 10) ? digit + '0' : digit - 10 + 'A'; // Hex için 'A'-'F' eklemesi
			value /= radix;
		} while (value > 0);

		// Negatif sayı ise eksi işaretini ekleme
		if (negatif)
		{
			// buffer[index++] = '-';
		}

		// Buffer'ı ters çevirme
		for (int i = 0, j = index - 1; i < j; ++i, --j)
		{
			char temp = buffer[i];
			buffer[i] = buffer[j];
			buffer[j] = temp;
		}

		// Null karakter ekleme
		buffer[index] = '\0';
	}
	/**
	 * 64 bit long
	 */
	void itoa_long_long(unsigned long long val, char *buffer, int radix)
	{

		// Negatif sayı kontrolü
		int negatif = 0;

		int index = 0;

		// Rakamları ters sırayla buffer'a ekleme
		do
		{
			unsigned long long digit = val % radix;
			buffer[index++] = (digit < 10) ? digit + '0' : digit - 10 + 'A'; // Hex için 'A'-'F' eklemesi
			val /= radix;
		} while (val > 0);

		// Negatif sayı ise eksi işaretini ekleme
		if (negatif)
		{
			buffer[index++] = '-';
		}

		// Buffer'ı ters çevirme
		for (int i = 0, j = index - 1; i < j; ++i, --j)
		{
			char temp = buffer[i];
			buffer[i] = buffer[j];
			buffer[j] = temp;
		}

		// Null karakter ekleme
		buffer[index] = '\0';
	}

} // namespace lib
