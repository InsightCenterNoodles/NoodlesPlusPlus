#include "imageplot.h"

#include "plotty.h"

void ImagePlot::rebuild(Domain const& d) {
    auto center = (m_top_left + m_bottom_right) / 2.0f;

    auto o = -(m_bottom_left - center);

    if (!m_image_texture) {
        m_image_texture = noo::create_texture_from_file(m_doc, m_image_data);
    }

    if (!m_image_mat) {
        noo::MaterialData mat;

        mat.color     = { 1, 1, 1, 1 };
        mat.metallic  = 0;
        mat.roughness = 1;
        mat.texture   = m_image_texture;

        m_image_mat = noo::create_material(m_doc, mat);
    }

    noo::MeshTPtr mesh;

    {
        // create a plane

        std::array<glm::vec3, 4> positions = {
            m_top_left, m_bottom_left, m_bottom_right, o
        };

        for (auto& p : positions) {
            p = d.transform(p);
        }

        std::array<glm::vec3, 4> normals;

        for (auto& n : normals) {
            n = glm::cross(m_top_left - m_bottom_left,
                           m_bottom_right - m_bottom_left);
            n = glm::normalize(n);
        }

        std::array<glm::u16vec3, 2> index = {
            glm::u16vec3 { 0, 1, 2 },
            glm::u16vec3 { 3, 1, 2 },
        };

        noo::BufferMeshDataRef ref;

        ref.positions = positions;
        ref.normals   = normals;
        ref.triangles = index;

        std::vector<std::byte> mesh_data;

        auto result = noo::pack_mesh_to_vector(ref, mesh_data);

        noo::BufferCopySource buffer_data;
        buffer_data.to_copy = mesh_data;

        auto buffer_ptr = noo::create_buffer(m_doc, buffer_data);

        noo::MeshData noo_mesh_data(result, buffer_ptr);

        mesh = create_mesh(m_doc, noo_mesh_data);
    }


    noo::ObjectData object_data;
    object_data.material  = m_image_mat;
    object_data.transform = glm::mat4(1);
    object_data.mesh      = mesh;

    m_obj = create_object(m_doc, object_data);
}

ImagePlot::ImagePlot(Plotty&                host,
                     int64_t                id,
                     std::vector<std::byte> image_data,
                     glm::vec3              top_left,
                     glm::vec3              bottom_left,
                     glm::vec3              bottom_right)
    : Plot(host, id),
      m_image_data(std::move(image_data)),
      m_top_left(top_left),
      m_bottom_left(bottom_left),
      m_bottom_right(bottom_right) {
    rebuild(host.domain()->current_domain());
}

ImagePlot::~ImagePlot() { }

void ImagePlot::domain_updated(Domain const& d) {
    rebuild(d);
}
