/**
 * @file HistoryIO.h
 * @brief Serialization for operation history (JSONL format)
 */

#pragma once

#include <QJsonObject>
#include <QString>
#include <unordered_map>
#include <vector>

namespace onecad::app {
class Document;
struct OperationRecord;
}

namespace onecad::io {

class Package;

/**
 * @brief Serialization for history/ops.jsonl and history/state.json
 * 
 * Per FILE_FORMAT.md §7:
 * Uses JSON Lines format (one JSON object per line) for Git-friendly diffs.
 */
class HistoryIO {
public:
    /**
     * @brief Save operation history to package
     */
    static bool saveHistory(Package* package,
                            const std::vector<app::OperationRecord>& operations,
                            const std::unordered_map<std::string, bool>& suppressionState);
    
    /**
     * @brief Load operation history from package
     */
    static bool loadHistory(Package* package,
                            app::Document* document,
                            QString& errorMessage);
    
    /**
     * @brief Serialize single operation to JSON
     */
    static QJsonObject serializeOperation(const app::OperationRecord& op);
    
    /**
     * @brief Deserialize JSON to operation record
     */
    static app::OperationRecord deserializeOperation(const QJsonObject& json);
    
    /**
     * @brief Compute hash of operations for cache validation
     */
    static QString computeOpsHash(const std::vector<app::OperationRecord>& operations);
    
private:
    HistoryIO() = delete;
};

} // namespace onecad::io
