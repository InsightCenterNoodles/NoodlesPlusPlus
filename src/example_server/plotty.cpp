#include "plotty.h"

#include "imageplot.h"
#include "linesegmentplot.h"
#include "pointplot.h"
#include "simpletable.h"
#include "tableplot.h"

#include "../shared/variant_tools.h"

#include <glm/gtx/quaternion.hpp>

#include <QColor>
#include <QDebug>

struct PositionListArgument {
    std::vector<glm::vec3> positions;

    PositionListArgument() = default;
    PositionListArgument(noo::AnyVar&& av) {
        auto reals = av.coerce_real_list();

        if (reals.size()) {

            positions.resize(reals.size() / 3);

            for (size_t i = 0; i < positions.size(); i += 3) {
                positions[i] = { reals[i + 0], reals[i + 1], reals[i + 2] };
            }
            return;
        }
    }
};

struct ColorListArgument {
    std::vector<glm::vec3> colors;

    ColorListArgument() = default;
    ColorListArgument(noo::AnyVar&& av) {
        auto reals = av.coerce_real_list();

        if (reals.size()) {

            colors.resize(reals.size() / 3);

            for (size_t i = 0; i < colors.size(); i += 3) {
                colors[i] = { reals[i + 0], reals[i + 1], reals[i + 2] };
            }
            return;
        }

        // not real list. string list?

        if (av.has_list()) {
            auto raw_colors = av.steal_vector();

            colors.reserve(raw_colors.size());

            for (auto& raw_c : raw_colors) {
                auto str = raw_c.steal_string();

                auto c = QColor(str.c_str());

                colors.emplace_back(c.redF(), c.greenF(), c.blueF());
            }
        }
    }
};

struct Scale3DListArgument {
    std::vector<glm::vec3> scales;

    Scale3DListArgument() = default;
    Scale3DListArgument(noo::AnyVar&& av) {
        auto reals = av.coerce_real_list();

        if (reals.size()) {

            scales.resize(reals.size() / 3);

            for (size_t i = 0; i < scales.size(); i += 3) {
                scales[i] = { reals[i + 0], reals[i + 1], reals[i + 2] };
            }
            return;
        }
    }
};

struct Scale2DListArgument {
    std::vector<glm::vec2> scales;

    Scale2DListArgument() = default;
    Scale2DListArgument(noo::AnyVar&& av) {
        auto reals = av.coerce_real_list();

        if (reals.size()) {

            scales.resize(reals.size() / 2);

            for (size_t i = 0; i < scales.size(); i += 2) {
                scales[i] = { reals[i + 0], reals[i + 1] };
            }
            return;
        }
    }
};


// Domain ======================================================================

glm::vec3 Domain::transform(glm::vec3 a) const {
    return lerp(a, input_min, input_max, output_min, output_max);
}

SharedDomain::SharedDomain(QObject* p) : QObject(p) { }

Domain const& SharedDomain::current_domain() const {
    return m_current_domain;
}
bool SharedDomain::domain_auto_updates() const {
    return m_domain_auto;
}

void SharedDomain::set_domain_auto_updates(bool b) {
    m_domain_auto = b;
}

void SharedDomain::ask_set_domain(Domain d) {
    if (!m_domain_auto) return;
    m_current_domain = d;
    emit domain_updated();
}

void SharedDomain::ask_update_input_bounds(glm::vec3 l, glm::vec3 h) {
    if (!m_domain_auto) return;

    qDebug() << "Bounds updated" << l.x << l.y << l.z << h.x << h.y << h.z;

    m_current_domain.input_min = l;
    m_current_domain.input_max = h;

    emit domain_updated();
}

void SharedDomain::set_axis_labels(std::string x_axis_title,
                                   std::string y_axis_title,
                                   std::string z_axis_title) {
    m_x_axis_title = std::move(x_axis_title);
    m_y_axis_title = std::move(y_axis_title);
    m_z_axis_title = std::move(z_axis_title);
    emit labels_updated();
}

std::string SharedDomain::x_axis_title() const {
    return m_x_axis_title;
}
std::string SharedDomain::y_axis_title() const {
    return m_y_axis_title;
}
std::string SharedDomain::z_axis_title() const {
    return m_z_axis_title;
}

// =============================================================================

void Plotty::make_box() {

    auto d = m_shared_domain->current_domain();

    auto select = [d](float x, float y, float z) {
        return glm::mix(d.output_min, d.output_max, glm::vec3(x, y, z));
    };

    std::array<glm::vec3, 8> const box_p = {
        select(0, 0, 0), select(0, 0, 1), select(0, 1, 0), select(0, 1, 1),
        select(1, 0, 0), select(1, 0, 1), select(1, 1, 0), select(1, 1, 1),
    };

    static std::vector<glm::u16vec2> const box_i = {
        { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 }, { 1, 3 },
        { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
    };


    if (!m_box_mesh) {
        noo::BufferMeshDataRef md;

        md.positions = box_p;
        md.lines     = box_i;

        m_box_mesh = noo::create_mesh(m_doc, md);
    }

    if (!m_box_mat) {
        noo::MaterialData md;
        md.color = { 1, 1, 1, 1 };

        m_box_mat = noo::create_material(m_doc, md);
    }


    noo::ObjectData nd;
    nd.parent   = plot_root();
    nd.material = m_box_mat;
    nd.mesh     = m_box_mesh;


    m_box = noo::create_object(m_doc, nd);
}

void Plotty::rebuild_axis() {

    m_axis_x = nullptr;
    m_axis_y = nullptr;
    m_axis_z = nullptr;


    auto make_axis =
        [&](std::string const& t, glm::vec3 pos, glm::vec3 orientation) {
            auto rot =
                glm::rotation(glm::vec3(1, 0, 0), glm::normalize(orientation));

            auto mat = glm::mat4_cast(rot);

            auto tf = glm::translate(glm::mat4(1), pos) * mat;

            noo::ObjectTextDefinition otd;
            otd.font   = "Arial";
            otd.text   = t;
            otd.height = .05;

            noo::ObjectData nd;
            nd.parent    = plot_root();
            nd.transform = tf;
            nd.text      = otd;


            return noo::create_object(m_doc, nd);
        };

    m_axis_x =
        make_axis(m_shared_domain->x_axis_title(), { .5, 0, 0 }, { 0, 1, 1 });
    m_axis_y =
        make_axis(m_shared_domain->y_axis_title(), { 0, .5, 0 }, { 1, 0, 1 });
    m_axis_z =
        make_axis(m_shared_domain->z_axis_title(), { 0, 0, .5 }, { 1, 1, 0 });
}

// Set domain ==================================================================

struct ForceToGLMVec3 {
    glm::vec3 v;

    ForceToGLMVec3() = default;
    ForceToGLMVec3(noo::AnyVar&& rv) {
        auto l = rv.coerce_real_list();

        v.x = noo::get_or_default(l, 0);
        v.y = noo::get_or_default(l, 1);
        v.z = noo::get_or_default(l, 2);
    }
};

auto make_set_domain_method(Plotty& p) {
    noo::MethodData m;
    m.method_name            = "set_domain";
    m.documentation          = "Set table bounds";
    m.argument_documentation = {
        { "[ real ] ", "3-tuple input bb min" },
        { "[ real ] ", "3-tuple input bb max" },
        { "[ real ] ", "3-tuple output bb min" },
        { "[ real ] ", "3-tuple output bb max" },
        { "[ string ] ", "3-tuple strings for axis names" },
    };
    m.return_documentation = "An integer plot id";

    m.set_code([&p](noo::MethodContext const&,
                    ForceToGLMVec3     input_min,
                    ForceToGLMVec3     input_max,
                    ForceToGLMVec3     output_min,
                    ForceToGLMVec3     output_max,
                    noo::StringListArg strings) {
        // auto clist = args.steal_vector();

        std::vector<std::string> axis_names;

        Domain d;
        d.input_min  = input_min.v;
        d.input_max  = input_max.v;
        d.output_min = output_min.v;
        d.output_max = output_max.v;

        p.domain()->set_domain_auto_updates(false);
        p.domain()->ask_set_domain(d);
        p.domain()->set_axis_labels(noo::get_or_default(strings.list, 0),
                                    noo::get_or_default(strings.list, 1),
                                    noo::get_or_default(strings.list, 2));

        return noo::AnyVar {};
    });

    return noo::create_method(p.document().get(), m);
}

// Add table ===================================================================

auto make_load_table_method(Plotty& p) {
    noo::MethodData m;
    m.method_name            = "load_table";
    m.documentation          = "Load a new table";
    m.argument_documentation = {
        { "string", "Name of the new table." },
        { "{ \"c\": [ reals ], ... }",
          "Columns; each key in the object is the column name, and maps to a "
          "list of data (reals). As an alternative, this argument can be [ "
          "{\"name\" : \"str\", \"data\" : [ reals ]} ]." },
    };
    m.return_documentation = "An integer plot id";

    struct LoadTableColumn {
        std::string         name;
        std::vector<double> reals;

        LoadTableColumn() = default;

        LoadTableColumn(std::string s, std::vector<double>&& r)
            : name(std::move(s)), reals(std::move(r)) { }

        LoadTableColumn(noo::AnyVar&& var) {
            auto obj = var.steal_map();

            name  = noo::steal_or_default(obj, "name").steal_string();
            reals = noo::steal_or_default(obj, "data").coerce_real_list();
        }
    };

    struct Columns {
        std::vector<LoadTableColumn> cols;

        Columns(noo::AnyVar&& var) {
            VMATCH(
                var,
                VCASE(noo::AnyVarList & l) {
                    for (auto& pack : l) {
                        cols.emplace_back(std::move(pack));
                    }
                },
                VCASE(noo::AnyVarMap & m) {
                    for (auto& [k, v] : m) {
                        cols.emplace_back(k, v.coerce_real_list());
                    }
                },
                VCASE(auto&) {
                    auto s = var.dump_string();
                    qDebug() << "Unknown argument" << s.c_str();
                })
        }
    };

    m.set_code([&p](noo::MethodContext const&, std::string name, Columns pack) {
        qDebug() << "Asking for new table" << name.c_str();
        if (pack.cols.size() == 0) {
            throw noo::MethodException("No columns given. Check input format.");
        }

        TableStorage storage;

        storage.name = name;

        storage.header.reserve(pack.cols.size());
        storage.data.reserve(pack.cols.size());

        for (auto& c : pack.cols) {
            storage.header.emplace_back(c.name);
            storage.data.emplace_back(std::move(c.reals));
        }

        auto table = std::make_shared<SimpleTable>(std::move(storage));

        return p.append<TablePlot>(-1, table);
    });

    return noo::create_method(p.document().get(), m);
}


// Add points ==================================================================

auto make_new_point_plot_method(Plotty& p) {
    noo::MethodData m;
    m.method_name            = "new_point_plot";
    m.documentation          = "Create a new point plot";
    m.argument_documentation = {
        { "[x y z x y z]", "A list of 3D points, laid out in a 1D array." },
        { "[ c ]",
          "A list of colors. Can be a 1D array of 3-tuple floats for RGB, or a "
          "list of hex strings" },
        { "[sx sy sz sx sy sz]",
          "A list of 3D scales, laid out in a 1D array." }
    };
    m.return_documentation = "An integer plot id";


    // input is
    // points [px,py,pz,px,py,pz,...]
    // colors [cr,cg,cb,cr,cg,cb,...]
    // scales [sx,sy,sz,sx,sy,sz,...]

    m.set_code([&p](noo::MethodContext const&,
                    PositionListArgument pos,
                    ColorListArgument    cols,
                    Scale3DListArgument  scales) -> noo::AnyVar {
        return p.append<PointPlot>(-1,
                                   std::move(pos.positions),
                                   std::move(cols.colors),
                                   std::move(scales.scales));
    });

    return noo::create_method(p.document().get(), m);
}

// Add lines ===================================================================

auto make_new_line_segment_plot_method(Plotty& p) {
    noo::MethodData m;
    m.method_name            = "new_line_segment_plot";
    m.documentation          = "Create a new line segment plot";
    m.argument_documentation = {
        { "[x y z x y z]",
          "A list of 3D point 2-tuples, laid out in a 1D array." },
        { "[ c ]",
          "A list of colors, one color for each point. Can be a 1D array of "
          "3-tuple floats for RGB, or a list of hex strings" },
        { "[sx sy sx sy]", "A list of 2D scales, laid out in a 1D array." }
    };
    m.return_documentation = "An integer plot id";

    m.set_code([&p](noo::MethodContext const&,
                    PositionListArgument pos,
                    ColorListArgument    cols,
                    Scale2DListArgument  scales) -> noo::AnyVar {
        return p.append<LineSegmentPlot>(-1,
                                         std::move(pos.positions),
                                         std::move(cols.colors),
                                         std::move(scales.scales));
    });

    return noo::create_method(p.document().get(), m);
}

// Add image ===================================================================

auto make_new_image_plot_method(Plotty& p) {
    noo::MethodData m;
    m.method_name            = "new_image_plot";
    m.documentation          = "Create a new image plane";
    m.argument_documentation = {
        { "", "Bytes of the on-disk image." },
        { "", "Point for the top-left of image plane" },
        { "", "Point for the bottom-left of image plane" },
        { "", "Point for the bottom-right of image plane" }
    };
    m.return_documentation = "An integer plot id";

    m.set_code([&](noo::MethodContext const&,
                   std::vector<std::byte> data,
                   PositionListArgument   pa,
                   PositionListArgument   pb,
                   PositionListArgument   pc) {
        return p.append<ImagePlot>(-1,
                                   std::move(data),
                                   noo::get_or_default(pa.positions, 0),
                                   noo::get_or_default(pb.positions, 0),
                                   noo::get_or_default(pc.positions, 0));
    });

    return noo::create_method(p.document().get(), m);
}

// Update Table ================================================================

auto make_update_table_method(Plotty& p) {

    noo::MethodData update_table;
    update_table.method_name            = "update_table_plot";
    update_table.documentation          = "Update table plot settings";
    update_table.argument_documentation = {
        { "plot_id", "Table plot identifier" },
        { "settings_array",
          "Array (count of 5) of source data columns indicies [ x, y, z, col, "
          "scale ]. Arg can be nothing to skip. col and scale indicies can be "
          "negative to not use." },
        { "color_map",
          "Color map. A list of [ [real, 'hexcolor'], ... ], where the "
          "real key is from 0->1. If None, will try to use the given color "
          "column directly for colors." }
    };
    update_table.return_documentation = "None";

    struct ColorMapArg {
        std::vector<std::pair<float, glm::vec3>> color_map;

        ColorMapArg() = default;
        ColorMapArg(noo::AnyVar&& a) {
            auto cmap_list = a.steal_vector();

            for (auto& a : cmap_list) {
                auto control_p = a.steal_vector();

                auto key   = noo::get_or_default(control_p, 0).to_real();
                auto value = noo::get_or_default(control_p, 1).to_string();

                auto qt_col = QColor(value.c_str());

                color_map.emplace_back(key,
                                       glm::vec3 { qt_col.redF(),
                                                   qt_col.greenF(),
                                                   qt_col.blueF() });
            }
        }
    };

    update_table.set_code([&p](noo::MethodContext const&,
                               int64_t              plot_id,
                               std::vector<int64_t> columns,
                               ColorMapArg          cmap) -> noo::AnyVar {
        // interpret

        auto* target = p.get_plot(plot_id);

        if (!target) throw noo::MethodException("Bad plot id!");

        auto* table_plot = dynamic_cast<TablePlot*>(target);

        if (!table_plot)
            throw noo::MethodException("Plot is not a table plot!");

        {
            auto x = noo::get_or_default(columns, 0);
            auto y = noo::get_or_default(columns, 1);
            auto z = noo::get_or_default(columns, 2);
            auto c = noo::get_or_default(columns, 3);
            auto s = noo::get_or_default(columns, 4);

            if (x < 0 or y < 0 or z < 0)
                throw noo::MethodException("Need extant x y and z columns!");

            table_plot->set_columns(x, y, z, c, s, cmap.color_map);
        }


        return true;
    });

    return noo::create_method(p.document().get(), update_table);
}

// Add Plotty ==================================================================

Plotty::Plotty(uint16_t port) {
    m_shared_domain = new SharedDomain(this);

    connect(m_shared_domain,
            &SharedDomain::domain_updated,
            this,
            &Plotty::on_domain_updated);

    m_server = noo::create_server(port);

    Q_ASSERT(m_server);

    m_doc = noo::get_document(m_server.get());

    Q_ASSERT(m_doc);

    noo::DocumentData docup;

    {
        auto ptr = make_load_table_method(*this);
        docup.method_list.push_back(ptr);


        ptr = make_new_point_plot_method(*this);
        docup.method_list.push_back(ptr);

        ptr = make_new_line_segment_plot_method(*this);
        docup.method_list.push_back(ptr);

        ptr = make_new_image_plot_method(*this);
        docup.method_list.push_back(ptr);

        ptr = make_set_domain_method(*this);
        docup.method_list.push_back(ptr);

        ptr = make_update_table_method(*this);
        docup.method_list.push_back(ptr);
    }

    noo::update_document(m_doc, docup);

    {
        noo::ObjectData nd;

        Q_ASSERT(m_doc);

        m_plot_root = noo::create_object(m_doc, nd);
    }

    rebuild_axis();
    make_box();
}

Plotty::~Plotty() { }

std::shared_ptr<noo::DocumentT> Plotty::document() {
    return m_doc;
}

noo::ObjectTPtr Plotty::plot_root() {
    return m_plot_root;
}

SharedDomain* Plotty::domain() const {
    return m_shared_domain;
}

int64_t Plotty::add_immediate_plot(std::vector<glm::vec3>&& points,
                                   std::vector<glm::vec3>&& colors,
                                   std::vector<glm::vec3>&& scales) {

    return append<PointPlot>(
        0, std::move(points), std::move(colors), std::move(scales));

    return 0;
}

Plot* Plotty::get_plot(size_t i) {
    auto iter = m_plots.find(i);

    if (iter == m_plots.end()) return nullptr;

    return iter->second.get();
}

void Plotty::on_domain_updated() {
    make_box();
}

void Plotty::on_domain_labels_updated() {
    rebuild_axis();
}
