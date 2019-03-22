

#include <websocketpp/endpoint.hpp>

#include "v8toolkit/debugger.h"
#include "v8toolkit/log.h"


//#include <nlohmann/json.hpp>

namespace v8toolkit {

using namespace ::v8toolkit::literals;

void WebSocketChannel::sendResponse(int callId,
                          std::unique_ptr<v8_inspector::StringBuffer> message) {
    this->web_socket_service.send(this->connection, message->string());
}

void WebSocketChannel::sendNotification(std::unique_ptr<v8_inspector::StringBuffer> message) {
    this->web_socket_service.send(this->connection, message->string());
}

void WebSocketChannel::flushProtocolNotifications() {}

WebSocketChannel::WebSocketChannel(WebSocketService & web_socket_service, websocketpp::connection_hdl connection) :
    web_socket_service(web_socket_service),
    connection(connection)
{}


WebSocketChannel::~WebSocketChannel() {}

WebSocketService::WebSocketService(v8toolkit::DebugContext & debug_context, short port) :
    isolate(debug_context.get_isolate()),
    debug_context(debug_context),
    port(port)
//    ,
//            message_manager(debug_context)
{
    v8toolkit::log.info(LogT::Subjects::DebugWebSocket, "Creating WebSocketService");
    // disables all websocketpp debugging messages
    //websocketpp::set_access_channels(websocketpp::log::alevel::none);
    this->debug_server.get_alog().clear_channels(websocketpp::log::alevel::all);
    this->debug_server.get_elog().clear_channels(websocketpp::log::elevel::all);

    // only allow one connection
    this->debug_server.set_validate_handler(
            bind(&WebSocketService::websocket_validation_handler, this, websocketpp::lib::placeholders::_1));
    // store the connection so events can be sent back to the client without the client sending something first
    this->debug_server.set_open_handler(bind(&WebSocketService::on_open, this, websocketpp::lib::placeholders::_1));
    // note that the client disconnected
    this->debug_server.set_close_handler(
            bind(&WebSocketService::on_close, this, websocketpp::lib::placeholders::_1));
    // handle websocket messages from the client
    this->debug_server.set_message_handler(
            bind(&WebSocketService::on_message, this, websocketpp::lib::placeholders::_1,
                 websocketpp::lib::placeholders::_2));
    this->debug_server.set_http_handler(
            websocketpp::lib::bind(&WebSocketService::on_http, this, websocketpp::lib::placeholders::_1));
    this->debug_server.init_asio();
    this->debug_server.set_reuse_addr(true);
	v8toolkit::log.info(LogT::Subjects::DebugWebSocket, "Debug context websocket channel listening on port: {}", this->port);
    this->debug_server.listen(this->port);
    this->debug_server.start_accept();
    v8toolkit::log.info(LogT::Subjects::DebugWebSocket, "Done creating WebSocketService");

}

WebSocketService::~WebSocketService() {}

void WebSocketService::on_http(websocketpp::connection_hdl hdl) {
    WebSocketService::DebugServer::connection_ptr con = this->debug_server.get_con_from_hdl(hdl);

    std::stringstream output;
    output << "<!doctype html><html><body>You requested "
           << con->get_resource()
           << "</body></html>";

    // Set status to 200 rather than the default error code
    con->set_status(websocketpp::http::status_code::ok);
    // Set body text to the HTML created above
    con->set_body(output.str());
}


void WebSocketService::on_message(websocketpp::connection_hdl hdl, WebSocketService::DebugServer::message_ptr msg) {
    std::smatch matches;
    std::string message_payload = msg->get_payload();


    log.info(LogT::Subjects::DebugWebSocket, "Got websocket message: {}", message_payload);

//    this->message_manager.process_request_message(message_payload);
    v8_inspector::StringView message_view((uint8_t const *)message_payload.c_str(), message_payload.length());

    GLOBAL_CONTEXT_SCOPED_RUN(this->debug_context.get_isolate(), this->debug_context.get_global_context());
    this->debug_context.get_session().dispatchProtocolMessage(message_view);
    this->message_received_time = std::chrono::high_resolution_clock::now();
}


void WebSocketService::on_open(websocketpp::connection_hdl connection) {
    log.info(LogT::Subjects::DebugWebSocket, "Got websocket connection");
    assert(this->connections.size() == 0);
    this->connections.emplace(connection, WebSocketChannel(*this, connection));
    this->debug_context.connect_with_channel(&this->connections.find(connection)->second);
    this->message_received_time = std::chrono::high_resolution_clock::now();

}



void WebSocketService::on_close(websocketpp::connection_hdl hdl) {
    assert(this->connections.size() == 1);
    this->connections.erase(hdl);
    assert(this->connections.size() == 0);
    log.info(LogT::Subjects::DebugWebSocket, "Websocket connection closed");

    // not sure if this is right, but unpause when debugger disconnects
    this->debug_context.reset_session();
    this->debug_context.paused = false;
    this->message_received_time = std::chrono::high_resolution_clock::now();

}


void WebSocketService::send(websocketpp::connection_hdl & connection, const v8_inspector::StringView &string) {
    log.info(LogT::Subjects::DebugWebSocket, "Websocket sending message of length: {}", string.length());

    if (string.is8Bit()) {
        v8toolkit::log.info(LogT::Subjects::DebugWebSocket, "Sending 8-bit message: {}", xl::string_view(reinterpret_cast<char const *>(string.characters8()), string.length()));
        this->send_message(connection, (void *)string.characters8(), string.length());
    } else {
        size_t length = string.length();
        uint16_t const * source = string.characters16();
        auto buffer = std::make_unique<char[]>(length);
        for(int i = 0; i < length; i++) {
            buffer[i] = source[i];
        }
//        std::string str;
//        str.reserve(string.length() + 1);
//        str.resize(string.length());
//        char const * view_ptr = (char const *)string.characters8();
//        for (int i = 0; view_ptr[i] != '\0'; i += 2) {
//            ((char *)str.data())[i/2] = view_ptr[i];
//        }
//        std::wstring wide_string((wchar_t *) (string.characters16()), string.length());
        v8toolkit::log.info(LogT::Subjects::DebugWebSocket, "Sending stripped-down 16-bit message: {}", xl::string_view(buffer.get(), string.length()));

        this->send_message(connection, static_cast<void*>(buffer.get()), length);
    }
}

bool WebSocketService::websocket_validation_handler(websocketpp::connection_hdl hdl) {
    // only allow one connection
    log.info(LogT::Subjects::DebugWebSocket, "Websocket validation handler, checking that connection count is 0, not sure why");
    return this->connections.size() == 0;
}


void WebSocketService::send_message(websocketpp::connection_hdl connection, void * data, size_t length) {
    //    auto logger = log.to(LogT::Levels::Info, LogT::Subjects::DebugWebSocket);
    if (!this->connections.empty()) {
	//        logger("Sending data on websocket: {}", xl::string_view(reinterpret_cast<char *>(data), length));
        this->debug_server.send(connection, data, length, websocketpp::frame::opcode::TEXT);
    } else {
	//  logger("Not sending message because no connections");
    }
    this->message_sent_time = std::chrono::high_resolution_clock::now();
}


void WebSocketService::run_one() {
    this->debug_server.run_one();
}


void WebSocketService::poll() {
    this->debug_server.poll();
}


void WebSocketService::poll_one() {
    this->debug_server.poll_one();
}


void WebSocketService::poll_until_idle(float idle_time) {
    auto logger = log.to(LogT::Levels::Info, LogT::Subjects::DebugWebSocket)("Polling until idle for {} seconds", idle_time);

    do {
        this->poll();
    } while(this->seconds_since_message() < idle_time);
    logger("done polling, idle time of {} exceeded", idle_time);
}


using namespace std::literals::chrono_literals;

void WebSocketService::wait_for_connection(std::chrono::duration<float> sleep_between_polls) {
        auto logger = log.to(LogT::Levels::Info, LogT::Subjects::DebugWebSocket)("Waiting indefinitely for websocket connection");
    while(this->connections.empty()) {
        this->poll_one();
        if (this->connections.empty()) {
            // don't suck up 100% cpu while waiting for connection
            std::this_thread::sleep_for(sleep_between_polls);
        }
    }
    //    logger("Got websocket connection, finished waiting");
}


DebugContext::DebugContext(std::shared_ptr<v8toolkit::Isolate> isolate, v8::Local<v8::Context> context, short port) :
        v8toolkit::Context(isolate, context),
        port(port),
        inspector(v8_inspector::V8Inspector::create(this->get_isolate(), this)),
        web_socket_service(*this, port)
{
    auto logger = log.to(LogT::Levels::Info, LogT::Subjects::DebugWebSocket)("Creating debug context (in part from non-debug context)");
    this->reset_session();
    this->get_context()->SetAlignedPointerInEmbedderData(kInspectorClientIndex, this);

    inspector->contextCreated(v8_inspector::V8ContextInfo(*this, kContextGroupId, v8_inspector::StringView()));

    //v8::Debug::SetLiveEditEnabled(this->isolate, true);
}


float WebSocketService::seconds_since_message_received() {
    std::chrono::duration<float> duration = std::chrono::high_resolution_clock::now() - this->message_received_time;
    return duration.count();
}


float WebSocketService::seconds_since_message_sent() {
    std::chrono::duration<float> duration = std::chrono::high_resolution_clock::now() - this->message_sent_time;
    return duration.count();

}


float WebSocketService::seconds_since_message() {
    auto sent_time = this->seconds_since_message_sent();
    auto received_time = this->seconds_since_message_received();
    return sent_time < received_time ? sent_time : received_time;
}


void DebugContext::reset_session() {
    this->session.reset();
}

} // end v8toolkit namespace
