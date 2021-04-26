#include "linesegmentplot.h"

#include "utility.h"

static std::vector<glm::vec3> const tube_vertex_info = {
    { -0.500000, 0.000000, -1.000000 }, { 0.500000, 0.000000, -1.000000 },
    { 0.500000, -0.866025, -0.500000 }, { -0.500000, -0.866026, -0.500000 },
    { 0.500000, -0.866025, 0.500000 },  { -0.500000, -0.866026, 0.500000 },
    { 0.500000, -0.000000, 1.000000 },  { -0.500000, -0.000000, 1.000000 },
    { 0.500000, 0.866026, 0.500000 },   { -0.500000, 0.866025, 0.500000 },
    { 0.500000, 0.866026, -0.500000 },  { -0.500000, 0.866025, -0.500000 },
};

static std::vector<glm::vec3> const tube_normal_info = {
    { 0.0000, 0.0000, -1.0000 },  { 0.0000, -0.5000, -0.8660 },
    { 0.0000, -1.0000, -0.0000 }, { 0.0000, -0.8660, 0.5000 },
    { 0.0000, 0.0000, 1.0000 },   { -0.0000, 0.8660, 0.5000 },
    { -0.0000, 0.8660, -0.5000 },
};

static std::vector<std::array<int, 6>> tube_face_info = {
    { 1, 1, 3, 2, 4, 2 },  { 3, 3, 6, 4, 4, 3 },   { 5, 4, 8, 5, 6, 4 },
    { 7, 5, 10, 6, 8, 5 }, { 9, 6, 12, 7, 10, 6 }, { 12, 7, 2, 1, 1, 1 },
    { 1, 1, 2, 1, 3, 2 },  { 3, 3, 5, 4, 6, 4 },   { 5, 4, 7, 5, 8, 5 },
    { 7, 5, 9, 6, 10, 6 }, { 9, 6, 11, 7, 12, 7 }, { 12, 7, 11, 7, 2, 1 },
};

static auto build_common_tube(noo::DocumentTPtr doc) {

    std::vector<glm::vec3>    ns;
    std::vector<glm::u16vec3> fs;

    ns.resize(tube_vertex_info.size());
    fs.reserve(tube_face_info.size());

    for (auto const& a : tube_face_info) {
        auto va = a[0] - 1;
        auto na = a[1] - 1;

        auto vb = a[2] - 1;
        auto nb = a[3] - 1;

        auto vc = a[4] - 1;
        auto nc = a[5] - 1;

        ns[va] = tube_normal_info[na];
        ns[vb] = tube_normal_info[nb];
        ns[vc] = tube_normal_info[nc];

        fs.push_back({ va, vb, vc });
    }

    noo::MaterialData mat;
    mat.color        = { 1, 1, 1, 1 };
    mat.metallic     = 0;
    mat.roughness    = 1;
    mat.use_blending = false;

    auto mat_ptr = create_material(doc, mat);

    noo::BufferMeshDataRef mesh_data;

    mesh_data.positions = tube_vertex_info;
    mesh_data.normals   = ns;
    mesh_data.triangles = fs;

    auto mesh = create_mesh(doc, mesh_data);

    noo::ObjectData object_data;
    object_data.material  = mat_ptr;
    object_data.transform = glm::mat4(1);
    object_data.mesh      = mesh;

    auto obj = create_object(doc, object_data);

    return std::make_tuple(mat_ptr, mesh, obj);
}

void LineSegmentPlot::rebuild_instances() {
    auto num_rows = m_points.size() / 2;

    m_instances.resize(num_rows);

    if (m_instances.empty()) return;

    glm::vec3 default_col(1);
    glm::vec2 default_scale(1);


    std::span<glm::vec3> col_array =
        m_colors.size() ? std::span<glm::vec3>(m_colors)
                        : std::span<glm::vec3>(&default_col, 1);

    std::span<glm::vec2> scale_array =
        m_scales.size() ? std::span<glm::vec2>(m_scales)
                        : std::span<glm::vec2>(&default_scale, 1);

    auto d = m_host->domain()->current_domain();


    for (size_t i = 0; i < num_rows; i++) {
        glm::mat4& m = m_instances[i];

        m[0] = glm::vec4(d.transform(m_points[i]), 1);
        m[1] = glm::vec4(col_array[i % col_array.size()], 0);
        m[2] = glm::vec4(0, 0, 0, 1);
        m[3] = glm::vec4(scale_array[i % scale_array.size()], 1, 1);
    }

    noo::ObjectUpdateData up;
    up.instances = m_instances;

    noo::update_object(m_obj, up);
}


LineSegmentPlot::LineSegmentPlot(Plotty&                  host,
                                 int64_t                  id,
                                 std::vector<glm::vec3>&& points,
                                 std::vector<glm::vec3>&& colors,
                                 std::vector<glm::vec2>&& scales)
    : Plot(host, id),
      m_points(std::move(points)),
      m_colors(std::move(colors)),
      m_scales(std::move(scales)) {

    auto [pmat, pmesh, pobj] = build_common_tube(m_doc);

    m_mat  = pmat;
    m_mesh = pmesh;
    m_obj  = pobj;

    {
        auto [l, h] = min_max_of(points);

        host.domain()->ask_update_input_bounds(l, h);
    }

    rebuild_instances();
}

LineSegmentPlot::~LineSegmentPlot() { }

void LineSegmentPlot::domain_updated(Domain const&) {
    rebuild_instances();
}
