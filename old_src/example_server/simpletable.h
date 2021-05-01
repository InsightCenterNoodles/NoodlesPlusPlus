#ifndef SIMPLETABLE_H
#define SIMPLETABLE_H

#include <server_interface.h>

#include <QObject>

struct TableColumn {
    using RealList   = std::vector<double>;
    using StringList = std::vector<std::string>;
    using VectorList = std::vector<glm::vec3>;

    using VariantType = std::variant<RealList, StringList, VectorList>;

    VariantType column_data;

    TableColumn() = default;

    TableColumn(RealList&&);
    TableColumn(StringList&&);
    TableColumn(VectorList&&);


    bool is_reals() const { return column_data.index() == 0; }
    bool is_strings() const { return column_data.index() == 1; }
    bool is_vectors() const { return column_data.index() == 2; }

    size_t size() const;

    RealList&   as_reals();
    StringList& as_strings();
    VectorList& as_vectors();

    RealList const&   as_reals() const;
    StringList const& as_strings() const;
    VectorList const& as_vectors() const;

    void push_back(double);
    void push_back(std::string);
    void push_back(glm::vec3);

    double      get_real_at(size_t) const;
    std::string get_string_at(size_t) const;
    glm::vec3   get_vector_at(size_t) const;
};

struct TableStorage {
    std::string              name;
    std::vector<std::string> header;
    std::vector<TableColumn> data;
};

class SimpleTableRowQuery : public noo::TableQuery {
    std::vector<double> m_data;

public:
    SimpleTableRowQuery(int64_t             row,
                        std::span<int64_t>  columns,
                        TableStorage const& s);
    ~SimpleTableRowQuery() override = default;

    size_t next() const override;

    void write_next_to(std::span<double> target) override;
};


class SimpleTableBlockQuery : public noo::TableQuery {
    std::vector<double> m_data;

public:
    SimpleTableBlockQuery(std::pair<int64_t, int64_t> rows,
                          std::span<int64_t>          columns,
                          TableStorage const&         s);
    ~SimpleTableBlockQuery() override = default;

    size_t next() const override;

    void write_next_to(std::span<double> target) override;
};

class SimpleTable : public noo::TableSource {
    TableStorage m_storage;

public:
    SimpleTable(TableStorage&&);
    ~SimpleTable() override;

    TableStorage const& storage() const { return m_storage; }

    std::vector<std::string> get_columns() override;

    size_t get_num_rows() override;

    noo::QueryPtr get_row(int64_t row, std::span<int64_t> columns) override;

    noo::QueryPtr get_block(std::pair<int64_t, int64_t> rows,
                            std::span<int64_t>          columns) override;

    noo::QueryPtr get_selection_data(std::string_view selection_id) override;

    bool request_row_insert(int64_t /*row*/,
                            std::span<double> /*data*/) override;
    bool request_row_update(int64_t /*row*/,
                            std::span<double> /*data*/) override;
    bool request_row_append(std::span<std::span<double>>) override;

    bool request_deletion(noo::Selection const&) override;

    noo::SelectionRef get_selection(std::string_view) override;

    bool request_set_selection(std::string_view, noo::SelectionRef) override;

    std::vector<std::string> get_all_selections() override;
};

#endif // SIMPLETABLE_H
