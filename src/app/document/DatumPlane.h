/**
 * @file DatumPlane.h
 * @brief User-created reference (datum) planes usable as sketch planes.
 *
 * A datum plane is defined parametrically (offset/angle relative to a base plane,
 * or offset from a model face) and carries a cached resolved frame. Sketches placed
 * on a datum copy the resolved frame at creation (frozen, like sketch-on-face).
 */
#ifndef ONECAD_APP_DOCUMENT_DATUMPLANE_H
#define ONECAD_APP_DOCUMENT_DATUMPLANE_H

#include "../../core/sketch/Sketch.h"  // core::sketch::SketchPlane

#include <string>

namespace onecad::app {

struct DatumPlane {
    enum class Kind {
        OffsetFromPlane,  ///< Offset along a base origin/datum plane normal
        OffsetFromFace,   ///< Offset from a model face (frozen snapshot)
        AngledFromEdge,   ///< Rotate a base plane about an edge by angleDeg
        ThreePoint        ///< Frame from three points (reserved)
    };

    std::string id;
    std::string name;
    Kind kind = Kind::OffsetFromPlane;

    // Definition (parametric, re-derivable):
    std::string basePlaneId;  ///< "XY"/"XZ"/"YZ" or another datum id (Offset/Angled)
    std::string baseBodyId;   ///< Owner body of baseFaceId (OffsetFromFace)
    std::string baseFaceId;   ///< ElementMap face id (OffsetFromFace), frozen
    std::string axisEdgeId;   ///< ElementMap edge id (AngledFromEdge)
    double offset = 0.0;
    double angleDeg = 0.0;

    // Cached resolved frame (source of truth for sketches placed on this datum).
    core::sketch::SketchPlane resolvedPlane;
    bool resolvedValid = false;
};

inline const char* datumPlaneKindName(DatumPlane::Kind kind) {
    switch (kind) {
        case DatumPlane::Kind::OffsetFromPlane: return "OffsetFromPlane";
        case DatumPlane::Kind::OffsetFromFace:  return "OffsetFromFace";
        case DatumPlane::Kind::AngledFromEdge:  return "AngledFromEdge";
        case DatumPlane::Kind::ThreePoint:      return "ThreePoint";
        default:                                return "Unknown";
    }
}

inline DatumPlane::Kind datumPlaneKindFromName(const std::string& name) {
    if (name == "OffsetFromFace") return DatumPlane::Kind::OffsetFromFace;
    if (name == "AngledFromEdge") return DatumPlane::Kind::AngledFromEdge;
    if (name == "ThreePoint")     return DatumPlane::Kind::ThreePoint;
    return DatumPlane::Kind::OffsetFromPlane;
}

} // namespace onecad::app

#endif // ONECAD_APP_DOCUMENT_DATUMPLANE_H
