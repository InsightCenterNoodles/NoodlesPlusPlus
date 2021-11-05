#ifndef CLIENTMESSAGEHANDLER_H
#define CLIENTMESSAGEHANDLER_H

#include "src/generated/noodles_generated.h"

#include <QWebSocket>

namespace nooc {

class ClientWriter {
    QWebSocket&                    m_socket;
    flatbuffers::FlatBufferBuilder m_builder;

    bool m_written = false;

    void finished_writing_and_export();

public:
    ClientWriter(QWebSocket& s);
    ~ClientWriter();
    flatbuffers::FlatBufferBuilder& builder() { return m_builder; }

    operator flatbuffers::FlatBufferBuilder&() { return m_builder; }

    flatbuffers::FlatBufferBuilder* operator->() { return &m_builder; }

    template <class T>
    void complete_message(flatbuffers::Offset<T> message) {

        auto enum_value = noodles::ClientMessageTypeTraits<T>::enum_value;

        Q_ASSERT(enum_value != noodles::ClientMessageType::NONE);

        auto sm = noodles::CreateClientMessage(
            m_builder, enum_value, message.Union());

        std::vector<flatbuffers::Offset<noodles::ClientMessage>> sms;

        sms.push_back(sm);

        auto sms_handle = noodles::CreateClientMessagesDirect(m_builder, &sms);

        m_builder.Finish(sms_handle);
        finished_writing_and_export();
    }
};

// =============================================================================

class ClientState;

class MessageHandler {
    QWebSocket&  m_socket;
    ClientState& m_state;
    QByteArray   m_bin_message;


    void process_message(noodles::MethodCreate const&);
    void process_message(noodles::MethodDelete const&);
    void process_message(noodles::SignalCreate const&);
    void process_message(noodles::SignalDelete const&);
    void process_message(noodles::ObjectCreateUpdate const&);
    void process_message(noodles::ObjectDelete const&);
    void process_message(noodles::BufferCreate const&);
    void process_message(noodles::BufferDelete const&);
    void process_message(noodles::MaterialCreateUpdate const&);
    void process_message(noodles::MaterialDelete const&);
    void process_message(noodles::TextureCreateUpdate const&);
    void process_message(noodles::TextureDelete const&);
    void process_message(noodles::LightCreateUpdate const&);
    void process_message(noodles::LightDelete const&);
    void process_message(noodles::GeometryCreate const&);
    void process_message(noodles::GeometryDelete const&);
    void process_message(noodles::TableCreateUpdate const&);
    void process_message(noodles::TableDelete const&);
    void process_message(noodles::PlotCreateUpdate const&);
    void process_message(noodles::PlotDelete const&);
    void process_message(noodles::DocumentUpdate const&);
    void process_message(noodles::DocumentReset const&);
    void process_message(noodles::SignalInvoke const&);
    void process_message(noodles::MethodReply const&);


    void process_message(noodles::ServerMessage const& message);

public:
    MessageHandler(QWebSocket& s, ClientState& state, QByteArray m);

    void process();
};

} // namespace nooc

#endif // CLIENTMESSAGEHANDLER_H
