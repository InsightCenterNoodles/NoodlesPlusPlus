#ifndef TABLEPLOT_H
#define TABLEPLOT_H

#include "pointplot.h"

class SimpleTable;

class TablePlot : public PointPlot {
    Q_OBJECT

    std::shared_ptr<SimpleTable> m_table_data;
    noo::TableTPtr               m_noo_table;

    // sources
    int64_t m_xcol = 0;
    int64_t m_ycol = 0;
    int64_t m_zcol = 0;

    int64_t m_color_col = -1;
    int64_t m_size_col  = -1;

    std::vector<std::pair<float, glm::vec3>> m_color_map;

    bool are_sources_valid() const;

    ///
    /// \brief Rebuild the local cache, assuming all columns have been sanitized
    ///
    void rebuild_cache(size_t from = 0, size_t count = -1);

    void set_clear();

public:
    TablePlot(Plotty& host, int64_t id, std::shared_ptr<SimpleTable> const&);

    ~TablePlot() override;

    void set_columns(int64_t                                  xcol,
                     int64_t                                  ycol,
                     int64_t                                  zcol,
                     int64_t                                  colorcol,
                     int64_t                                  sizecol,
                     std::vector<std::pair<float, glm::vec3>> cmap);

private slots:
    void on_rows_inserted_at(int64_t at, int64_t count);
};

#endif // TABLEPLOT_H
