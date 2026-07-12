#include "MassPropertiesPanel.h"

#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Shape.hxx>

namespace onecad::ui {

MassPropertiesPanel::MassPropertiesPanel(QWidget* parent)
    : QWidget(parent) {
    auto* layout = new QFormLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);

    m_nameLabel = new QLabel(QStringLiteral("-"));
    m_volumeLabel = new QLabel(QStringLiteral("-"));
    m_surfaceAreaLabel = new QLabel(QStringLiteral("-"));
    m_centerOfMassLabel = new QLabel(QStringLiteral("-"));
    m_boundingBoxLabel = new QLabel(QStringLiteral("-"));
    m_faceCountLabel = new QLabel(QStringLiteral("-"));
    m_edgeCountLabel = new QLabel(QStringLiteral("-"));
    m_vertexCountLabel = new QLabel(QStringLiteral("-"));

    layout->addRow(tr("Body:"), m_nameLabel);
    layout->addRow(tr("Volume:"), m_volumeLabel);
    layout->addRow(tr("Surface Area:"), m_surfaceAreaLabel);
    layout->addRow(tr("Center of Mass:"), m_centerOfMassLabel);
    layout->addRow(tr("Bounding Box:"), m_boundingBoxLabel);
    layout->addRow(tr("Faces:"), m_faceCountLabel);
    layout->addRow(tr("Edges:"), m_edgeCountLabel);
    layout->addRow(tr("Vertices:"), m_vertexCountLabel);
}

void MassPropertiesPanel::updateForShape(const TopoDS_Shape& shape,
                                          const QString& bodyName) {
    if (shape.IsNull()) {
        clear();
        return;
    }

    m_nameLabel->setText(bodyName);

    // Topology counts (cheap traversals) \u2014 also used to gate the heavy integrals.
    int faces = 0, edges = 0, vertices = 0;
    for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next()) {
        ++faces;
    }
    for (TopExp_Explorer exp(shape, TopAbs_EDGE); exp.More(); exp.Next()) {
        ++edges;
    }
    for (TopExp_Explorer exp(shape, TopAbs_VERTEX); exp.More(); exp.Next()) {
        ++vertices;
    }
    m_faceCountLabel->setText(QString::number(faces));
    m_edgeCountLabel->setText(QString::number(edges));
    m_vertexCountLabel->setText(QString::number(vertices));

    // Bounding box (cheap).
    Bnd_Box bbox;
    BRepBndLib::Add(shape, bbox);
    if (!bbox.IsVoid()) {
        double xmin, ymin, zmin, xmax, ymax, zmax;
        bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);
        m_boundingBoxLabel->setText(
            QString("%1 x %2 x %3 mm")
                .arg(xmax - xmin, 0, 'f', 2)
                .arg(ymax - ymin, 0, 'f', 2)
                .arg(zmax - zmin, 0, 'f', 2));
    } else {
        m_boundingBoxLabel->setText(QStringLiteral("-"));
    }

    // The volume/surface-area integrals dominate cost; skip them for very large
    // models so interactive selection stays responsive.
    if (faces > m_heavyComputeFaceLimit) {
        const QString skipped = tr("\u2014 (large model)");
        m_volumeLabel->setText(skipped);
        m_surfaceAreaLabel->setText(skipped);
        m_centerOfMassLabel->setText(QStringLiteral("-"));
        return;
    }

    // Volume properties
    GProp_GProps volumeProps;
    BRepGProp::VolumeProperties(shape, volumeProps);
    double volume = volumeProps.Mass();
    m_volumeLabel->setText(QString::number(volume, 'f', 4) + " mm\u00B3");

    gp_Pnt com = volumeProps.CentreOfMass();
    m_centerOfMassLabel->setText(
        QString("(%1, %2, %3)")
            .arg(com.X(), 0, 'f', 3)
            .arg(com.Y(), 0, 'f', 3)
            .arg(com.Z(), 0, 'f', 3));

    // Surface properties
    GProp_GProps surfaceProps;
    BRepGProp::SurfaceProperties(shape, surfaceProps);
    double surfaceArea = surfaceProps.Mass();
    m_surfaceAreaLabel->setText(QString::number(surfaceArea, 'f', 4) + " mm\u00B2");
}

void MassPropertiesPanel::clear() {
    m_nameLabel->setText(QStringLiteral("-"));
    m_volumeLabel->setText(QStringLiteral("-"));
    m_surfaceAreaLabel->setText(QStringLiteral("-"));
    m_centerOfMassLabel->setText(QStringLiteral("-"));
    m_boundingBoxLabel->setText(QStringLiteral("-"));
    m_faceCountLabel->setText(QStringLiteral("-"));
    m_edgeCountLabel->setText(QStringLiteral("-"));
    m_vertexCountLabel->setText(QStringLiteral("-"));
}

} // namespace onecad::ui
