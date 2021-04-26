#include "plot.h"

#include "plotty.h"
#include "simpletable.h"

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/quaternion.hpp"


#include <QDebug>

// static noo::MethodTPtr activate_method;

// static noo::MethodTPtr get_options_method;
// static noo::MethodTPtr get_current_option_method;
// static noo::MethodTPtr set_current_option_method;

// static noo::MethodTPtr move_method;
// static noo::MethodTPtr rot_method;
// static noo::MethodTPtr scale_method;

// static noo::MethodTPtr select_region_method;
// static noo::MethodTPtr select_at_method;
// static noo::MethodTPtr clear_select_method;

// static noo::MethodTPtr probe_method;


static std::weak_ptr<noo::MethodT> get_id_method_hook;

static auto make_get_id_method(Plotty& host, noo::DocumentTPtr doc_ptr) {
    noo::MethodData method_data;
    method_data.method_name            = "get_plot_id";
    method_data.documentation          = "Get the plot id from a plot object";
    method_data.argument_documentation = {};
    method_data.return_documentation =
        "The integer id of the plot. -1 if there is none.";

    method_data.set_code([&host](noo::MethodContext const& ctx) -> noo::AnyVar {
        auto obj = ctx.get_object();

        if (!obj)
            throw noo::MethodException(
                "Method should only be called on an object");

        auto const& plots = host.all_plots();

        for (auto const& [k, v] : plots) {
            if (v->object() == obj) { return k; }
        }

        return -1;
    });

    return noo::create_method(doc_ptr, method_data);
}

void Plot::domain_updated(Domain const&) { }

Plot::Plot(Plotty& host, int64_t id)
    : m_host(&host), m_doc(host.document()), m_plot_id(id) {

    auto method = get_id_method_hook.lock();

    if (!method) {
        m_get_id_method    = make_get_id_method(host, m_doc);
        get_id_method_hook = m_get_id_method;
    } else {
        m_get_id_method = method;
    }

    connect(host.domain(), &SharedDomain::domain_updated, [&]() {
        auto const& d = host.domain()->current_domain();
        domain_updated(d);
    });
}

Plot::~Plot() { }

noo::ObjectTPtr const& Plot::object() {
    return m_obj;
}
