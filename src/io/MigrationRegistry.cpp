#include "MigrationRegistry.h"
#include "ManifestIO.h"

namespace onecad::io {

MigrationRegistry& MigrationRegistry::instance() {
    static MigrationRegistry registry = [] {
        MigrationRegistry r;
        // 1.0.0 -> 1.1.0: additive-only change (extrude two-direction params,
        // datumPlanes array); the manifest itself needs no transformation.
        r.registerMigration("1.0.0", "1.1.0", [](QJsonObject&) { return true; });
        return r;
    }();
    return registry;
}

void MigrationRegistry::registerMigration(
    const QString& fromVersion, const QString& toVersion, MigrationFn fn) {
    m_migrations[fromVersion] = {toVersion, std::move(fn)};
}

bool MigrationRegistry::needsMigration(const QString& fileVersion) const {
    return !fileVersion.isEmpty() && fileVersion != currentVersion();
}

bool MigrationRegistry::migrate(
    const QString& fromVersion, QJsonObject& manifest) const {
    QString current = fromVersion;
    int steps = 0;
    constexpr int kMaxSteps = 100;

    while (current != currentVersion() && steps < kMaxSteps) {
        auto it = m_migrations.find(current);
        if (it == m_migrations.end()) {
            return false; // No migration path
        }
        if (!it->second.fn(manifest)) {
            return false; // Migration failed
        }
        current = it->second.toVersion;
        ++steps;
    }
    return current == currentVersion();
}

QString MigrationRegistry::currentVersion() const {
    return ManifestConstants::FORMAT_VERSION;
}

} // namespace onecad::io
