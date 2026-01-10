/**
 * @file SketchIO.h
 * @brief Serialization for sketch JSON files
 */

#pragma once

#include <QJsonObject>
#include <QString>
#include <memory>

namespace onecad::core::sketch {
class Sketch;
}

namespace onecad::io {

class Package;

/**
 * @brief Serialization for sketches/{uuid}.json
 * 
 * Per FILE_FORMAT.md §11:
 * Each sketch is stored as a separate JSON file for Git diff clarity.
 */
class SketchIO {
public:
    /**
     * @brief Save sketch to package
     */
    static bool saveSketch(Package* package, 
                           const QString& sketchId,
                           const core::sketch::Sketch* sketch);
    
    /**
     * @brief Load sketch from package
     */
    static std::unique_ptr<core::sketch::Sketch> loadSketch(
        Package* package,
        const QString& sketchId,
        QString& errorMessage);
    
    /**
     * @brief Serialize sketch to JSON
     */
    static QJsonObject serializeSketch(const QString& sketchId,
                                        const core::sketch::Sketch* sketch);
    
    /**
     * @brief Deserialize JSON to new sketch
     */
    static std::unique_ptr<core::sketch::Sketch> deserializeSketch(
        const QJsonObject& json,
        QString& errorMessage);
    
private:
    SketchIO() = delete;
};

} // namespace onecad::io
