//
// Created by Fanchao Liu on 8/08/20.
//

#include <QSqlField>
#include <QVariant>
#include <QObject>

#include <algorithm>

#include "DbUtils.h"

#include <catch2/catch.hpp>

struct TestObject {
Q_GADGET
public:

    int id = 0;
    Q_PROPERTY(int id MEMBER id);

    QString name;
    Q_PROPERTY(QString name MEMBER name);

    bool operator==(const TestObject &rhs) const {
        return id == rhs.id &&
               name == rhs.name;
    }

    bool operator!=(const TestObject &rhs) const {
        return !(rhs == *this);
    }

    inline bool operator<(const TestObject &rhs) const {
        return id < rhs.id;
    }
};

inline void updateRecord(QSqlRecord &record) {
}

template<typename T, typename...Args>
inline void updateRecord(QSqlRecord &record, const QString &key, T value, Args...args) {
    auto v = QVariant::fromValue(value);
    QSqlField field(key, v.type());
    field.setValue(v);
    record.append(field);
    updateRecord(record, args...);
}

template<typename...Args>
static QSqlRecord createRecord(Args...args) {
    QSqlRecord record;
    updateRecord(record, args...);
    return record;
}

TEST_CASE("Should read from POD") {
    auto[inputRecord, expectedObject] = GENERATE(table<QSqlRecord, TestObject>(
            {
                    {
                            createRecord("id", 1, "name", QStringLiteral("Name1")),
                            TestObject{1, "Name1"},
                    },
                    {
                            createRecord("id", QStringLiteral("string id"), "name", QStringLiteral("Name1")),
                            TestObject{0, "Name1"},
                    },
            }));

    TestObject actual;
    REQUIRE(sqlx::DbUtils::readFrom(actual, inputRecord));
    CHECK(actual.id == expectedObject.id);
    CHECK(actual.name == expectedObject.name);
}

TEST_CASE("Should read from int") {
    auto[inputRecord, expectedOutput, expectedSuccess] = GENERATE(table<QSqlRecord, int, bool>(
            {
                    {createRecord("key1", 5),                                5, true},
                    {createRecord("key1", 5.0),                              5, true},
                    {createRecord("key1", QStringLiteral("5")),              5, true},
                    {createRecord("key1", true),                             1, true},
                    {createRecord("key1", false),                            0, true},
                    {createRecord("key1", QStringLiteral("unknown number")), 0, false},
            }
    ));

    int actual = 0;
    CHECK(sqlx::DbUtils::readFrom(actual, inputRecord) == expectedSuccess);
    CHECK(actual == expectedOutput);
}

TEST_CASE("Should read from string") {
    auto[inputRecord, expectedOutput, expectedSuccess] = GENERATE(table<QSqlRecord, QString, bool>(
            {
                    {createRecord("key1", 5),                   QStringLiteral("5"),     true},
                    {createRecord("key1", 5.0),                 QStringLiteral("5"),     true},
                    {createRecord("key1", QStringLiteral("5")), QStringLiteral("5"),     true},
                    {createRecord("key1", true),                QStringLiteral("true"),  true},
                    {createRecord("key1", false),               QStringLiteral("false"), true},
            }
    ));

    QString actual;
    CHECK(sqlx::DbUtils::readFrom(actual, inputRecord) == expectedSuccess);
    CHECK(actual == expectedOutput);
}

TEST_CASE("Should perform correct database operations") {
    auto db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(":memory:");
    REQUIRE(db.open());
    REQUIRE(sqlx::DbUtils::update(db, "create table tests (id integer primary key on conflict fail, name text)"));

    QVector<TestObject> inputs(50);
    for (int i = 0; i < inputs.size(); i++) {
        inputs[i].id = i + 1;
        inputs[i].name = QStringLiteral("Name %1").arg(i + 1);
        REQUIRE(sqlx::DbUtils::insert<int>(db, "insert into tests (id, name) values (?, ?)",
                                           {inputs[i].id, inputs[i].name}));
    }

    SECTION("queryList with POD") {
        auto[querySql, binds, expectedData, expectedSuccess] = GENERATE_COPY(
                table<QString, QVector<QVariant>, QVector<TestObject>, bool>(
                        {
                                {
                                        "select * from tests order by id asc limit 10",
                                        {},  inputs.mid(0, 10), true,
                                },
                                {
                                        "select * from tests where id = ?",
                                        {1}, inputs.mid(0, 1),  true,
                                },
                                {
                                        "select * from tests2 where id = ?",
                                        {1}, {},                false,
                                },
                        }
                ));

        auto actual = sqlx::DbUtils::queryList<TestObject>(db, querySql, binds);
        if (expectedSuccess) {
            REQUIRE(actual.success());
            std::sort(actual->begin(), actual->end());
            CHECK(*actual == expectedData);
        } else {
            REQUIRE(actual.error());
        }
    }

    SECTION("queryList with primitive") {
        auto[querySql, binds, expectedData, expectedSuccess] = GENERATE(
                table<QString, QVector<QVariant>, QVector<int>, bool>(
                        {
                                {
                                        "select id from tests order by id asc limit 3",
                                        {},  {1, 2, 3}, true,
                                },
                                {
                                        "select id from tests where id = ?",
                                        {1}, {1},       true,
                                },
                                {
                                        "select * from tests2 where id = ?",
                                        {1}, {},        false,
                                },
                        }
                ));

        auto actual = sqlx::DbUtils::queryList<int>(db, querySql, binds);
        if (expectedSuccess) {
            REQUIRE(actual.success());
            std::sort(actual->begin(), actual->end());
            CHECK(*actual == expectedData);
        } else {
            REQUIRE(actual.error());
        }
    }

    SECTION("queryStream") {
        auto[querySql, binds, expectedData, expectedSize, expectedSuccess] = GENERATE_COPY(
                table<QString, QVector<QVariant>, QVector<TestObject>, size_t, bool>(
                        {
                                {
                                        "select * from tests order by id asc limit 10",
                                        {},  inputs.mid(0, 10), 10, true,
                                },
                                {
                                        "select * from tests where id = ?",
                                        {1}, inputs.mid(0, 1),  1,  true,
                                },
                                {
                                        "select * from tests2 where id = ?",
                                        {1}, {},                0,  false
                                },
                        }
                ));

        QVector<TestObject> output;

        auto actualSize = sqlx::DbUtils::queryStream<TestObject>(db, querySql, binds, [&](auto &obj) {
            output.append(obj);
            return true;
        });

        if (expectedSuccess) {
            REQUIRE(actualSize);
            CHECK(*actualSize == expectedSize);
        } else {
            REQUIRE(!actualSize);
        }

        CHECK(output == expectedData);

    }

    SECTION("queryRawStream") {
        auto[querySql, binds, successExpected, columnExpected] = GENERATE_COPY(
                table<QString, QVector<QVariant>, std::optional<size_t>, QVector<QString>>(
                        {
                                {
                                        "select id, name from tests order by id asc limit 10",
                                        {},  10,           {"id", "name"}
                                },
                                {
                                        "select * from tests2 where id = ?",
                                        {1}, std::nullopt, {}
                                },
                        }
                ));

        auto columns = columnExpected;

        auto actualResult = sqlx::DbUtils::queryRawStream(db, querySql, binds, [&](QSqlRecord record) {
            for (const QString &columnName : columns) {
                REQUIRE(record.contains(columnName));
            }
            return true;
        });

        CHECK(actualResult.toOptional() == successExpected);
    }

    SECTION("queryFirst POD") {
        auto[sql, binds, expected] = GENERATE_COPY(table<QString, QVector<QVariant>, std::optional<TestObject>>(
                {
                        {
                                "select * from tests where id = ?",
                                {1},  inputs[0],
                        },
                        {
                                "select * from tests order by id asc",
                                {},   inputs[0],
                        },
                        {
                                "select * from tests where id = ?",
                                {-1}, std::nullopt,
                        },
                        {
                                "select * from tests2",
                                {},   std::nullopt,
                        },
                }));

        auto actual = sqlx::DbUtils::queryFirst<TestObject>(db, sql, binds);
        CHECK(actual.toOptional() == expected);
    }

    SECTION("queryFirst primitive") {
        auto[sql, binds, expected] = GENERATE_COPY(table<QString, QVector<QVariant>, std::optional<int>>(
                {
                        {
                                "select id from tests where id = ?",
                                {1},  inputs[0].id,
                        },
                        {
                                "select id from tests order by id asc",
                                {},   inputs[0].id,
                        },
                        {
                                "select id from tests where id = ?",
                                {-1}, std::nullopt,
                        },
                        {
                                "select id from tests2",
                                {},   std::nullopt,
                        },
                }));

        auto actual = sqlx::DbUtils::queryFirst<int>(db, sql, binds);
        CHECK(actual.toOptional() == expected);
    }

    SECTION("insert") {
        auto[sql, binds, expected] = GENERATE(table<QString, QVector<QVariant>, std::optional<int>>(
                {
                        {
                                "insert into tests (id, name) values (?, 'test')",
                                {100}, 100,
                        },
                        {
                                "insert into tests (id, name) values (?, 'test')",
                                {1},   std::nullopt,
                        }
                }));

        auto actual = sqlx::DbUtils::insert<int>(db, sql, binds);
        CHECK(actual.toOptional() == expected);
    }

    SECTION("update") {
        auto[sql, binds, expected] = GENERATE_COPY(table<QString, QVector<QVariant>, std::optional<int>>(
                {
                        {
                                "update tests set name = 'test_name' where id = ? or id = ?",
                                {1, 2}, 2,
                        },
                        {
                                "delete from tests where 1",
                                {},     inputs.size(),
                        },
                        {
                                "delete from tests2 where 1",
                                {},     std::nullopt
                        },
                }));

        auto actual = sqlx::DbUtils::update(db, sql, binds);
        CHECK(actual.toOptional() == expected);
    }

    db.close();
}


#include "DbUtilsTest.moc"