/**
 * @file RollbackCommand.h
 * @brief Command to rollback to a specific operation (suppress downstream).
 */
#ifndef ONECAD_APP_COMMANDS_ROLLBACKCOMMAND_H
#define ONECAD_APP_COMMANDS_ROLLBACKCOMMAND_H

#include "Command.h"
#include <string>
#include <unordered_map>

namespace onecad {
namespace app {

class Document;

namespace commands {

/**
 * @brief Undoable command to rollback to a specific operation.
 *
 * Suppresses all operations after the target operation.
 * Can be undone to restore the suppressed operations.
 */
class RollbackCommand : public Command {
public:
    RollbackCommand(Document* document, const std::string& targetOpId);

    bool execute() override;
    bool undo() override;

private:
    Document* document_;
    std::string targetOpId_;
    std::unordered_map<std::string, bool> previousSuppression_;
};

} // namespace commands
} // namespace app
} // namespace onecad

#endif // ONECAD_APP_COMMANDS_ROLLBACKCOMMAND_H
