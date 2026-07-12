/**
 * @file OperationValidation.h
 * @brief Input-type validation for operations (the single contract enforced at creation).
 *
 * Extrude is a strictly sketch-region feature: it must consume a SketchRegionRef.
 * Face/body push-pull is no longer a valid extrude input — the user creates a sketch
 * on the face first (see auto-sketch-on-face fast path). Revolve and other operations
 * keep their existing, broader input acceptance.
 */
#ifndef ONECAD_APP_DOCUMENT_OPERATIONVALIDATION_H
#define ONECAD_APP_DOCUMENT_OPERATIONVALIDATION_H

#include "OperationRecord.h"

#include <string>
#include <variant>

namespace onecad::app {

/**
 * @brief Validate that an operation's input variant is allowed for its type.
 * @param type   Operation type.
 * @param input  Primary input variant.
 * @param whyNot Optional out-param; on rejection receives a user-facing reason.
 * @return true if the (type, input) pairing is permitted.
 */
inline bool validateOperationInputType(OperationType type,
                                       const OperationInput& input,
                                       std::string* whyNot = nullptr) {
    auto reject = [&](const char* reason) {
        if (whyNot) {
            *whyNot = reason;
        }
        return false;
    };

    switch (type) {
        case OperationType::Extrude:
            if (!std::holds_alternative<SketchRegionRef>(input)) {
                return reject("Extrude requires a sketch region; create a sketch on the "
                              "face or plane first, then extrude its region.");
            }
            return true;
        default:
            // Other operation types keep their existing, broader input acceptance.
            return true;
    }
}

} // namespace onecad::app

#endif // ONECAD_APP_DOCUMENT_OPERATIONVALIDATION_H
