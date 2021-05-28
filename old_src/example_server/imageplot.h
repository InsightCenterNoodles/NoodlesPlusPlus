#ifndef IMAGEPLOT_H
#define IMAGEPLOT_H

#include "plot.h"

class ImagePlot : public Plot {

    std::vector<std::byte> m_image_data;

    glm::vec3 m_top_left;
    glm::vec3 m_bottom_left;
    glm::vec3 m_bottom_right;

    noo::TextureTPtr  m_image_texture;
    noo::MaterialTPtr m_image_mat;
    noo::ObjectTPtr   m_obj;

    void rebuild(Domain const&);

public:
    ImagePlot(Plotty&                host,
              int64_t                id,
              std::vector<std::byte> image_data,
              glm::vec3              top_left,
              glm::vec3              bottom_left,
              glm::vec3              bottom_right);

    ~ImagePlot() override;

    void domain_updated(Domain const&) override;
};

#endif // IMAGEPLOT_H
