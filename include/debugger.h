#pragma once


#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <string>
#include <sstream>
#include <fmt/ostream.h>

#include <v8-debug.h>
#include <v8-inspector.h>
#include <v8-debug.h>

#include "javascript.h"

namespace v8toolkit {


/**
Inspector types:

v8_inspector::V8Inspector::Channel
 - Interface: sendResponse, sendNotification, flushProtocol
  - basically takes a string and sends it out



v8_inspector::V8InspectorClient
  - has a "channel"
  - has an V8Inspector
  - creates a V8InspectorSesssion when an inspector is "connected" to a channel

V8InspectorSession:
 - Does the "main work"
 - Received messages are sent to session::dispatchProtocolMessage



 v8_inspector::V8Inspector
  - associated and 'connected' to a v8::Context


*/

class DebugMessage {
private:
    std::string method_name;

public:
    DebugMessage(std::string const & message_payload);
    std::string const & get_method_name();
};

class RequestMessage : public DebugMessage {

public:
    // takes a message and returns the appropriate RequestMessage object type for it
    static std::unique_ptr<RequestMessage> process_message(std::string const & message_payload);
};

class ResponseMessage : public DebugMessage {

public:
    static std::unique_ptr<ResponseMessage> create_response(std::string const & method_name);
};

class DebugMessageManager {
    // map type between
    using MessageMap = std::map<std::string, std::function<void(std::string const &)>>;
};

// implements sending data to debugger
class WebsocketChannel : public v8_inspector::V8Inspector::Channel {
public:
    WebsocketChannel(v8::Local<v8::Context> context, short port);

    virtual ~WebsocketChannel();

private:
    void sendResponse(int callId,
                      std::unique_ptr<v8_inspector::StringBuffer> message) override {
        Send(message->string());
    }


    void sendNotification(std::unique_ptr<v8_inspector::StringBuffer> message) override {

        Send(message->string());
    }


    void flushProtocolNotifications() override {}


    void Send(const v8_inspector::StringView &string);

    v8::Isolate *isolate;
    v8::Global<v8::Context> context;

    using DebugServerType = websocketpp::server<websocketpp::config::asio>;
    DebugServerType debug_server;

    unsigned short port = 0;

    using WebSocketConnections = std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>>;
    WebSocketConnections connections;

    void send_message(std::string const &message);
//    void send_message(std::wstring const &message);


    bool websocket_validation_handler(websocketpp::connection_hdl hdl);

    // registered callbacks with websocketpp
    void on_open(websocketpp::connection_hdl hdl);

    void on_close(websocketpp::connection_hdl hdl);

    void on_message(websocketpp::connection_hdl hdl, DebugServerType::message_ptr msg);

    void on_http(websocketpp::connection_hdl hdl);


public:
    void wait_for_connection(std::chrono::duration<float> sleep_between_polls);
    void poll();
    void poll_one();

};


enum {
    // The debugger reserves the first slot in the Context embedder data.
    kDebugIdIndex = v8::Context::kDebugIdIndex,
    kModuleEmbedderDataIndex,
    kInspectorClientIndex
};


class InspectorClient : public v8_inspector::V8InspectorClient {
public:
    InspectorClient(v8::Local<v8::Context> context, short port) {
        isolate_ = context->GetIsolate();
        channel_.reset(new WebsocketChannel(context, port));
        inspector_ = v8_inspector::V8Inspector::create(isolate_, this);
        session_ =
                inspector_->connect(1, channel_.get(), v8_inspector::StringView());
        context->SetAlignedPointerInEmbedderData(kInspectorClientIndex, this);
        inspector_->contextCreated(v8_inspector::V8ContextInfo(
                context, kContextGroupId, v8_inspector::StringView()));

        v8::Debug::SetLiveEditEnabled(isolate_, true);

        context_.Reset(isolate_, context);
    }

    virtual void runMessageLoopOnPause(int contextGroupId) override {
        this->paused = true;
        while(true) {
            std::cerr << fmt::format("runMessageLoopOnPause") << std::endl;
            sleep(1);
            this->channel_->poll();
        }
        std::cerr << fmt::format("exiting runMessageLoopOnPause") << std::endl;
    }

    virtual void quitMessageLoopOnPause() override{
        std::cerr << fmt::format("quitMessageLoopOnPause, setting paused=false") << std::endl;
        this->paused = false;
    }

public:
    static v8_inspector::V8InspectorSession *GetSession(v8::Local<v8::Context> context) {
        InspectorClient *inspector_client = static_cast<InspectorClient *>(
                context->GetAlignedPointerFromEmbedderData(kInspectorClientIndex));
        return inspector_client->session_.get();
    }

    v8::Local<v8::Context> ensureDefaultContextInGroup(int group_id) override {
        return context_.Get(isolate_);
    }

    static void SendInspectorMessage(v8::Local<v8::Context> context, std::string const & message) {
        auto isolate = context->GetIsolate();
        auto session = GetSession(context);
        v8_inspector::StringView message_view((uint8_t const *)message.c_str(), message.length());
        session->dispatchProtocolMessage(message_view);
    }

    static const int kContextGroupId = 1;
    bool paused = false;

    std::unique_ptr<v8_inspector::V8Inspector> inspector_;
    std::unique_ptr<v8_inspector::V8InspectorSession> session_;
    std::unique_ptr<WebsocketChannel> channel_;
    v8::Global<v8::Context> context_;
    v8::Isolate *isolate_;
};
///// END NEW DEBUG CODE


};
