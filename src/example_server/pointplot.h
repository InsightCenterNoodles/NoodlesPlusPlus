#ifndef POINTPLOT_H
#define POINTPLOT_H

#include "plot.h"
#include "plotty.h"

class PointPlot : public Plot {
protected:
    std::vector<glm::vec3> m_points;
    std::vector<glm::vec3> m_colors;
    std::vector<glm::vec3> m_scales;

    std::vector<glm::mat4> m_instances;


    void rebuild_instances(size_t from = 0, size_t count = -1);

public:
    PointPlot(Plotty&                  host,
              int64_t                  id,
              std::vector<glm::vec3>&& points,
              std::vector<glm::vec3>&& colors,
              std::vector<glm::vec3>&& scales);
    ~PointPlot() override;

    void replace_with(std::span<glm::vec3> points,
                      std::span<glm::vec3> colors,
                      std::span<glm::vec3> scales);

    void append(std::span<glm::vec3> points,
                std::span<glm::vec3> colors,
                std::span<glm::vec3> scales);

    void domain_updated(Domain const&) override;
};
#endif // POINTPLOT_H
