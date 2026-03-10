#pragma once

#include <QString>
#include <QJsonObject>
#include <functional>
#include <map>

namespace onecad::io {

class MigrationRegistry {
public:
    using MigrationFn = std::function<bool(QJsonObject& manifest)>;

    static MigrationRegistry& instance();

    void registerMigration(const QString& fromVersion, const QString& toVersion,
                           MigrationFn fn);

    bool needsMigration(const QString& fileVersion) const;
    bool migrate(const QString& fromVersion, QJsonObject& manifest) const;

    QString currentVersion() const;

private:
    MigrationRegistry() = default;

    struct Migration {
        QString toVersion;
        MigrationFn fn;
    };
    // fromVersion -> Migration
    std::map<QString, Migration> m_migrations;
};

} // namespace onecad::io
