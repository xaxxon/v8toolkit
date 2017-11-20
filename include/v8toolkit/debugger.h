#pragma once

#define V8TOOLKIT_DEBUGGING_ACTIVE

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <string>
#include <sstream>
#include <fmt/ostream.h>

#include <v8-debug.h>
#include <v8-inspector.h>
#include <v8-debug.h>


//#include <nlohmann/json.hpp>

#include "javascript.h"

namespace v8toolkit {

class DebugContext;

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
#if 0
//class ResponseMessage;
//
//class DebugMessage {
//
//public:
//    DebugMessage(v8toolkit::DebugContext & debug_context) :
//        debug_context(debug_context)
//    {}
//
//    DebugMessage(DebugMessage const &) = default;
//
//    // the message id associated with this message (either the incoming ID of a
//    //   RequestMessage or the ID of the request a ResponseMessage is for
//    v8toolkit::DebugContext & debug_context;
//
//    virtual nlohmann::json to_json() const = 0;
//};
//
///**
// *sent from the application to the debugger as informational data, not in direct response to a
// * request
// */
//class InformationalMessage : public DebugMessage {
//
//public:
//    InformationalMessage(v8toolkit::DebugContext & debug_context) :
//        DebugMessage(debug_context)
//    {}
//
//};
//
//void to_json(nlohmann::json& j, const InformationalMessage & informational_message);
//
//
//
//class RequestResponseMessage : public DebugMessage {
//
//public:
//    RequestResponseMessage(v8toolkit::DebugContext & debug_context, int message_id) :
//        DebugMessage(debug_context),
//        message_id(message_id)
//    {}
//
//    int const message_id;
//};
//
//
//class RequestMessage : public RequestResponseMessage {
//private:
//    std::string method_name;
//public:
//    RequestMessage(v8toolkit::DebugContext & context, nlohmann::json const & json);
//
//    /**
//     * Every RequestMessage must generate a ResponseMessage
//     * @return the corresponding ResponseMessage to this RequestMessage
//     */
//    virtual std::unique_ptr<ResponseMessage> generate_response_message() const = 0;
//
//    /**
//     * A RequestMessage generates 0 or more InformationalMessage objects
//     * @return Any InformationalMessage objects which need to be sent back to the debugger
//     */
//    virtual std::vector<std::unique_ptr<InformationalMessage> > generate_additional_messages() const {return {};}
//
//
//    nlohmann::json to_json() const override;
//};
//
///**
// * Any message involved in a query/response from the debugger
// */
//class ResponseMessage : public RequestResponseMessage {
//    friend class RequestMessage;
//
//protected:
//    ResponseMessage(RequestMessage const & request_message);
//
//public:
//
//};
//void to_json(nlohmann::json& j, const ResponseMessage & response_message);
//
//
///**
// * Stores mapping between
// */
//class MessageManager {
//public:
//    // map type between
//    using MessageMap = std::map<std::string, std::function<std::unique_ptr<RequestMessage>(std::string const &)>>;
//private:
//
//    MessageMap message_map;
//
//    v8toolkit::DebugContext & debug_context;
//
//public:
//    MessageManager(v8toolkit::DebugContext & context);
//
//    template<class RequestMessageT, std::enable_if_t<std::is_base_of<RequestMessage, RequestMessageT>::value, int> = 0>
//    void add_request_message_handler() {
//        this->message_map[RequestMessageT::message_name] = [this](std::string const & message_payload){
//            nlohmann::json const & json = nlohmann::json::parse(message_payload);
//            return std::make_unique<RequestMessageT>(this->debug_context, json);
//        };
//    };
//
//    std::vector<std::unique_ptr<ResponseMessage>> generate_response_message(RequestMessage const & request_message);
//
//    void process_request_message(std::string const &message_payload);
//};
#endif

// implements sending data to debugger
class WebsocketChannel : public v8_inspector::V8Inspector::Channel {
public:
    WebsocketChannel(v8toolkit::DebugContext & debug_context, short port);

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
    v8toolkit::DebugContext & debug_context;

    using DebugServerType = websocketpp::server<websocketpp::config::asio>;
    DebugServerType debug_server;

    unsigned short port = 0;

    using WebSocketConnections = std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>>;
    WebSocketConnections connections;

//    void send_message(std::wstring const &message);


    bool websocket_validation_handler(websocketpp::connection_hdl hdl);

    // registered callbacks with websocketpp
    void on_open(websocketpp::connection_hdl hdl);

    void on_close(websocketpp::connection_hdl hdl);

    void on_message(websocketpp::connection_hdl hdl, DebugServerType::message_ptr msg);

    void on_http(websocketpp::connection_hdl hdl);

    std::chrono::time_point<std::chrono::high_resolution_clock> message_received_time = std::chrono::high_resolution_clock::now();
    std::chrono::time_point<std::chrono::high_resolution_clock> message_sent_time = std::chrono::high_resolution_clock::now();

public:
    void wait_for_connection(std::chrono::duration<float> sleep_between_polls = std::chrono::duration<float>(0.1f));
    void poll();

    /**
     * runs a single request if available, otherwise returns immediately
     */
    void poll_one();

    /**
     * polls until connection has been idle for  specified amount of time
     * @param idle_time amount of time between messages to wait until returning
     */
    void poll_until_idle(float idle_time = 1.0f);

    /**
     * blocks until a request is handled
     */
    void run_one();

    void send_message(void * data, size_t length);


    /**
     * How long ago was most recent message received
     * @return number of seconds
     */
    float seconds_since_message_received();

    /**
     * How long ago was most recent message sent
     * @return number of sceonds
     */
    float seconds_since_message_sent();

    /**
     * how long ago was a message either sent or received
     * @return number of seconds
     */
    float seconds_since_message();

//    MessageManager message_manager;
};


enum {
    // The debugger reserves the first slot in the Context embedder data.
        kDebugIdIndex = v8::Context::kDebugIdIndex,
    kModuleEmbedderDataIndex,
    kInspectorClientIndex
};


class DebugContext : public v8_inspector::V8InspectorClient, public v8toolkit::Context {
private:
    std::string frame_id = "12345.1"; // arbitrary value
    std::unique_ptr<WebsocketChannel> channel;
    std::unique_ptr<v8_inspector::V8Inspector> inspector;
    std::unique_ptr<v8_inspector::V8InspectorSession> session;

    std::vector<std::string> message_types_handled_by_v8_inspector = {};

    short port;

public:
    DebugContext(std::shared_ptr<v8toolkit::Isolate> isolate_helper, v8::Local<v8::Context> context, short port);


    virtual void runMessageLoopOnPause(int contextGroupId) override {
        this->paused = true;
        while (this->paused) {
            this->channel->run_one();
        }
        std::cerr << fmt::format("exiting runMessageLoopOnPause") << std::endl;
    }

    virtual void quitMessageLoopOnPause() override {
        std::cerr << fmt::format("quitMessageLoopOnPause, setting paused=false") << std::endl;
        this->paused = false;
    }

    std::string const & get_frame_id() const {return this->frame_id;}
    std::string get_base_url() const {return this->get_uuid_string();}

    v8::Local<v8::Context> ensureDefaultContextInGroup(int group_id) override {
        return *this;
    }

    static const int kContextGroupId = 1;
    bool paused = false;

    WebsocketChannel & get_channel() {return *this->channel;}
    v8_inspector::V8InspectorSession & get_session() {return *this->session;}
    void reset_session();

    short get_port(){return this->port;}

};
///// END NEW DEBUG CODE

} // end v8toolkit namespace
