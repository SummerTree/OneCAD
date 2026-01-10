/**
 * @file JSONUtils.h
 * @brief Utilities for deterministic JSON serialization
 * 
 * Per FILE_FORMAT.md: All JSON must be canonical with stable key ordering,
 * fixed float format, and consistent indentation.
 */

#pragma once

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QByteArray>
#include <QString>
#include <QDateTime>
#include <QUuid>

#include <Eigen/Core>

namespace onecad::io {

/**
 * @brief Utilities for JSON serialization
 */
class JSONUtils {
public:
    /**
     * @brief Convert JSON object to canonical byte array
     * 
     * Produces deterministic output suitable for Git:
     * - Stable key ordering (lexicographic)
     * - Consistent indentation (2 spaces)
     * - Fixed float precision
     */
    static QByteArray toCanonicalJson(const QJsonObject& obj);
    static QByteArray toCanonicalJson(const QJsonArray& arr);
    
    /**
     * @brief Generate new UUID string
     */
    static QString generateUuid();
    
    /**
     * @brief Get current timestamp in ISO 8601 UTC format
     */
    static QString currentTimestamp();
    
    /**
     * @brief Parse ISO 8601 timestamp
     */
    static QDateTime parseTimestamp(const QString& timestamp);
    
    // =========================================================================
    // Eigen <-> JSON
    // =========================================================================
    
    /**
     * @brief Convert Eigen Vector3d to JSON array
     */
    static QJsonArray toJsonArray(const Eigen::Vector3d& vec);
    
    /**
     * @brief Convert Eigen Vector2d to JSON array
     */
    static QJsonArray toJsonArray(const Eigen::Vector2d& vec);
    
    /**
     * @brief Parse JSON array to Vector3d
     */
    static Eigen::Vector3d toVector3d(const QJsonArray& arr);
    
    /**
     * @brief Parse JSON array to Vector2d
     */
    static Eigen::Vector2d toVector2d(const QJsonArray& arr);
    
    // =========================================================================
    // Validation
    // =========================================================================
    
    /**
     * @brief Validate UUID format
     */
    static bool isValidUuid(const QString& uuid);
    
    /**
     * @brief Compute SHA-256 hash of data
     */
    static QString computeSha256(const QByteArray& data);
    
private:
    JSONUtils() = delete;
};

} // namespace onecad::io
