#ifndef QT3DEXTRAS_QMETALROUGHMATERIAL_H
#define QT3DEXTRAS_QMETALROUGHMATERIAL_H

#include <Qt3DExtras/qt3dextras_global.h>
#include <Qt3DRender/qmaterial.h>
#include <QtGui/qcolor.h>

#include <QShaderProgramBuilder>
#include <QTechnique>

namespace Qt3DRender {
class QAbstractTexture;
}

class QInstancedMetalRoughMaterial : public Qt3DRender::QMaterial {
    Q_OBJECT

    Qt3DRender::QParameter*            m_baseColorParameter;
    Qt3DRender::QParameter*            m_metalnessParameter;
    Qt3DRender::QParameter*            m_roughnessParameter;
    Qt3DRender::QParameter*            m_baseColorMapParameter;
    Qt3DRender::QParameter*            m_metalnessMapParameter;
    Qt3DRender::QParameter*            m_roughnessMapParameter;
    Qt3DRender::QParameter*            m_ambientOcclusionMapParameter;
    Qt3DRender::QParameter*            m_normalMapParameter;
    Qt3DRender::QParameter*            m_textureScaleParameter;
    Qt3DRender::QEffect*               m_metalRoughEffect;
    Qt3DRender::QTechnique*            m_metalRoughGL3Technique;
    Qt3DRender::QRenderPass*           m_metalRoughGL3RenderPass;
    Qt3DRender::QShaderProgram*        m_metalRoughGL3Shader;
    Qt3DRender::QShaderProgramBuilder* m_metalRoughGL3ShaderBuilder;
    Qt3DRender::QTechnique*            m_metalRoughES3Technique;
    Qt3DRender::QRenderPass*           m_metalRoughES3RenderPass;
    Qt3DRender::QShaderProgram*        m_metalRoughES3Shader;
    Qt3DRender::QShaderProgramBuilder* m_metalRoughES3ShaderBuilder;
    Qt3DRender::QTechnique*            m_metalRoughRHITechnique;
    Qt3DRender::QRenderPass*           m_metalRoughRHIRenderPass;
    Qt3DRender::QShaderProgram*        m_metalRoughRHIShader;
    Qt3DRender::QShaderProgramBuilder* m_metalRoughRHIShaderBuilder;
    Qt3DRender::QFilterKey*            m_filterKey;


    Q_PROPERTY(QVariant baseColor READ baseColor WRITE setBaseColor NOTIFY
                   baseColorChanged)
    Q_PROPERTY(QVariant metalness READ metalness WRITE setMetalness NOTIFY
                   metalnessChanged)
    Q_PROPERTY(QVariant roughness READ roughness WRITE setRoughness NOTIFY
                   roughnessChanged)
    Q_PROPERTY(
        QVariant ambientOcclusion READ ambientOcclusion WRITE
            setAmbientOcclusion NOTIFY ambientOcclusionChanged REVISION 10)
    Q_PROPERTY(QVariant normal READ normal WRITE setNormal NOTIFY normalChanged
                   REVISION 10)
    Q_PROPERTY(float textureScale READ textureScale WRITE setTextureScale NOTIFY
                   textureScaleChanged REVISION 10)

public:
    explicit QInstancedMetalRoughMaterial(Qt3DCore::QNode* parent = nullptr);
    ~QInstancedMetalRoughMaterial();

    QVariant baseColor() const;
    QVariant metalness() const;
    QVariant roughness() const;
    QVariant ambientOcclusion() const;
    QVariant normal() const;
    float    textureScale() const;

public slots:
    void setBaseColor(const QVariant& baseColor);
    void setMetalness(const QVariant& metalness);
    void setRoughness(const QVariant& roughness);
    void setAmbientOcclusion(const QVariant& ambientOcclusion);
    void setNormal(const QVariant& normal);
    void setTextureScale(float textureScale);

private slots:
    void handleTextureScaleChanged(const QVariant& var);

signals:
    void baseColorChanged(const QVariant& baseColor);
    void metalnessChanged(const QVariant& metalness);
    void roughnessChanged(const QVariant& roughness);
    void ambientOcclusionChanged(const QVariant& ambientOcclusion);
    void normalChanged(const QVariant& normal);
    void textureScaleChanged(float textureScale);
};

#endif // QT3DEXTRAS_QMETALROUGHMATERIAL_H
