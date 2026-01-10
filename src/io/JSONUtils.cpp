/**
 * @file JSONUtils.cpp
 * @brief Implementation of JSON utilities
 */

#include "JSONUtils.h"

#include <QCryptographicHash>
#include <algorithm>

namespace onecad::io {

QByteArray JSONUtils::toCanonicalJson(const QJsonObject& obj) {
    // Qt's QJsonDocument produces consistent output with Indented format
    // Keys are sorted in the order they were inserted, so we need to 
    // rebuild with sorted keys for true determinism
    
    QJsonObject sorted;
    QStringList keys = obj.keys();
    keys.sort();
    
    for (const QString& key : keys) {
        sorted.insert(key, obj.value(key));
    }
    
    QJsonDocument doc(sorted);
    return doc.toJson(QJsonDocument::Indented);
}

QByteArray JSONUtils::toCanonicalJson(const QJsonArray& arr) {
    QJsonDocument doc(arr);
    return doc.toJson(QJsonDocument::Indented);
}

QString JSONUtils::generateUuid() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString JSONUtils::currentTimestamp() {
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

QDateTime JSONUtils::parseTimestamp(const QString& timestamp) {
    return QDateTime::fromString(timestamp, Qt::ISODate);
}

QJsonArray JSONUtils::toJsonArray(const Eigen::Vector3d& vec) {
    return QJsonArray{vec.x(), vec.y(), vec.z()};
}

QJsonArray JSONUtils::toJsonArray(const Eigen::Vector2d& vec) {
    return QJsonArray{vec.x(), vec.y()};
}

Eigen::Vector3d JSONUtils::toVector3d(const QJsonArray& arr) {
    if (arr.size() < 3) {
        return Eigen::Vector3d::Zero();
    }
    return Eigen::Vector3d(
        arr[0].toDouble(),
        arr[1].toDouble(),
        arr[2].toDouble()
    );
}

Eigen::Vector2d JSONUtils::toVector2d(const QJsonArray& arr) {
    if (arr.size() < 2) {
        return Eigen::Vector2d::Zero();
    }
    return Eigen::Vector2d(
        arr[0].toDouble(),
        arr[1].toDouble()
    );
}

bool JSONUtils::isValidUuid(const QString& uuid) {
    QUuid parsed = QUuid::fromString(uuid);
    return !parsed.isNull();
}

QString JSONUtils::computeSha256(const QByteArray& data) {
    QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex());
}

} // namespace onecad::io
