#ifndef PLOT_H
#define PLOT_H

#include <server_interface.h>

#include <QObject>

#include <memory>

/*!
 * \brief Compute linear interpolation
 * \param x The interpolation source.
 * \param x0 The minimum range of x.
 * \param x1 The maximum range of x.
 * \param y0 The minimum range of y.
 * \param y1 The maximum range of y.
 *
 * Template T and U should support +, -, *, and / operators.
 */
template <class T, class U>
U lerp(T x, T const& x0, T const& x1, U const& y0, U const& y1) {
    return y0 + (y1 - y0) * ((x - x0) / (x1 - x0));
}

struct Domain;

class Plotty;

class Plot : public QObject {
    Q_OBJECT
protected:
    Plotty*           m_host;
    noo::DocumentTPtr m_doc;
    int64_t           m_plot_id;

    //
    noo::MethodTPtr m_get_id_method;

    // owned by subclasses
    noo::MaterialTPtr m_mat;
    noo::MeshTPtr     m_mesh;
    noo::ObjectTPtr   m_obj;

    virtual void domain_updated(Domain const&);

public:
    Plot(Plotty& host, int64_t id);
    ~Plot();

    noo::ObjectTPtr const& object();
};


#endif // PLOT_H
