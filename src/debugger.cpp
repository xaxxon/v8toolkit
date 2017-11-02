#include "debugger.h"


#include <regex>
#include <v8-debug.h>

#include <websocketpp/endpoint.hpp>

#include <nlohmann/json.hpp>

namespace v8toolkit {

using namespace ::v8toolkit::literals;

using json = nlohmann::json;

#if 0
//RequestMessage::RequestMessage(v8toolkit::DebugContext & context, nlohmann::json const & json) :
//        RequestResponseMessage(context, json["id"].get<int>()),
//        method_name(json["method"].get<std::string>())
//{}
//
//json RequestMessage::to_json() const {
//    throw InvalidCallException("Request message types shouldn't every be re-serialized to json");
//}
//
//ResponseMessage::ResponseMessage(RequestMessage const & request_message) :
//        RequestResponseMessage(request_message)
//{}
//
//
//MessageManager::MessageManager(v8toolkit::DebugContext & debug_context) :
//        debug_context(debug_context)
//{
////    this->add_request_message_handler<Page_GetResourceTree>();
//
//    std::cerr << fmt::format("mapped method names:") << std::endl;
//    for(auto & pair : this->message_map) {
//        std::cerr << fmt::format("{}", pair.first) << std::endl;
//    }
//};
//
//
//
//
//
//
//void MessageManager::process_request_message(std::string const & message_payload) {
//
//    json json = json::parse(message_payload);
//    std::string method_name = json["method"];
//    std::cerr << fmt::format("processing request message with method name: {}", method_name) << std::endl;
//
//    // Try to find a custom handler for the message
//    auto matching_message_pair = this->message_map.find(method_name);
//
//    // if a custom matcher is found, use that
//    if (matching_message_pair != this->message_map.end()) {
//        std::cerr << fmt::format("found custom handler for message type") << std::endl;
//        auto request_message = matching_message_pair->second(message_payload);
//
//        // send the required response message
//        this->debug_context.get_channel().send_message(nlohmann::json(*request_message->generate_response_message()).dump());
//
//        // send any other messages which may be generated based on actions taken because of RequestMessage
//        for (auto & debug_message : request_message->generate_additional_messages()) {
//            nlohmann::json json = *debug_message;
//            this->debug_context.get_channel().send_message(json.dump());
//        }
//    }
//    // otherwise, if no custom behavior is specified for this message type, send it to v8-inspector to handle
//    else {
//        std::cerr << fmt::format("sending message type to v8 inspector") << std::endl;
//        v8_inspector::StringView message_view((uint8_t const *)message_payload.c_str(), message_payload.length());
//        this->debug_context.get_session().dispatchProtocolMessage(message_view);
//    }
//}
#endif

WebsocketChannel::~WebsocketChannel() {}

WebsocketChannel::WebsocketChannel(v8toolkit::DebugContext & debug_context, short port) :
            isolate(debug_context.get_isolate()),
            debug_context(debug_context),
            port(port)
//    ,
//            message_manager(debug_context)
{
    // disables all websocketpp debugging messages
    //websocketpp::set_access_channels(websocketpp::log::alevel::none);
    this->debug_server.get_alog().clear_channels(websocketpp::log::alevel::all);
    this->debug_server.get_elog().clear_channels(websocketpp::log::elevel::all);

    // only allow one connection
    this->debug_server.set_validate_handler(
            bind(&WebsocketChannel::websocket_validation_handler, this, websocketpp::lib::placeholders::_1));

    // store the connection so events can be sent back to the client without the client sending something first
    this->debug_server.set_open_handler(bind(&WebsocketChannel::on_open, this, websocketpp::lib::placeholders::_1));

    // note that the client disconnected
    this->debug_server.set_close_handler(
            bind(&WebsocketChannel::on_close, this, websocketpp::lib::placeholders::_1));

    // handle websocket messages from the client
    this->debug_server.set_message_handler(
            bind(&WebsocketChannel::on_message, this, websocketpp::lib::placeholders::_1,
                 websocketpp::lib::placeholders::_2));
    this->debug_server.set_open_handler(
            websocketpp::lib::bind(&WebsocketChannel::on_open, this, websocketpp::lib::placeholders::_1));
    this->debug_server.set_http_handler(
            websocketpp::lib::bind(&WebsocketChannel::on_http, this, websocketpp::lib::placeholders::_1));

    this->debug_server.init_asio();
    this->debug_server.set_reuse_addr(true);
    std::cerr << "Listening on port " << this->port << std::endl;
    this->debug_server.listen(this->port);

    this->debug_server.start_accept();

}

void WebsocketChannel::on_http(websocketpp::connection_hdl hdl) {
    WebsocketChannel::DebugServerType::connection_ptr con = this->debug_server.get_con_from_hdl(hdl);

    std::stringstream output;
    output << "<!doctype html><html><body>You requested "
           << con->get_resource()
           << "</body></html>";

    // Set status to 200 rather than the default error code
    con->set_status(websocketpp::http::status_code::ok);
    // Set body text to the HTML created above
    con->set_body(output.str());
}


void WebsocketChannel::on_message(websocketpp::connection_hdl hdl, WebsocketChannel::DebugServerType::message_ptr msg) {
    std::smatch matches;
    std::string message_payload = msg->get_payload();
    std::cerr << "Got debugger message: " << message_payload << std::endl;
//    this->message_manager.process_request_message(message_payload);
    v8_inspector::StringView message_view((uint8_t const *)message_payload.c_str(), message_payload.length());

    GLOBAL_CONTEXT_SCOPED_RUN(this->debug_context.get_isolate(), this->debug_context.get_global_context());
    this->debug_context.get_session().dispatchProtocolMessage(message_view);
    this->message_received_time = std::chrono::high_resolution_clock::now();
}


void WebsocketChannel::on_open(websocketpp::connection_hdl hdl) {
    std::cerr << fmt::format("on open") << std::endl;
    assert(this->connections.size() == 0);
    this->connections.insert(hdl);
}



void WebsocketChannel::on_close(websocketpp::connection_hdl hdl) {
    assert(this->connections.size() == 1);
    this->connections.erase(hdl);
    assert(this->connections.size() == 0);

    // not sure if this is right, but unpause when debugger disconnects
    this->debug_context.reset_session();
    this->debug_context.paused = false;
}


void WebsocketChannel::Send(const v8_inspector::StringView &string) {
    std::cerr << fmt::format("sending message of length: {}", string.length()) << std::endl;
    std::cerr << string.characters8() << std::endl;

    if (string.is8Bit()) {
        this->send_message((void *)string.characters8(), string.length());
    } else {
        size_t length = string.length();
        auto source= string.characters16();
        auto buffer = new char[length];
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
        this->send_message((void*)buffer, length);
        delete [] buffer;
    }
}

bool WebsocketChannel::websocket_validation_handler(websocketpp::connection_hdl hdl) {
    // only allow one connection
    std::cerr << fmt::format("validation handler") << std::endl;
    return this->connections.size() == 0;
}
//
//void WebsocketChannel::send_message(std::wstring const & message) {
//    std::wcerr << message << std::endl;
//
//     if (!this->connections.empty()) {
//
//        std::cerr << fmt::format("Sending wide message: {}", str) << std::endl;
//        this->debug_server.send(*this->connections.begin(), str, websocketpp::frame::opcode::TEXT);
//    } else {
//        std::cerr << fmt::format("Not sending wide message because no connections: {}", str) << std::endl;
//    }
//}

void WebsocketChannel::send_message(void * data, size_t length) {
    if (!this->connections.empty()) {
//        std::cerr << fmt::format("Sending message: {}", message) << std::endl;
        this->debug_server.send(*this->connections.begin(), data, length, websocketpp::frame::opcode::TEXT);
    } else {
        std::cerr << fmt::format("Not sending message because no connections: {}") << std::endl;
    }
    this->message_sent_time = std::chrono::high_resolution_clock::now();
}


void WebsocketChannel::run_one() {
    this->debug_server.run_one();
}

void WebsocketChannel::poll() {
    this->debug_server.poll();
}

void WebsocketChannel::poll_one() {
    this->debug_server.poll_one();
}

void WebsocketChannel::poll_until_idle(float idle_time) {
    do {
        this->poll();
    } while(this->seconds_since_message() > idle_time);
}


using namespace std::literals::chrono_literals;

void WebsocketChannel::wait_for_connection(std::chrono::duration<float> sleep_between_polls) {
    while(this->connections.empty()) {
        this->poll_one();
        if (this->connections.empty()) {
            // don't suck up 100% cpu while waiting for connection
            std::this_thread::sleep_for(sleep_between_polls);
        }
    }
}

void DebugContext::reset_session() {
}

DebugContext::DebugContext(std::shared_ptr<v8toolkit::Isolate> isolate_helper, v8::Local<v8::Context> context, short port) :
        v8toolkit::Context(isolate_helper, context),
        channel(std::make_unique<WebsocketChannel>(*this, port)),
        inspector(v8_inspector::V8Inspector::create(this->get_isolate(), this)),
        session(inspector->connect(1, channel.get(), v8_inspector::StringView())),
        port(port)
{


    this->reset_session();
    this->get_context()->SetAlignedPointerInEmbedderData(kInspectorClientIndex, this);

    inspector->contextCreated(v8_inspector::V8ContextInfo(*this, kContextGroupId, v8_inspector::StringView()));

    v8::Debug::SetLiveEditEnabled(this->isolate, true);

}

#if 0
void to_json(nlohmann::json &j, const ResponseMessage & response_message) {
    j = {
        {"id",     response_message.message_id},
        {"result", response_message.to_json()}
    };
}


void to_json(nlohmann::json &j, const InformationalMessage & informational_message) {
    j = informational_message.to_json();
}
#endif


float WebsocketChannel::seconds_since_message_received() {
    std::chrono::duration<float> duration = std::chrono::high_resolution_clock::now() - this->message_received_time;
    return duration.count();
}

float WebsocketChannel::seconds_since_message_sent() {
    std::chrono::duration<float> duration = std::chrono::high_resolution_clock::now() - this->message_sent_time;
    return duration.count();

}


float WebsocketChannel::seconds_since_message() {
    auto sent_time = this->seconds_since_message_sent();
    auto received_time = this->seconds_since_message_received();
    return sent_time < received_time ? sent_time : received_time;
}


} // end v8toolkit namespace