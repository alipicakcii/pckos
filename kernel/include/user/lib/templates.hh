// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_TEMPLATES_GLOBAL_H
#define PCKOS_TEMPLATES_GLOBAL_H

#include <stddef.h>
#include <stdint.h>



// C++ referans çöküşü örnekleri
// A&  &  olur  A&
// A&  && olur  A&
// A&& &  olur  A&
// A&& && olur  A&&

// enable_if şablon sınıfı
// eğer Condition true ise T tipini dönecekdir
// eğer Condition false ise void tipini dönecekdir
template <bool Condition, typename T = void>
struct enable_if
{
};
template <typename T>
struct enable_if<true, T>
{
    using type = T;
};
template <bool Condition, typename T = void>
using enable_if_t = typename enable_if<Condition, T>::type;

// is_same şablon sınıfı iki tipin aynı olup olmadığını kontrol eder
// eğer aynı ise True döner değilse False döner
template <typename T, typename U>
struct is_same
{
    static constexpr bool value = false;
};
template <typename T>
struct is_same<T, T>
{
    static constexpr bool value = true;
};

// has_default_constructor şablon sınıfı
// bir tipin varsayılan yapıcıya sahip olup olmadığını kontrol eder
// eğer varsa True döner yoksa False döner
template <typename T>
struct has_default_constructor
{
    template <typename U>
    static char test(decltype(U()) *);

    template <typename U>
    static int test(...);

    static constexpr bool value = sizeof(test<T>(nullptr)) == sizeof(char);
};

// is_pointer şablon sınıfı
// bir tipin gösterici olup olmadığını kontrol eder
// eğer gösterici ise True döner değilse False döner
template <typename T>
struct is_pointer
{
    static constexpr bool value = false;
};
template <typename T>
struct is_pointer<T *>
{
    static constexpr bool value = true;
};

template <typename T>
struct is_pointer<const T *>
{
    static constexpr bool value = true;
};


// conditional şablon sınıfı
// eğer Condition true ise true_type tipini dönecekdir
// eğer Condition false ise false_type tipini dönecekdir
template <bool Condition, typename true_type, typename false_type>
struct conditional
{
    using type = true_type;
};
template <typename true_type, typename false_type>
struct conditional<false, true_type, false_type>
{
    using type = false_type;
};

// remove_reference şablon sınıfı
// bir tipin referans olup olmadığını kontrol eder
// eğer referans ise referansı kaldırır ve tipi döner
// eğer referans değilse tipi döner
template <typename _Tp>
struct remove_reference
{
    using type = _Tp;
};
template <typename _Tp>
struct remove_reference<_Tp &>
{
    using type = _Tp;
};
template <typename _Tp>
struct remove_reference<_Tp &&>
{
    using type = _Tp;
};
template <typename _Tp>
using remove_reference_t = typename remove_reference<_Tp>::type;

// move şablon fonksiyonu
// bir nesneyi taşımak için kullanılır
// taşınan nesne rvalue referans olarak döner
// bu fonksiyon nesnenin taşınmasını sağlar
// taşınan nesne artık geçersizdir
// bu nedenle taşınan nesneye erişim sağlanmamalıdır
// taşınan nesneye erişim sağlanırsa undefined behavior oluşur
template <typename _Tp>
constexpr typename remove_reference<_Tp>::type &&move(_Tp &&__t)
{
    return static_cast<typename remove_reference<_Tp>::type &&>(__t);
}

// forward şablon fonksiyonları mükemmel aktarım
template <typename _Tp>
constexpr _Tp &&forward(typename remove_reference<_Tp>::type &__t)
{ 
    return static_cast<_Tp &&>(__t);
}
template <typename _Tp>
constexpr _Tp &&forward(typename remove_reference<_Tp>::type &&__t)
{

    return static_cast<_Tp &&>(__t);
}

// first_type: Variadic template parametrelerinden ilkini alır
template <typename First, typename... Rest>
struct first_type
{
    using type = First;
};

// bir tipin referans olup olmadığını kontrol eder
// eğer referans ise True döner değilse False döner
template <typename T>
struct is_reference
{
    static constexpr bool value = false;
};
template <typename T>
struct is_reference<T &>
{
    static constexpr bool value = true;
};
template <typename T>
struct is_reference<T &&>
{
    static constexpr bool value = true;
};

// is_lvalue_reference şablon sınıfı
// bir tipin lvalue referans olup olmadığını kontrol eder
// eğer lvalue referans ise True döner değilse False döner
template <typename T>
struct is_lvalue_reference
{
    static constexpr bool value = false;
};
template <typename T>
struct is_lvalue_reference<T &>
{
    static constexpr bool value = true;
};

// is_rvalue_reference şablon sınıfı
// bir tipin rvalue referans olup olmadığını kontrol eder
// eğer rvalue referans ise True döner değilse False döner
template <typename T>
struct is_rvalue_reference
{
    static constexpr bool value = false;
};
template <typename T>
struct is_rvalue_reference<T &&>
{
    static constexpr bool value = true;
};

// remove_const şablon sınıfı
// bir tipin const olup olmadığını kontrol eder
// eğer const ise const'ı kaldırır ve tipi döner
// eğer const değilse tipi döner
template <typename T>
struct remove_const
{
    using type = T;
};
template <typename T>
struct remove_const<const T>
{
    using type = T;
};
template <typename T>
struct remove_const<const T &>
{
    using type = T;
};
template <typename T>
struct remove_const<const T &&>
{
    using type = T;
};

// Türevlilik kontrolü için SFINAE tasarımı:
template <typename Base, typename Derived>
struct is_base_of
{
private:
    // Eğer Derived*, const volatile Base*'e dönüştürülebiliyorsa bu overload seçilir.
    static char test(const volatile Base *);
    // Dönüştürülemiyorsa bu overload seçilir (varargs => her şeye uyar).
    static int test(...);

public:
    // Eğer ilk overload seçilirse sizeof(char) döner, aksi halde sizeof(int) döner.
    static constexpr bool value = sizeof(test(static_cast<Derived *>(nullptr))) == sizeof(char);
};

// is_const şablon sınıfı
// bir tipin const olup olmadığını kontrol eder
// eğer const ise True döner değilse False döner
template <typename T>
struct is_const
{
    static constexpr bool value = false;
};
template <typename T>
struct is_const<const T>
{
    static constexpr bool value = true;
};
template <typename T>
struct is_const<const T &>
{
    static constexpr bool value = true;
};
template <typename T>
constexpr bool is_const_v = is_const<T>::value;

// value_category_of_string fonksiyonu
// bir nesnenin değer kategorisini döner
// LVALUE: sol taraf değeri
// RVALUE: sağ taraf değeri
// UNKNOWN: bilinmeyen değer kategorisi
// IS_LVALUE: bir nesnenin sol taraf değeri olup olmadığını kontrol eder
// IS_RVALUE: bir nesnenin sağ taraf değeri olup olmadığını kontrol eder
// IS_GLVALUE: bir nesnenin sol taraf değeri veya sağ taraf değeri olup olmadığını kontrol eder
// IS_XVALUE: bir nesnenin sağ taraf değeri olup olmadığını kontrol eder
// IS_PRVALUE: bir nesnenin sol taraf değeri olup olmadığını kontrol eder
#define IS_XVALUE(X) is_rvalue_reference<decltype(X)>::value
#define IS_LVALUE(X) is_lvalue_reference<decltype(X)>::value
#define IS_PRVALUE(X) !is_reference<decltype(X)>::value
#define IS_GLVALUE(X) (IS_LVALUE(X) || IS_XVALUE(X))
#define IS_RVALUE(X) (IS_PRVALUE(X) || IS_XVALUE(X))
template <typename T>
constexpr const char *value_category_of_string(T &&ref)
{

    if constexpr (IS_LVALUE(ref))
    {
        return "LVALUE";
    }
    else if constexpr (IS_RVALUE(ref))
    {
        return "RVALUE";
    }
    return "UNKNOWN";
}

// STL tuple emülasyonu
template <typename Head, typename... Tail>
class tuple
{
public:
    Head first;
    tuple<Tail...> rest;

    tuple(Head &&head, Tail &&...tail) : first(Head(forward<Head>(head))),
                                         rest(tuple<Tail...>(forward<Tail>(tail)...))
    {
    }
    

    auto &operator=(const tuple &other)
    {
        if (this != &other)
        {
            first = other.first;
            rest = other.rest;
        }
        return *this;
    }

    // bu operator sorunlu düzeltilecek ancak oldukca urasriracak:D
    template <typename PHead, typename... PTail>    
    tuple< PHead&, PTail&...>  &operator=(const tuple< PHead, PTail...> &other)
    {
        if (static_cast<const void*>(this) != static_cast<const void*>(&other))
        {
            this->first = other.first;
            const tuple<PHead&, PTail&...>  *_other = reinterpret_cast<const tuple<PHead&, PTail&...> *>(&other);
            this->rest = _other->rest;

         
        }
        return *this;
    }
    template <typename PHead, typename... PTail>    
    tuple< PHead&, PTail&...>  &operator=(const tuple< PHead&, PTail&...> &other)
    {
        if (this != &other)
        {
            this->first = other.first;
            this->rest = other.rest;
    
        }
        return *this;
    }
  

  
    template <size_t N = 0>
    constexpr auto &get() const
    {
        // günün sonunda aradığımız
        // elaman burada dönen first'dir
        if constexpr (N == 0)
        {
            return first;
        }
        else
        {
            return rest.template get<N - 1>();
        }
    }
};

template <typename Head>
class tuple<Head>
{
public:
    Head first;

    tuple(Head &&head) : first(forward<Head>(head))
    {
    }
    auto &operator=(const tuple &other)
    {
        if (this != &other)
        {
            first = other.first;
        }
        return *this;
    }

   
    template <size_t N = 0>
    constexpr auto &get() const
    {
        return first;
    }
};
template <typename... Ts>
constexpr tuple<Ts &...> tie(Ts &...args) noexcept
{
    return tuple<Ts &...>(args...);
}

template <size_t N = 0, typename... Args>
constexpr auto &get_tuple(tuple<Args &...> &t) noexcept
{
    return t.template get<N>();
}

template <size_t N = 0, typename... Args>
constexpr const auto &get_tuple(const tuple<Args...> &t) noexcept
{
    return t.template get<N>();
   
}



// declval: T tipinden rvalue referans gibi davranan bir ifade verir
template <typename T>
typename remove_reference<T>::type&& declval() noexcept;

//  T'nin noexcept move constructor'a sahip olup olmadığını derleme zamanında anlar
template <typename T>
struct is_nothrow_move_constructible
{
private:
    // Eğer U tipi, U(U&&) yani move constructible ise ve bu işlem noexcept ise bu overload seçilir
    template <typename U, typename = decltype(U(declval<U&&>()))>
    static constexpr bool test(int) noexcept(noexcept(U(declval<U&&>()))) 
    {
        return noexcept(U(declval<U&&>()));
    }

    // Yukarıdaki ifadeyi derleyemiyorsak (move constructor yok veya başka hata) burası seçilir
    template <typename>
    static constexpr bool test(...) 
    {
        return false;
    }

public:
    static constexpr bool value = test<T>(0);
};

#endif
