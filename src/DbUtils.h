//
// Created by Fanchao Liu on 25/06/20.
//

#ifndef GAMEMATCHER_DBUTILS_H
#define GAMEMATCHER_DBUTILS_H

#include <QMetaObject>
#include <QMetaProperty>
#include <QSqlQuery>
#include <optional>
#include <QtDebug>
#include <QSqlError>
#include <QSqlRecord>
#include <QHash>
#include <QString>
#include <QVector>
#include <QVariant>
#include <QTimeZone>
#include <variant>

#include <type_traits>

#include "TypeUtils.h"

namespace sqlx {

    template<typename T>
    struct QueryResult {
        mutable std::variant<QSqlError, T> result;

        inline QueryResult(const QSqlError &e) : result(e) {}

        inline QueryResult(const QSqlError *e) : result(*e) {}

        inline QueryResult(const T &data) : result(data) {}

        inline QueryResult() = default;

        inline T *success() const {
            return std::get_if<T>(&result);
        }

        inline QSqlError *error() const {
            return std::get_if<QSqlError>(&result);
        }

        inline T orDefault(T defaultValue = T()) {
            if (auto d = success()) {
                return *d;
            }
            return defaultValue;
        }

        inline std::optional<T> toOptional() {
            if (auto d = success()) {
                return *d;
            }
            return std::nullopt;
        }

        inline explicit operator bool() const {
            return success() != nullptr;
        }

        inline T *operator->() {
            assert(success());
            return success();
        }

        inline T &operator*() {
            assert(success());
            return *success();
        }
    };

    class DbUtils {
    public:

        template<typename Entity, std::enable_if_t<HasMetaObject<Entity, const QMetaObject>::value, int> = 0>
        static bool readFrom(Entity &entity, const QSqlRecord &record) {
            static auto propertyMaps = [] {
                QHash<QString, QMetaProperty> result;
                const QMetaObject *metaObject = &Entity::staticMetaObject;
                while (metaObject) {
                    for (auto i = metaObject->propertyCount() - 1; i >= 0; i--) {
                        auto prop = metaObject->property(i);
                        result[QLatin1String(prop.name())] = prop;
                    }
                    metaObject = metaObject->superClass();
                }
                return result;
            }();

            for (int i = 0; i < record.count(); i++) {
                auto key = record.fieldName(i);
                auto prop = propertyMaps.constFind(key);
                if (prop == propertyMaps.constEnd()) {
                    qWarning() << "Unable to find property " << key
                               << " in the entity: " << Entity::staticMetaObject.className();
                    continue;
                }

                if (!prop->isWritable()) {
                    qWarning() << "Unable to write to property: " << key
                               << " in the entity: " << Entity::staticMetaObject.className();
                }

                QVariant value;
                if (!record.isNull(i)) {
                    value = record.value(i);
                }

                if (prop->isEnumType()) {
                    auto valueAsString = value.toString();
                    bool converted = false;
                    QMetaEnum e = prop->enumerator();
                    for (int j = 0, size = e.keyCount(); j < size; j++) {
                        if (valueAsString.compare(QLatin1String(e.key(j)), Qt::CaseInsensitive) == 0) {
                            value = e.value(j);
                            converted = true;
                            break;
                        }
                    }
                    if (!converted) {
                        qWarning().nospace() << "Unable to convert " << valueAsString << " to " << prop->type();
                    }
                }
                else if (prop->type() == QVariant::DateTime) {
                    // Try to convert to a date time.

                    // Firstly try integer then string in UTC.
                    bool ok;
                    if (auto longValue = value.toLongLong(&ok); ok) {
                        value = QDateTime::fromSecsSinceEpoch(longValue, QTimeZone::utc());
                    } else {
                        value = QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
                    }
                }

                if (!prop->writeOnGadget(&entity, value)) {
                    qWarning().nospace() << "Unable to write to property: " << key
                                         << " in the entity: " << Entity::staticMetaObject.className()
                                         << ", withValue = " << value;
                }
            }
            return true;
        }

        template<typename T, std::enable_if_t<!HasMetaObject<T, const QMetaObject>::value, int> = 0>
        static bool readFrom(T &out, const QSqlRecord &record) {
            if (auto v = record.value(0); v.convert(qMetaTypeId<T>()) ) {
                out = v.value<T>();
                return true;
            } else {
                qWarning().nospace() << "Unable to convert from primitive";
            }
            return false;
        }

        static QueryResult<QSqlQuery> buildQuery(QSqlDatabase &db, const QString &sql, const QVector<QVariant> &binds) {
            QueryResult<QSqlQuery> rc;
            QSqlQuery q(db);
            if (!q.prepare(sql)) {
                qWarning() << "Error preparing: " << q.lastQuery() << ": " << q.lastError();
                rc.result = q.lastError();
                return rc;
            }

            for (int i = 0, size = binds.size(); i < size; i++) {
                q.bindValue(i, binds[i]);
            }

            if (!q.exec()) {
                qWarning() << "Error executing: " << q.lastQuery() << ": " << q.lastError();
                rc.result = q.lastError();
                return rc;
            }

            rc.result = q;
            return rc;
        }

        template<typename ResultType>
        static inline QueryResult<QVector<ResultType>>
        queryList(QSqlDatabase &db, const QString &sql, const QVector<QVariant> &binds = {}) {
            auto query = buildQuery(db, sql, binds);
            if (!query) return query.error();
            if (!query->isSelect()) return {};
            QVector<ResultType> result;
            result.reserve(query->size());
            while (query->next()) {
                ResultType r;
                if (!readFrom(r, query->record())) {
                    return QSqlError(QObject::tr("Unable to read from record"));
                }
                result.push_back(r);
            }
            return result;
        }

        template<typename ResultType, typename Streamer>
        static inline QueryResult<size_t>
        queryStream(QSqlDatabase &db, const QString &sql, const QVector<QVariant> &binds,
                    Streamer streamer) {
            auto query = buildQuery(db, sql, binds);
            if (!query) return query.error();
            if (!query->isSelect()) return {};
            size_t rc = 0;
            while (query->next()) {
                ResultType r;
                if (!readFrom(r, query->record())) {
                    return QSqlError(QObject::tr("Unable to read from record"));
                }

                if (!streamer(r)) {
                    break;
                }

                rc++;
            }
            return rc;
        }

        template<typename Streamer>
        static inline QueryResult<size_t> queryRawStream(
                QSqlDatabase &db, const QString &sql, const QVector<QVariant> &binds, Streamer streamer) {
            auto query = buildQuery(db, sql, binds);
            if (!query) return query.error();
            if (!query->isSelect()) return {};
            size_t rc = 0;
            while (query->next()) {
                if (!streamer(query->record())) {
                    break;
                }

                rc++;
            }
            return rc;
        }


        template<typename ResultType>
        static inline QueryResult<ResultType>
        queryFirst(QSqlDatabase &db, const QString &sql, const QVector<QVariant> &binds = {}) {
            auto result = queryList<ResultType>(db, sql, binds);
            if (!result) return result.error();
            if (result->isEmpty()) {
                return QSqlError(QObject::tr("Empty data set"));
            }
            return result->at(0);
        }

        template<typename IdType>
        static inline QueryResult<IdType>
        insert(QSqlDatabase &db, const QString &sql, const QVector<QVariant> &binds = {}) {
            auto query = buildQuery(db, sql, binds);
            if (!query) return query.error();
            if (auto id = query->lastInsertId(); id.isValid()) {
                return id.value<IdType>();
            }
            return QSqlError(QObject::tr("Unable to retrieve lastInsertId"));
        }

        static inline QueryResult<int>
        update(QSqlDatabase &db, const QString &sql, const QVector<QVariant> &binds = {}) {
            auto query = buildQuery(db, sql, binds);
            if (!query) return query.error();
            return query->numRowsAffected();
        }
    };

}

#endif //GAMEMATCHER_DBUTILS_H
