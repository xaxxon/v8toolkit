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

  v8_inspector::V8Inspector
  - associated and 'connected' to a v8::Context
  - created with a V8InspectorClient pointer (and an isolate)
  - call V8Inspector::connect(channel) to get a V8InspectorSession

v8_inspector::V8Inspector::Channel
 - Interface: sendResponse, sendNotification, flushProtocol
  - basically takes a string and sends it out



v8_inspector::V8InspectorClient
  - knows how to do work when javascript is paused

V8InspectorSession:
 - Does the "main work"
 - Received messages are sent to session::dispatchProtocolMessage
 - created when you associate a channel and a V8Inspector



 DebugContext is a V8InspectorClient because it knows what to do when paused
  - to do this, it needs to know what the channel is
  - it should have the websocket server



*/

class WebSocketService;

/**
 * Represents the actual network connection between the debugger and the V8Inspector - that connection
 *   is what constitutes a V8InspectorSession
 */
class WebSocketChannel : public v8_inspector::V8Inspector::Channel {

private:
    WebSocketService & web_socket_service;
    websocketpp::connection_hdl connection;

public:
    void sendResponse(int callId,
                              std::unique_ptr<v8_inspector::StringBuffer> message) override;

    void sendNotification(std::unique_ptr<v8_inspector::StringBuffer> message) override;

    void flushProtocolNotifications() override;


    WebSocketChannel(WebSocketService & web_socket_service, websocketpp::connection_hdl connection);
    ~WebSocketChannel();
};



class WebSocketService {
public:
    WebSocketService(v8toolkit::DebugContext & debug_context, short port);

    ~WebSocketService();


    using DebugServer = websocketpp::server<websocketpp::config::asio>;
    using WebSocketConnections = std::map<websocketpp::connection_hdl, WebSocketChannel, std::owner_less<websocketpp::connection_hdl>>;


    void send(websocketpp::connection_hdl & connection, const v8_inspector::StringView &string);

private:
    v8::Isolate *isolate;
    v8toolkit::DebugContext & debug_context;

    DebugServer debug_server;

    unsigned short port = 0;

    WebSocketConnections connections;


    bool websocket_validation_handler(websocketpp::connection_hdl hdl);

    // registered callbacks with websocketpp
    void on_open(websocketpp::connection_hdl hdl);

    void on_close(websocketpp::connection_hdl hdl);

    void on_message(websocketpp::connection_hdl hdl, DebugServer::message_ptr msg);

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

    void send_message(websocketpp::connection_hdl connection, void * data, size_t length);


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
    short port;
    std::string frame_id = "12345.1"; // arbitrary value
    std::unique_ptr<v8_inspector::V8Inspector> inspector;
    std::unique_ptr<v8_inspector::V8InspectorSession> session;
    WebSocketService web_socket_service;


public:
    DebugContext(std::shared_ptr<v8toolkit::Isolate> isolate_helper, v8::Local<v8::Context> context, short port);


    virtual void runMessageLoopOnPause(int contextGroupId) override {
        this->paused = true;
        while (this->paused) {
            this->web_socket_service.run_one();
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

    v8_inspector::V8InspectorSession & get_session() {return *this->session;}
    void reset_session();

    // misnamed for convenience
    WebSocketService & get_channel() {return this->web_socket_service;}

    short get_port(){return this->port;}

    void connect_with_channel(v8_inspector::V8Inspector::Channel * channel) {
        this->session = this->inspector->connect(1, channel, v8_inspector::StringView());
    }

};
///// END NEW DEBUG CODE

} // end v8toolkit namespace
