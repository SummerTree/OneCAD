/**
 * @file ElementMapIO.h
 * @brief Serialization for ElementMap topology data
 */

#pragma once

#include <QJsonObject>
#include <QString>

namespace onecad::kernel::elementmap {
class ElementMap;
}

namespace onecad::io {

class Package;

/**
 * @brief Serialization for topology/elementmap.json
 * 
 * Per FILE_FORMAT.md §12:
 * Versioned descriptor schema with stable hashing metadata.
 */
class ElementMapIO {
public:
    /**
     * @brief Save ElementMap to package
     */
    static bool saveElementMap(Package* package,
                                const kernel::elementmap::ElementMap& elementMap);
    
    /**
     * @brief Load ElementMap from package
     */
    static bool loadElementMap(Package* package,
                                kernel::elementmap::ElementMap& elementMap,
                                QString& errorMessage);
    
    /**
     * @brief Serialize ElementMap to JSON
     */
    static QJsonObject serializeElementMap(const kernel::elementmap::ElementMap& elementMap);
    
    /**
     * @brief Deserialize JSON to ElementMap
     */
    static bool deserializeElementMap(const QJsonObject& json,
                                       kernel::elementmap::ElementMap& elementMap,
                                       QString& errorMessage);
    
private:
    ElementMapIO() = delete;
};

} // namespace onecad::io
