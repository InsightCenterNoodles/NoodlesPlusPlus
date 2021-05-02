#include "bufferlist.h"

#include "src/common/variant_tools.h"
#include "src/generated/interface_tools.h"

namespace noo {

static QByteArray span_to_array(std::span<std::byte const> sp) {
    if (sp.empty()) return {};
    return QByteArray(reinterpret_cast<char const*>(sp.data()), sp.size());
}

BufferList::BufferList(ServerT* s) : ComponentListBase(s) { }
BufferList::~BufferList() = default;

BufferT::BufferT(IDType id, BufferList* host, BufferData const& d)
    : ComponentMixin<BufferT, BufferList, BufferID>(id, host) {

    VMATCH(
        d,
        VCASE(BufferCopySource const& source) {
            m_bytes = span_to_array(source.to_copy);
            if (m_bytes.isEmpty()) { m_bytes.fill('\0', 128); }
        },
        VCASE(BufferURLSource const& source) { m_url_source = source; });
}

void BufferT::write_new_to(Writer& w) {
    auto lid = convert_id(id(), w);

    if (m_url_source) {
        auto url_string = m_url_source->url_source.toString().toStdString();
        auto size       = m_url_source->source_byte_size;

        auto x = noodles::CreateBufferCreateDirect(
            w, lid, nullptr, url_string.c_str(), size);

        w.complete_message(x);
        return;
    }

    auto byte_handle =
        w->CreateVectorScalarCast<int8_t>(m_bytes.data(), m_bytes.size());

    auto x = noodles::CreateBufferCreate(w, lid, byte_handle);

    w.complete_message(x);
    return;
}

void BufferT::write_delete_to(Writer& w) {
    auto lid = convert_id(id(), w);

    auto x = noodles::CreateBufferDelete(w, lid);

    w.complete_message(x);
}

void BufferT::write_refresh_to(Writer& w) {
    write_new_to(w);
}

// =============================================================================


LightList::LightList(ServerT* s) : ComponentListBase(s) { }
LightList::~LightList() = default;

LightT::LightT(IDType id, LightList* host, LightData const& d)
    : ComponentMixin<LightT, LightList, LightID>(id, host), m_data(d) { }

void LightT::write_new_to(Writer& w) {
    auto lid = convert_id(id(), w);

    auto ncol = convert(m_data.color);

    auto x = noodles::CreateLightCreateUpdate(w, lid, &ncol, m_data.intensity);

    w.complete_message(x);
}

void LightT::update(LightData const& d, Writer& w) {
    m_data = d;

    write_new_to(w);
}

void LightT::update(LightData const& d) {
    auto w = m_parent_list->new_bcast();

    update(d, *w);
}
void LightT::write_delete_to(Writer& w) {
    auto lid = convert_id(id(), w);

    auto x = noodles::CreateLightDelete(w, lid);

    w.complete_message(x);
}

} // namespace noo
