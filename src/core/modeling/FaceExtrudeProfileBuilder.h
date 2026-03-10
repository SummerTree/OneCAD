/**
 * @file FaceExtrudeProfileBuilder.h
 * @brief Builds a single merged planar profile for face push/pull extrusion.
 */
#ifndef ONECAD_CORE_MODELING_FACEEXTRUDEPROFILEBUILDER_H
#define ONECAD_CORE_MODELING_FACEEXTRUDEPROFILEBUILDER_H

#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace onecad::core::modeling {

class FaceExtrudeProfileBuilder {
public:
    struct Options {
        double normalDotTolerance = 0.9999;
        double planeDistanceTolerance = 1e-3;
        double pointClassifyTolerance = 1e-4;
    };

    struct Result {
        TopoDS_Shape profileShape;
        std::size_t inputFaceCount = 0;
        std::size_t mergedFaceCount = 0;
    };

    /**
     * @brief Build one merged planar profile from a connected coplanar patch.
     *
     * Returns nullopt and populates errorOut on failure. Partial fallback is not allowed.
     */
    static std::optional<Result> build(const TopoDS_Face& seedFace,
                                       const std::vector<TopoDS_Face>& patchFaces,
                                       std::string& errorOut);

    /**
     * @brief Build one merged planar profile with explicit options.
     */
    static std::optional<Result> build(const TopoDS_Face& seedFace,
                                       const std::vector<TopoDS_Face>& patchFaces,
                                       std::string& errorOut,
                                       const Options& options);
};

} // namespace onecad::core::modeling

#endif // ONECAD_CORE_MODELING_FACEEXTRUDEPROFILEBUILDER_H
