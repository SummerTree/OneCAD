/**
 * @file OperationRecord.h
 * @brief History operation records for modeling features.
 */
#ifndef ONECAD_APP_DOCUMENT_OPERATIONRECORD_H
#define ONECAD_APP_DOCUMENT_OPERATIONRECORD_H

#include <string>
#include <variant>
#include <vector>

namespace onecad::app {

// ─────────────────────────────────────────────────────────────────────────────
// Operation Types
// ─────────────────────────────────────────────────────────────────────────────

enum class OperationType {
    Extrude,
    Revolve,
    Fillet,
    Chamfer,
    Shell,
    Boolean,
    LinearPattern,
    CircularPattern,
    Loft,
    Sweep,
    MirrorBody
};

enum class BooleanMode {
    NewBody,
    Add,
    Cut,
    Intersect
};

// ─────────────────────────────────────────────────────────────────────────────
// Reference Types (for inputs and topology references)
// ─────────────────────────────────────────────────────────────────────────────

struct SketchRegionRef {
    std::string sketchId;
    std::string regionId;
};

struct FaceRef {
    std::string bodyId;
    std::string faceId;
    std::vector<std::string> patchFaceIds;    // Optional explicit patch members; empty for legacy ops
};

struct SketchLineRef {
    std::string sketchId;
    std::string lineId;
};

struct EdgeRef {
    std::string bodyId;
    std::string edgeId;
};

struct BodyRef {
    std::string bodyId;
};

// ─────────────────────────────────────────────────────────────────────────────
// Operation Input (primary input for any operation)
// ─────────────────────────────────────────────────────────────────────────────

using OperationInput = std::variant<
    std::monostate,      // No input (e.g., for Boolean which uses params)
    SketchRegionRef,     // Extrude/Revolve from sketch region
    FaceRef,             // Push/pull face, shell input
    BodyRef              // Fillet/chamfer/shell/boolean target
>;

// ─────────────────────────────────────────────────────────────────────────────
// Parameter Structs (operation-specific configuration)
// ─────────────────────────────────────────────────────────────────────────────

enum class ExtrudeMode {
    Blind,       // Fixed distance (default)
    ThroughAll,  // Through entire model
    Symmetric,   // Half-distance both directions
    ToNext,      // To next face intersection
    ToFace       // To a specific target face
};

struct ExtrudeParams {
    double distance = 0.0;
    double draftAngleDeg = 0.0;
    ExtrudeMode extrudeMode = ExtrudeMode::Blind;
    BooleanMode booleanMode = BooleanMode::NewBody;
    std::string targetBodyId;               // Optional explicit boolean target body
    std::string targetFaceId;               // For ToFace mode
};

struct RevolveParams {
    double angleDeg = 360.0;
    using AxisRef = std::variant<std::monostate, SketchLineRef, EdgeRef>;
    AxisRef axis;
    BooleanMode booleanMode = BooleanMode::NewBody;
    std::string targetBodyId;               // Optional explicit boolean target body
};

struct FilletChamferParams {
    enum class Mode { Fillet, Chamfer };
    Mode mode = Mode::Fillet;
    double radius = 0.0;                    // Fillet radius or chamfer distance
    std::vector<std::string> edgeIds;       // ElementMap edge IDs
    bool chainTangentEdges = true;          // Auto-expand to tangent chain
};

struct ShellParams {
    double thickness = 0.0;
    std::vector<std::string> openFaceIds;   // ElementMap face IDs to remove
};

struct BooleanParams {
    enum class Op { Union, Cut, Intersect };
    Op operation = Op::Union;
    std::string targetBodyId;               // Body to modify (result)
    std::string toolBodyId;                 // Body to boolean with
};

struct LinearPatternParams {
    std::string sourceBodyId;               // Body to pattern
    double dirX = 1.0;                      // Direction vector
    double dirY = 0.0;
    double dirZ = 0.0;
    double spacing = 10.0;                  // Distance between instances
    int count = 2;                          // Total instances (including original)
    bool fuseResult = true;                 // Fuse all instances into one body
};

struct LoftParams {
    std::vector<std::string> profileSketchIds;  // Sketch IDs for each profile section
    std::vector<std::string> profileRegionIds;  // Region IDs for each section
    bool isSolid = true;                        // Solid vs surface result
    bool isRuled = false;                       // Ruled vs smooth interpolation
    BooleanMode booleanMode = BooleanMode::NewBody;
};

struct SweepParams {
    std::string profileSketchId;                // Profile sketch
    std::string profileRegionId;                // Profile region
    std::string pathSketchId;                   // Path sketch (must contain a single wire)
    std::string pathEdgeId;                     // Or a body edge as path
    BooleanMode booleanMode = BooleanMode::NewBody;
};

struct MirrorBodyParams {
    std::string sourceBodyId;
    double planePointX = 0.0;     // Point on mirror plane
    double planePointY = 0.0;
    double planePointZ = 0.0;
    double planeNormalX = 1.0;    // Mirror plane normal
    double planeNormalY = 0.0;
    double planeNormalZ = 0.0;
    bool fuseWithOriginal = false;
};

struct CircularPatternParams {
    std::string sourceBodyId;               // Body to pattern
    double axisX = 0.0;                     // Axis point
    double axisY = 0.0;
    double axisZ = 0.0;
    double axisDirX = 0.0;                  // Axis direction
    double axisDirY = 0.0;
    double axisDirZ = 1.0;
    double angleDeg = 360.0;               // Total angle
    int count = 4;                          // Total instances (including original)
    bool fuseResult = true;
};

// ─────────────────────────────────────────────────────────────────────────────
// Operation Params Variant
// ─────────────────────────────────────────────────────────────────────────────

using OperationParams = std::variant<
    ExtrudeParams,
    RevolveParams,
    FilletChamferParams,
    ShellParams,
    BooleanParams,
    LinearPatternParams,
    CircularPatternParams,
    LoftParams,
    SweepParams,
    MirrorBodyParams
>;

// ─────────────────────────────────────────────────────────────────────────────
// Operation Record (single history entry)
// ─────────────────────────────────────────────────────────────────────────────

struct OperationRecord {
    std::string opId;                           // Unique operation ID (UUID)
    OperationType type = OperationType::Extrude;
    OperationInput input;                       // Primary input (region, face, or body)
    OperationParams params;                     // Operation-specific parameters
    std::vector<std::string> resultBodyIds;     // Bodies produced/modified by this op
};

// ─────────────────────────────────────────────────────────────────────────────
// Utility Functions
// ─────────────────────────────────────────────────────────────────────────────

inline const char* operationTypeName(OperationType type) {
    switch (type) {
        case OperationType::Extrude: return "Extrude";
        case OperationType::Revolve: return "Revolve";
        case OperationType::Fillet: return "Fillet";
        case OperationType::Chamfer: return "Chamfer";
        case OperationType::Shell: return "Shell";
        case OperationType::Boolean: return "Boolean";
        case OperationType::LinearPattern: return "LinearPattern";
        case OperationType::CircularPattern: return "CircularPattern";
        case OperationType::Loft: return "Loft";
        case OperationType::Sweep: return "Sweep";
        case OperationType::MirrorBody: return "MirrorBody";
        default: return "Unknown";
    }
}

inline const char* extrudeModeName(ExtrudeMode mode) {
    switch (mode) {
        case ExtrudeMode::Blind: return "Blind";
        case ExtrudeMode::ThroughAll: return "ThroughAll";
        case ExtrudeMode::Symmetric: return "Symmetric";
        case ExtrudeMode::ToNext: return "ToNext";
        case ExtrudeMode::ToFace: return "ToFace";
        default: return "Unknown";
    }
}

inline const char* booleanModeName(BooleanMode mode) {
    switch (mode) {
        case BooleanMode::NewBody: return "NewBody";
        case BooleanMode::Add: return "Add";
        case BooleanMode::Cut: return "Cut";
        case BooleanMode::Intersect: return "Intersect";
        default: return "Unknown";
    }
}

inline const char* booleanOpName(BooleanParams::Op op) {
    switch (op) {
        case BooleanParams::Op::Union: return "Union";
        case BooleanParams::Op::Cut: return "Cut";
        case BooleanParams::Op::Intersect: return "Intersect";
        default: return "Unknown";
    }
}

} // namespace onecad::app

#endif // ONECAD_APP_DOCUMENT_OPERATIONRECORD_H
