#ifndef PLOTTY_H
#define PLOTTY_H

#include <server_interface.h>

#include <memory>
#include <unordered_map>

struct TableStorage;

class Plot;

struct Domain {
    glm::vec3 input_min = glm::vec3(-.5);
    glm::vec3 input_max = glm::vec3(0.5);

    // the output should probably be kept the same
    glm::vec3 output_min = glm::vec3(-.5);
    glm::vec3 output_max = glm::vec3(0.5);

    /// \brief transform an input coordinate to an output coordinate
    glm::vec3 transform(glm::vec3) const;
};

// =============================================================================

class SharedDomain : public QObject {
    Q_OBJECT
    Domain m_current_domain;
    bool   m_domain_auto = true;


    std::string m_x_axis_title = "x";
    std::string m_y_axis_title = "y";
    std::string m_z_axis_title = "z";

public:
    explicit SharedDomain(QObject* p);

    Domain const& current_domain() const;
    bool          domain_auto_updates() const;

    void set_domain_auto_updates(bool);

    void ask_set_domain(Domain d);

    void ask_update_input_bounds(glm::vec3, glm::vec3);

    void set_axis_labels(std::string x_axis_title,
                         std::string y_axis_title,
                         std::string z_axis_title);

    std::string x_axis_title() const;
    std::string y_axis_title() const;
    std::string z_axis_title() const;

signals:
    void domain_updated();
    void labels_updated();
};

// =============================================================================

class Plotty : public QObject {
    Q_OBJECT

    // Domain
    SharedDomain* m_shared_domain = nullptr;

    // Noodles stuff
    noo::ServerTPtr m_server;

    noo::DocumentTPtr m_doc;

    noo::ObjectTPtr m_plot_root;

    int64_t m_plot_counter = 1; // 0 is reserved for immediate plot

    noo::MeshTPtr     m_box_mesh;
    noo::MaterialTPtr m_box_mat;
    noo::ObjectTPtr   m_box;
    void              make_box();

    noo::ObjectTPtr m_axis_x;
    noo::ObjectTPtr m_axis_y;
    noo::ObjectTPtr m_axis_z;
    void            rebuild_axis();

    // Plots
    std::unordered_map<size_t, std::unique_ptr<Plot>> m_plots;


public:
    Plotty(uint16_t port);

    ~Plotty();

    std::shared_ptr<noo::DocumentT> document();

    noo::ObjectTPtr plot_root();

    SharedDomain* domain() const;

    auto const& all_plots() const { return m_plots; }

public:
    ///
    /// \brief Append a new plot to the scene
    ///
    /// \param req The slot the plot should take. If < 0, new slot
    ///
    template <class PlotType, class... Args>
    int64_t append(int64_t req, Args&&... args) {
        int64_t place = req;

        if (req < 0) {
            place = m_plot_counter;
            m_plot_counter++;
        }

        m_plots.try_emplace(place,
                            std::make_unique<PlotType>(
                                *this, place, std::forward<Args>(args)...));

        return place;
    }

    int64_t add_immediate_plot(std::vector<glm::vec3>&& points,
                               std::vector<glm::vec3>&& colors,
                               std::vector<glm::vec3>&& scales);

    Plot* get_plot(size_t);

private slots:
    void on_domain_updated();
    void on_domain_labels_updated();
};

#endif // PLOTTY_H
