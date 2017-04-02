#include "debugger.h"


#include <regex>
#include <v8-debug.h>

#include <websocketpp/endpoint.hpp>

#include <nlohmann/json.hpp>
#include <codecvt>

namespace v8toolkit {

using namespace ::v8toolkit::literals;

using json = nlohmann::json;

WebsocketChannel::~WebsocketChannel() {}


WebsocketChannel::WebsocketChannel(v8::Local<v8::Context> context, short port) :
            isolate(context->GetIsolate()),
            port(port),
            context(isolate, context) {
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
    std::string message = msg->get_payload();
    std::cerr << "Got debugger message: " << message << std::endl;

    nlohmann::json json = json::parse(msg->get_payload());

    InspectorClient::SendInspectorMessage(this->context.Get(this->isolate), message);
}


void WebsocketChannel::on_open(websocketpp::connection_hdl hdl) {
    std::cerr << fmt::format("on open") << std::endl;
    assert(this->connections.size() == 0);
    this->connections.insert(hdl);
}



void WebsocketChannel::on_close(websocketpp::connection_hdl hdl) {
    std::cerr << fmt::format("on close") << std::endl;
    assert(this->connections.size() == 1);
    this->connections.erase(hdl);
    assert(this->connections.size() == 0);

}


void WebsocketChannel::Send(const v8_inspector::StringView &string) {
    std::cerr << fmt::format("sending message of length: {}", string.length()) << std::endl;
    std::cerr << string.characters8() << std::endl;

    if (string.is8Bit()) {
        this->send_message((char *) (string.characters8()));
    } else {
        std::string str;
        str.reserve(string.length() + 1);
        str.resize(string.length());
        char const * view_ptr = (char const *)string.characters8();
        for (int i = 0; view_ptr[i] != '\0'; i += 2) {
            ((char *)str.data())[i/2] = view_ptr[i];
        }
        std::wstring wide_string((wchar_t *) (string.characters16()), string.length());
        this->send_message(str);
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

void WebsocketChannel::send_message(std::string const & message) {
    if (!this->connections.empty()) {
        std::cerr << fmt::format("Sending message: {}", message) << std::endl;
        this->debug_server.send(*this->connections.begin(), message, websocketpp::frame::opcode::TEXT);
    } else {
        std::cerr << fmt::format("Not sending message because no connections: {}", message) << std::endl;
    }

}



void WebsocketChannel::poll() {
    this->debug_server.poll();
}

void WebsocketChannel::poll_one() {
    this->debug_server.poll_one();
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


}