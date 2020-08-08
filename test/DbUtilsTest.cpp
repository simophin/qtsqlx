//
// Created by Fanchao Liu on 8/08/20.
//

#include <QtTest/QtTest>
#include <QSqlField>
#include <QVariant>

#include <algorithm>

#include "DbUtils.h"

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

class DbUtilsTest : public QObject {
Q_OBJECT

    QSqlDatabase db;

private slots:

    void initTestCase() {
        db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
        db.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(db.open());

        sqlx::DbUtils::update(
                db,
                QStringLiteral("create table tests (id integer primary key, name text)")
        );
    };

    void cleanupTestCase() {
        db.close();
    }

    void testReadFromPOD() {
        struct {
            const char *testName;
            QSqlRecord record;
            TestObject object;
        } testData[] = {
                {
                        "Happy day",
                        createRecord(QStringLiteral("id"), 1, QStringLiteral("name"), QStringLiteral("Name1")),
                        TestObject{1, QStringLiteral("Name1")},
                },

                {
                        "Incorrect type",
                        createRecord(QStringLiteral("id"), QStringLiteral("string id"), QStringLiteral("name"),
                                     QStringLiteral("Name1")),
                        TestObject{0, QStringLiteral("Name1")},
                },
        };

        for (const auto &[testName, record, expected] : testData) {
            TestObject actual;
            QVERIFY(sqlx::DbUtils::readFrom(actual, record));
            QCOMPARE(actual.id, expected.id);
            QCOMPARE(actual.name, expected.name);
        }
    }

    void testReadFromInt() {
        int actual = 0;
        QVERIFY(sqlx::DbUtils::readFrom(actual, createRecord(QStringLiteral("key1"), 5)));
        QCOMPARE(actual, 5);

        actual = 0;
        QVERIFY(sqlx::DbUtils::readFrom(actual, createRecord(QStringLiteral("key1"), QStringLiteral("5"))));
        QCOMPARE(actual, 5);

        actual = 0;
        QVERIFY(!sqlx::DbUtils::readFrom(actual, createRecord(QStringLiteral("key1"), QStringLiteral("5xu"))));
        QCOMPARE(actual, 0);
    }

    void testReadFromString() {
        QString actual;
        QVERIFY(sqlx::DbUtils::readFrom(actual, createRecord(QStringLiteral("key1"), 5)));
        QCOMPARE(actual, QStringLiteral("5"));

        actual.clear();
        QVERIFY(sqlx::DbUtils::readFrom(actual, createRecord(QStringLiteral("key1"), QStringLiteral("5"))));
        QCOMPARE(actual, QStringLiteral("5"));

        actual.clear();
        QVERIFY(sqlx::DbUtils::readFrom(actual, createRecord(QStringLiteral("key1"), true)));
        QCOMPARE(actual, QStringLiteral("true"));
    }

    void testQueryList_data() {
        QTest::addColumn<QString>("querySql");
        QTest::addColumn<QVector<QVariant>>("binds");
        QTest::addColumn<QVector<TestObject>>("input");
        QTest::addColumn<QVector<TestObject>>("expectedData");
        QTest::addColumn<bool>("expectedSuccess");

        QTest::newRow("happy day")
                << QStringLiteral("select * from tests")
                << QVector<QVariant>()
                << QVector<TestObject>{{1, QStringLiteral("Name 1")},
                                       {2, QStringLiteral("Name 2")}}
                << QVector<TestObject>{{1, QStringLiteral("Name 1")},
                                       {2, QStringLiteral("Name 2")}}
                << true;

        QTest::newRow("happy day #2")
                << QStringLiteral("select * from tests where id = 1")
                << QVector<QVariant>()
                << QVector<TestObject>{{1, QStringLiteral("Name 1")},
                                       {2, QStringLiteral("Name 2")}}
                << QVector<TestObject>{{1, QStringLiteral("Name 1")}}
                << true;

        QTest::newRow("incorrect syntax")
                << QStringLiteral("select * from tests2")
                << QVector<QVariant>()
                << QVector<TestObject>{{1, QStringLiteral("Name 1")},
                                       {2, QStringLiteral("Name 2")}}
                << QVector<TestObject>()
                << false;
    }

    void testQueryList() {
        QFETCH(QString, querySql);
        QFETCH(QVector<QVariant>, binds);
        QFETCH(QVector<TestObject>, input);
        QFETCH(QVector<TestObject>, expectedData);
        QFETCH(bool, expectedSuccess);

        for (const auto &obj : input) {
            QVERIFY(sqlx::DbUtils::insert<decltype(TestObject::id)>(
                    db,
                    QStringLiteral("insert into tests (id, name) values (?, ?)"),
                    {obj.id, obj.name}).success());
        }

        auto actual = sqlx::DbUtils::queryList<TestObject>(db, querySql, binds);
        if (expectedSuccess) {
            QVERIFY(actual.success());
            std::sort(actual->begin(), actual->end());
            QCOMPARE(*actual, expectedData);
        } else {
            QVERIFY(actual.error());
        }

        QVERIFY(sqlx::DbUtils::update(db,
                                      QStringLiteral("delete from tests where 1")).orDefault(0) > 0);
    }
};

QTEST_MAIN(DbUtilsTest)

#include "DbUtilsTest.moc"