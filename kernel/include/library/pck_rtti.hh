// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK

#ifndef PCKOS_LIBRARY_PCK_RTTI_HH
#define PCKOS_LIBRARY_PCK_RTTI_HH

#include <stdint.h>

// ***************************************************************
// C++ RTTI ABI Implementation
//
// C++'ta dynamic_cast işlemi, çalışma zamanında tür kontrolü
// yapmak için RTTI (Run-Time Type Information) mekanizmasını kullanır.
//
// Polimorfik sınıfların sanal tabloları (vtable), ilgili RTTI
// bilgilerine erişilmesini sağlar.
//
// type_info, sınıflara ait tür bilgilerini temsil eder.
// dynamic_cast işlemi sırasında gerekli olduğunda
// __dynamic_cast fonksiyonu çağrılır.
//
// Bu fonksiyon ve ilgili RTTI sınıfları PckOS'a port edilmiştir.
//
// Derleyici, __dynamic_cast fonksiyonuna kaynak ve hedef türlerin
// RTTI adreslerini ve ek bir offset bilgisi gönderir.
// ***************************************************************

class type_info
{
public:
    // STL içerisindeki std::type_info sınıfının temel
    // ABI yerleşimi referans alınmıştır.
    //
    // Bir sınıfa sanal fonksiyon veya sanal yıkıcı eklendiğinde,
    // nesne genellikle sanal tabloya işaret eden bir vptr içerir.
    //
    // Bu işaretçi x86 (32-bit) sistemlerde 4 bayt yer kaplar.
    // Nesnenin boyutu hizalama kurallarına bağlı olarak artabilir.
    //
    // dynamic_cast ile çalışma zamanında tür kontrolü yapılabilmesi
    // için kaynak sınıfın polimorfik olması gerekir.
    //
    // Sanal yıkıcı zorunlu değildir; herhangi bir sanal fonksiyon
    // sınıfın polimorfik olmasını sağlar.

    virtual ~type_info();

    explicit type_info(const char *__name);

    const char *name() const;
    bool equals(const type_info *other) const;

protected:
    const char *_name;
};

// ***************************************************************
// GCC / Clang Itanium C++ ABI RTTI sınıfları
//
// __class_type_info:
// Kalıtım bilgisi içermeyen sınıfların RTTI yapısı.
//
// __si_class_type_info:
// Public, sanal olmayan tekli kalıtımın RTTI yapısı.
//
// __base_type, doğrudan üst sınıfın RTTI adresini tutar.
// Bu adres üzerinden Base -> Sub -> Child zinciri izlenebilir.
// ***************************************************************

namespace __cxxabiv1
{
    class __class_type_info : public type_info
    {
    public:
        explicit __class_type_info(const char *name)
            : type_info(name) {}

        virtual ~__class_type_info();
    };

    class __si_class_type_info : public __class_type_info
    {
    public:
        const __class_type_info *__base_type;

        __si_class_type_info(
            const char *name,
            const __class_type_info *base)
            : __class_type_info(name),
              __base_type(base) {}

        virtual ~__si_class_type_info();
    };
}

#endif
