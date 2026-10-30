// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <library/pck_rtti.hh>
#include <stdint.h>

// ***************************************************************
// C++ RTTI ABI - Temel type_info implementasyonu
// ***************************************************************

type_info::type_info(const char *__name)
    : _name(__name)
{
}

type_info::~type_info()
{
}

const char *type_info::name() const
{
    return _name;
}

bool type_info::equals(const type_info *other) const
{
    return this == other;
}

// ***************************************************************
// RTTI vtable sembollerinin oluşturulması
//
// Önceden aşağıdaki semboller boş fonksiyonlarla tanımlanıyordu:
//
// _ZTVN10__cxxabiv117__class_type_infoE
// _ZTVN10__cxxabiv120__si_class_type_infoE
//
// Bu semboller aslında fonksiyon değil, sanal tablo sembolleridir.
//
// Sanal yıkıcıların sınıf dışında tanımlanması sayesinde GCC,
// ilgili gerçek vtable yapılarını oluşturabilir.
//
// Bu nedenle eski extern "C" void _ZTV...() tanımları
// artık kullanılmamalıdır.
// ***************************************************************

namespace __cxxabiv1
{
    __class_type_info::~__class_type_info()
    {
    }

    __si_class_type_info::~__si_class_type_info()
    {
    }
}

using namespace __cxxabiv1;

// GCC tarafından oluşturulan gerçek vtable sembolleri.
extern "C"
{
    extern const uintptr_t _ZTVN10__cxxabiv117__class_type_infoE[];
    extern const uintptr_t _ZTVN10__cxxabiv120__si_class_type_infoE[];
}

// ***************************************************************
// RTTI türünün __si_class_type_info olup olmadığını kontrol eder.
//
// Itanium ABI düzeninde vtable başlangıcında:
//
// vtable[0] = offset-to-top
// vtable[1] = RTTI adresi
// vtable[2] = İlk sanal fonksiyon adresi
//
// Nesnenin vptr değeri normal durumda vtable[2] adresini gösterir.
//
// Böylece RTTI nesnesinin vptr adresi karşılaştırılarak
// tekli kalıtım bilgisi içerip içermediği belirlenebilir.
// ***************************************************************

static bool is_single_inheritance_type(const type_info *info)
{
    if (info == nullptr)
        return false;

    uintptr_t vptr =
        *reinterpret_cast<const uintptr_t *>(info);

    uintptr_t expected =
        reinterpret_cast<uintptr_t>(
            &_ZTVN10__cxxabiv120__si_class_type_infoE[2]
        );

    return vptr == expected;
}

// ***************************************************************
// Tekli kalıtım zincirini kontrol eder.
//
// Örnek:
//
// class Base {};
// class Sub : public Base {};
// class Child : public Sub {};
//
// Child nesnesinin RTTI zinciri:
//
// Child -> Sub -> Base
//
// current, nesnenin dinamik türünü temsil eder.
// target, dönüşüm yapılmak istenen hedef türdür.
//
// Her adımda mevcut tür ile hedef tür karşılaştırılır.
// Eşleşme bulunamazsa doğrudan üst sınıfa geçilir.
//
// Zincir sona erdiğinde hedef tür bulunamamışsa false döner.
// ***************************************************************

static bool inherits_from(
    const type_info *current,
    const type_info *target)
{
    while (current != nullptr)
    {
        if (current == target)
            return true;

        // RTTI tekli kalıtım bilgisi içermiyorsa
        // daha fazla üst sınıf bulunamaz.
        if (!is_single_inheritance_type(current))
            return false;

        const __si_class_type_info *si =
            reinterpret_cast<const __si_class_type_info *>(current);

        // Doğrudan üst sınıfın RTTI adresine geç.
        current = si->__base_type;
    }

    return false;
}

// ***************************************************************
// C++ RTTI ABI - __dynamic_cast Implementation
//
// Bu fonksiyon, C++ RTTI ve ABI desteğinin sağlanması için
// gerçekleştirilen önemli bir port işlemidir.
//
// Derleyici, çalışma zamanında tür kontrolü gerektiren
// dynamic_cast işlemleri için __dynamic_cast fonksiyonunu
// çağırabilir.
//
// C++ dilinde polimorfik sınıflar arasında güvenli tür dönüşümü
// gerçekleştirmek için dynamic_cast kullanılır.
//
// Dönüşüm başarısız olduğunda işaretçi türleri için nullptr
// döndürülür. Referans dönüşümlerinde ise std::bad_cast
// istisnası oluşturulması gerekir.
//
// Bu implementasyon yalnızca işaretçi dönüşümlerinin
// belirli bir bölümünü desteklemektedir.
//
// ---------------------------------------------------------------
// PARAMETRELER
// ---------------------------------------------------------------
//
// obj:
// Tür dönüşümü yapılacak kaynak alt nesnenin adresidir.
//
// from:
// Kaynak sınıfın RTTI (type_info) adresidir.
//
// to:
// Hedef sınıfın RTTI (type_info) adresidir.
//
// src2dst_offset:
// Derleyicinin kaynak ve hedef sınıflar arasındaki ilişki
// hakkında sağladığı ABI offset bilgisidir.
//
// Parametreler derleyici tarafından otomatik olarak sağlanır.
//
// ---------------------------------------------------------------
// ÇALIŞMA PRENSİBİ
// ---------------------------------------------------------------
//
// 1. Öncelikle nesne adresinin geçerli olup olmadığı kontrol edilir.
//
// 2. Nesnenin başlangıcında bulunduğu varsayılan vptr
//    üzerinden sanal tablo (vtable) adresine erişilir.
//
// 3. Itanium ABI düzeninde vtable adres noktasından bir
//    işaretçi boyutu geriye gidilerek RTTI adresi okunur.
//
//    Bu mesafe 32-bit x86 sistemlerde 4 bayttır.
//
// 4. Elde edilen RTTI adresi, nesnenin dinamik türünü belirtir.
//
// 5. Kaynak türün, dinamik türün kalıtım zincirinde bulunup
//    bulunmadığı kontrol edilir.
//
// 6. Hedef türün aynı kalıtım zincirinde bulunup bulunmadığı
//    kontrol edilir.
//
// 7. Her iki tür de uygun zincirde bulunuyorsa dönüşüm
//    başarılı kabul edilir ve nesne adresi döndürülür.
//
// 8. Tür kontrolü başarısız olursa nullptr döndürülür.
//
// ---------------------------------------------------------------
// DESTEKLENEN KALITIM YAPILARI
// ---------------------------------------------------------------
//
// Base -> Sub
// Base -> Sub -> Child
// Base -> Sub -> Child -> Derived
//
// Yalnızca public, sanal olmayan tekli kalıtım desteklenir.
//
// ---------------------------------------------------------------
// ÖNEMLİ SINIRLAMALAR
// ---------------------------------------------------------------
//
// Bu implementasyon henüz tam RTTI desteği sağlamamaktadır.
//
// Çoklu kalıtım, sanal kalıtım, private/protected kalıtım
// ve genel pointer adjustment işlemleri desteklenmemektedir.
//
// Ayrıca kaynak alt nesnenin tam nesneye göre offset düzeltmesi
// yapılmamaktadır.
//
// Bu nedenle yalnızca sıfır offsetli tekli kalıtım
// düzeninde kullanılması amaçlanmıştır.
//
// ---------------------------------------------------------------
// ÖRNEK KULLANIM
// ---------------------------------------------------------------
//
// class Base
// {
// public:
//     virtual ~Base() {}
// };
//
// class Sub : public Base {};
// class Child : public Sub {};
//
// Base *base = new Child;
//
// Child *child = dynamic_cast<Child *>(base);
//
// if (child != nullptr)
// {
//     // Dönüşüm başarılı.
// }
//
// ***************************************************************

extern "C" void *__dynamic_cast(
    const void *obj,
    const __class_type_info *from,
    const __class_type_info *to,
    __PTRDIFF_TYPE__ src2dst_offset)
{
    (void)src2dst_offset;

    if (obj == nullptr || from == nullptr || to == nullptr)
    {
        return nullptr;
    }

    // Nesnenin sanal tablosunu al.
    const uintptr_t *vtable =
        *reinterpret_cast<const uintptr_t *const *>(obj);

    // Sanal tablodan dinamik RTTI adresini oku.
    const __class_type_info *dynamic_type =
        reinterpret_cast<const __class_type_info *>(vtable[-1]);

    // Kaynak tür, dinamik türün kalıtım zincirinde mi?
    if (!inherits_from(dynamic_type, from))
    {
        return nullptr;
    }

    // Hedef tür, dinamik türün kalıtım zincirinde mi?
    if (!inherits_from(dynamic_type, to))
    {
        return nullptr;
    }

    // Tekli, sanal olmayan kalıtımda nesne adresi değişmez.
    return const_cast<void *>(obj);
}
