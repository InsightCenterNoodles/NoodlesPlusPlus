#include "pointplot.h"

#include "utility.h"

#include <QDebug>

static std::vector<glm::vec3> const sphere_vertex_info = {
    { 0.000000, -1.250544, 0.000000 },   { 0.904894, -0.559262, 0.657436 },
    { -0.345632, -0.559262, 1.063763 },  { -1.118518, -0.559262, 0.000000 },
    { -0.345632, -0.559262, -1.063763 }, { 0.904894, -0.559262, -0.657436 },
    { 0.345632, 0.559262, 1.063763 },    { -0.904894, 0.559262, 0.657436 },
    { -0.904894, 0.559262, -0.657436 },  { 0.345632, 0.559262, -1.063763 },
    { 1.118518, 0.559262, 0.000000 },    { 0.000000, 1.250544, 0.000000 },
};

static std::vector<glm::vec3> const sphere_normal_info = []() {
    std::vector<glm::vec3> ret = sphere_vertex_info;

    for (auto& n : ret) {
        n = glm::normalize(n);
    }

    return ret;
}();

static std::vector<glm::u16vec3> const sphere_index_info = {
    { 0, 1, 2 },   { 1, 0, 5 },  { 0, 2, 3 },  { 0, 3, 4 },  { 0, 4, 5 },
    { 1, 5, 10 },  { 2, 1, 6 },  { 3, 2, 7 },  { 4, 3, 8 },  { 5, 4, 9 },
    { 1, 10, 6 },  { 2, 6, 7 },  { 3, 7, 8 },  { 4, 8, 9 },  { 5, 9, 10 },
    { 6, 10, 11 }, { 7, 6, 11 }, { 8, 7, 11 }, { 9, 8, 11 }, { 10, 9, 11 },
};

static auto build_common_sphere(noo::DocumentTPtr doc,
                                noo::TableTPtr    table = {}) {
    noo::MaterialData mat;
    mat.color        = { 1, 1, 1, 1 };
    mat.metallic     = 0;
    mat.roughness    = 1;
    mat.use_blending = false;

    auto mat_ptr = create_material(doc, mat);

    noo::BufferMeshDataRef mesh_data;

    mesh_data.positions = sphere_vertex_info;
    mesh_data.normals   = sphere_normal_info;
    mesh_data.triangles = sphere_index_info;

    auto mesh = create_mesh(doc, mesh_data);

    noo::ObjectData object_data;
    object_data.material  = mat_ptr;
    object_data.transform = glm::mat4(1);
    object_data.mesh      = mesh;

    if (table) { object_data.tables.push_back(table); }

    auto obj = create_object(doc, object_data);

    return std::make_tuple(mat_ptr, mesh, obj);
}


void PointPlot::rebuild_instances(size_t from, size_t count) {
    qDebug() << Q_FUNC_INFO << "ask" << from << count;
    from  = std::clamp<size_t>(from, 0, m_points.size());
    count = std::clamp<size_t>(count, 0, m_points.size() - from);
    qDebug() << Q_FUNC_INFO << "resolved" << from << count;

    if (count == 0) {
        m_instances.clear();

        noo::ObjectUpdateData up;
        up.instances = m_instances;

        noo::update_object(m_obj, up);
        return;
    }

    assert(from < m_points.size());
    assert(count + from <= m_points.size());

    auto num_rows = m_points.size();

    m_instances.resize(num_rows);

    if (m_instances.empty()) return;

    glm::vec3 default_col(1);
    glm::vec3 default_scale(1);


    std::span<glm::vec3> col_array =
        m_colors.size() ? std::span<glm::vec3>(m_colors)
                        : std::span<glm::vec3>(&default_col, 1);

    std::span<glm::vec3> scale_array =
        m_scales.size() ? std::span<glm::vec3>(m_scales)
                        : std::span<glm::vec3>(&default_scale, 1);

    auto d = m_host->domain()->current_domain();

    for (size_t i = from; i < (from + count); i++) {
        glm::mat4& m = m_instances[i];

        m[0] = glm::vec4(d.transform(m_points[i]), 1);
        m[1] = glm::vec4(col_array[i % col_array.size()], 0);
        m[2] = glm::vec4(0, 0, 0, 1);
        m[3] = glm::vec4(scale_array[i % scale_array.size()], 1);
    }

    noo::ObjectUpdateData up;
    up.instances = m_instances;

    noo::update_object(m_obj, up);
}

PointPlot::PointPlot(Plotty&                  host,
                     int64_t                  id,
                     std::vector<glm::vec3>&& points,
                     std::vector<glm::vec3>&& colors,
                     std::vector<glm::vec3>&& scales)
    : Plot(host, id),
      m_points(std::move(points)),
      m_colors(std::move(colors)),
      m_scales(std::move(scales)) {

    auto [pmat, pmesh, pobj] = build_common_sphere(m_doc);

    m_mat  = pmat;
    m_mesh = pmesh;
    m_obj  = pobj;

    if (m_points.size()) {
        auto [l, h] = min_max_of(m_points);
        host.domain()->ask_update_input_bounds(l, h);
    }

    rebuild_instances();
}

PointPlot::~PointPlot() { }

void PointPlot::append(std::span<glm::vec3> points,
                       std::span<glm::vec3> colors,
                       std::span<glm::vec3> scales) {

    if (points.empty()) return;


    {
        auto [l, h] = min_max_of(points);
        m_host->domain()->ask_update_input_bounds(l, h);
    }

    m_points.insert(m_points.end(), points.begin(), points.end());
    m_colors.insert(m_colors.end(), colors.begin(), colors.end());
    m_scales.insert(m_scales.end(), scales.begin(), scales.end());

    rebuild_instances();
}

inline void replace(std::span<glm::vec3> source, std::vector<glm::vec3>& dest) {
    dest.resize(source.size());

    std::copy(source.begin(), source.end(), dest.begin());
}

void PointPlot::replace_with(std::span<glm::vec3> points,
                             std::span<glm::vec3> colors,
                             std::span<glm::vec3> scales) {

    replace(points, m_points);
    replace(colors, m_colors);
    replace(scales, m_scales);

    if (m_points.size()) {
        auto [l, h] = min_max_of(m_points);
        m_host->domain()->ask_update_input_bounds(l, h);
    }

    rebuild_instances();
}

void PointPlot::domain_updated(Domain const&) {
    rebuild_instances();
}
