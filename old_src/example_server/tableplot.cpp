#include "tableplot.h"

#include "simpletable.h"

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/quaternion.hpp"

#include <QColor>
#include <QDebug>

// consumes RGB
glm::vec3
color_interpolation(glm::vec3 a, glm::vec3 b, float v, float l, float h) {
    glm::dvec3 hsv_a;
    glm::dvec3 hsv_b;

    QColor::fromRgbF(a.r, a.g, a.b).getHsvF(&hsv_a.x, &hsv_a.y, &hsv_a.z);
    QColor::fromRgbF(b.r, b.g, b.b).getHsvF(&hsv_b.x, &hsv_b.y, &hsv_b.z);

    auto result = lerp<double>(v, l, h, hsv_a, hsv_b);

    glm::dvec3 ret(1);

    QColor::fromHsvF(result.x, result.y, result.z)
        .getRgbF(&ret.r, &ret.g, &ret.b);

    return ret;
}

glm::vec3 color_map_sample(std::vector<std::pair<float, glm::vec3>> const& map,
                           float                                           f) {
    if (map.empty()) { return { 1, 1, 1 }; }

    auto b = std::upper_bound(
        map.begin(), map.end(), f, [](auto const& m, auto const& n) {
            return m < n.first;
        });

    if (b == map.begin()) { return b->second; }

    if (b == map.end()) { return map.back().second; }

    auto a = b - 1;

    return color_interpolation(a->second, b->second, f, a->first, b->first);
}

bool TablePlot::are_sources_valid() const {
    bool xyz = m_xcol >= 0 and m_ycol >= 0 and m_zcol >= 0;

    auto num_cols = m_table_data->get_columns().size();

    qDebug() << Q_FUNC_INFO << num_cols;

    bool xyz_max =
        m_xcol < num_cols and m_ycol < num_cols and m_zcol < num_cols;

    bool color_ok = m_color_col < 0 ? true : m_color_col < num_cols;
    bool size_ok  = m_size_col < 0 ? true : m_size_col < num_cols;

    qDebug() << Q_FUNC_INFO << xyz << xyz_max << color_ok << size_ok;

    return xyz and xyz_max and color_ok and size_ok;
}


void TablePlot::set_columns(int64_t                                  xcol,
                            int64_t                                  ycol,
                            int64_t                                  zcol,
                            int64_t                                  colorcol,
                            int64_t                                  sizecol,
                            std::vector<std::pair<float, glm::vec3>> cmap) {

    m_xcol = xcol;
    m_ycol = ycol;
    m_zcol = zcol;

    m_color_col = colorcol;
    m_size_col  = sizecol;

    m_color_map = std::move(cmap);

    rebuild_cache();
}


// this assumes that the incoming span is NOT EMPTY
template <class T>
inline T const& modulus_indexed(std::span<T> t, size_t index) {
    return t[index % t.size()];
}

template <class T>
inline T const& modulus_indexed(std::vector<T> const& t, size_t index) {
    return t[index % t.size()];
}

inline glm::vec3 color_to_vec3(std::string const& source) {
    QColor c(source.c_str());
    return glm::vec3(c.redF(), c.greenF(), c.blueF());
}

void TablePlot::rebuild_cache(size_t from, size_t count) {
    qDebug() << Q_FUNC_INFO << from << count;

    if (!are_sources_valid()) {
        qDebug() << "Sources invalid";
        return;
    }

    auto num_cols = m_table_data->get_columns().size();

    if (num_cols == 0) {
        qDebug() << "No columns in source";
        set_clear();
        return;
    }

    qDebug() << Q_FUNC_INFO << "Num cols in source" << num_cols;

    // lets do points
    auto& x_row_raw = m_table_data->storage().data.at(m_xcol);
    auto& y_row_raw = m_table_data->storage().data.at(m_ycol);
    auto& z_row_raw = m_table_data->storage().data.at(m_zcol);

    auto num_rows = std::min(x_row_raw.size(),
                             std::min(y_row_raw.size(), z_row_raw.size()));

    qDebug() << Q_FUNC_INFO << "Num rows in source" << num_rows;

    if (num_rows == 0) {
        set_clear();
        return;
    }

    from  = std::clamp<size_t>(from, 0, num_rows);
    count = std::clamp<size_t>(count, 0, num_rows - from);

    qDebug() << Q_FUNC_INFO << from << count;

    auto last = from + count;


    if (num_rows > m_points.size()) {
        m_points.resize(num_rows);
        m_colors.resize(num_rows);
        m_scales.resize(num_rows);
    }


    double const blank = 0.0;

    auto seat_source =
        [&blank](TableColumn const& c) -> std::span<double const> {
        if (c.is_reals()) return c.as_reals();
        return { &blank, 1 };
    };

    auto x_source = seat_source(x_row_raw);
    auto y_source = seat_source(y_row_raw);
    auto z_source = seat_source(z_row_raw);


    // new min_max
    glm::vec3 new_min(std::numeric_limits<float>::max());
    glm::vec3 new_max(std::numeric_limits<float>::lowest());

    for (size_t row = from; row < last; row++) {
        const auto p = glm::vec3(modulus_indexed(x_source, row),
                                 modulus_indexed(y_source, row),
                                 modulus_indexed(z_source, row));

        qDebug() << p.x << p.y << p.z;

        m_points[row] = p;

        new_min = glm::min(new_min, p);
        new_max = glm::max(new_max, p);
    }

    // now colors

    glm::vec3 const default_white(1, 1, 1);

    if (m_color_col < 0) {
        // none defined, set white

        std::span<glm::vec3 const> source(&default_white, 1);

        for (size_t row = from; row < last; row++) {
            m_colors[row] = modulus_indexed(source, row);
        }
    } else {
        // something is defined

        auto& color_raw = m_table_data->storage().data.at(m_color_col);

        if (color_raw.is_reals()) {

            auto& reals = color_raw.as_reals();

            for (size_t row = from; row < last; row++) {
                m_colors[row] = glm::vec3(modulus_indexed(reals, row));
            }

        } else if (color_raw.is_strings()) {

            auto& strings = color_raw.as_strings();

            for (size_t row = from; row < last; row++) {
                m_colors[row] = color_to_vec3(modulus_indexed(strings, row));
            }
        } else if (color_raw.is_vectors()) {
            auto& vecs = color_raw.as_vectors();

            for (size_t row = from; row < last; row++) {
                m_colors[row] = modulus_indexed(vecs, row);
            }
        }
    }

    // now scales

    glm::vec3 const default_size(0.05);

    if (m_size_col < 0) {
        // none defined, set default

        std::span<glm::vec3 const> source(&default_size, 1);

        for (size_t row = from; row < last; row++) {
            m_scales[row] = modulus_indexed(source, row);
        }
    } else {

        auto& scale_raw = m_table_data->storage().data.at(m_size_col);

        if (scale_raw.is_reals()) {

            auto& reals = scale_raw.as_reals();

            for (size_t row = from; row < last; row++) {
                m_scales[row] = glm::vec3(modulus_indexed(reals, row));
            }

        } else if (scale_raw.is_vectors()) {
            auto& vecs = scale_raw.as_vectors();

            for (size_t row = from; row < last; row++) {
                m_scales[row] = modulus_indexed(vecs, row);
            }
        }
    }

    qDebug() << "Cache rebuilt";

    rebuild_instances(from, count);
    // this is kinda nasty, as we are thus double rebuilding instances...

    m_host->domain()->ask_update_input_bounds(new_min, new_max);
}

void TablePlot::set_clear() {
    m_points.clear();
    m_colors.clear();
    m_scales.clear();
    rebuild_instances();
    return;
}


TablePlot::TablePlot(Plotty&                             host,
                     int64_t                             id,
                     std::shared_ptr<SimpleTable> const& table)
    : PointPlot(host, id, {}, {}, {}), m_table_data(table) {


    noo::TableData table_data;
    table_data.name   = table->storage().name;
    table_data.source = table;

    m_noo_table = noo::create_table(m_doc, table_data);

    set_columns(0, 1, 2, -1, -1, {});

    connect(table.get(),
            &SimpleTable::table_row_added,
            this,
            &TablePlot::on_rows_inserted_at);

    on_rows_inserted_at(0, -1); // negative 1 will get turned into max
}

TablePlot::~TablePlot() = default;

void TablePlot::on_rows_inserted_at(int64_t at, int64_t count) {
    rebuild_cache(at, count); // signed to unsigned intended
}
