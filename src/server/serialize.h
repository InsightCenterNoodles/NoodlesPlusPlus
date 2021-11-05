#ifndef SERIALIZE_H
#define SERIALIZE_H

#include "include/noo_id.h"
#include "include/noo_server_interface.h"
#include "src/generated/noodles_generated.h"

#include <flatbuffers/flatbuffers.h>

#include <QObject>

namespace noo {

class Writer : public QObject {
    Q_OBJECT

    flatbuffers::FlatBufferBuilder m_builder;

    bool m_written = false;

    void finished_writing_and_export();

public:
    Writer();
    ~Writer() noexcept override;

    flatbuffers::FlatBufferBuilder& builder();

    operator flatbuffers::FlatBufferBuilder&() { return builder(); }

    flatbuffers::FlatBufferBuilder* operator->() { return &m_builder; }

    template <class T>
    void complete_message(flatbuffers::Offset<T> message) {
        auto enum_value = noodles::ServerMessageTypeTraits<T>::enum_value;

        Q_ASSERT(enum_value != noodles::ServerMessageType::NONE);

        auto sm = noodles::CreateServerMessage(
            m_builder, enum_value, message.Union());

        std::vector<flatbuffers::Offset<noodles::ServerMessage>> sms;

        sms.push_back(sm);

        auto sms_handle = noodles::CreateServerMessagesDirect(m_builder, &sms);

        m_builder.Finish(sms_handle);
        finished_writing_and_export();
    }

signals:
    void data_ready(QByteArray);
};

} // namespace noo

#endif // SERIALIZE_H
