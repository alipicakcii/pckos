// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_LIBRARY_PCK_RTTI_HH
#define PCKOS_LIBRARY_PCK_RTTI_HH

namespace pckos
{
    // Derleyici RTTI’si kapalıyken kullanılan, projeye özgü hafif tür etiketleri.
    using TypeToken = void (*)();

    template <typename T>
    void type_tag()
    {
    }

    template <typename T>
    TypeToken type_token()
    {
        return &type_tag<T>;
    }

    template <typename Target, typename Source>
    Target *dynamic_cast_to(Source *source)
    {
        if (source == nullptr || !source->pckos_is_a(type_token<Target>()))
            return nullptr;
        return static_cast<Target *>(source);
    }

    template <typename Target, typename Source>
    const Target *dynamic_cast_to(const Source *source)
    {
        if (source == nullptr || !source->pckos_is_a(type_token<Target>()))
            return nullptr;
        return static_cast<const Target *>(source);
    }
}

#endif
