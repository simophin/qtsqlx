//
// Created by Fanchao Liu on 1/07/20.
//

#ifndef GAMEMATCHER_TYPEUTILS_H
#define GAMEMATCHER_TYPEUTILS_H

#include <boost/tti/has_static_member_data.hpp>

#include <QString>
#include <QMetaEnum>
#include <optional>

namespace sqlx {

    BOOST_TTI_TRAIT_HAS_STATIC_MEMBER_DATA(HasMetaObject, staticMetaObject)

    template<typename EnumType>
    static inline QString enumToString(EnumType e) {
        return QString(QLatin1String(QMetaEnum::fromType<EnumType>().valueToKey(e)));
    }

    template<typename EnumType>
    static inline std::optional<EnumType> enumFromString(const char *str) {
        bool ok;
        auto rc = static_cast<EnumType>(QMetaEnum::fromType<EnumType>().keyToValue(str, &ok));
        if (ok) return rc;
        return std::nullopt;
    }

}

#endif //GAMEMATCHER_TYPEUTILS_H
