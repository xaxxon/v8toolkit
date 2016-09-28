#include "debugger.h"


#include <regex>


void Debugger::on_open(websocketpp::connection_hdl hdl) {

}


void Debugger::on_message(websocketpp::connection_hdl hdl, Debugger::DebugServerType::message_ptr msg) {
    std::smatch matches;
    std::string message = msg->get_payload();

    if (std::regex_match(message, matches, std::regex("\\s*\\{\"id\":(\\d+),\"method\":\"([^\"]+)\".*"))) {
        int message_id = std::stoi(matches[1]);
        std::string method_name = matches[2];

        std::string response = fmt::format("{{\"id\":{},\"result\":{{}}}}", message_id);

        if (method_name == "Page.getResourceTree") {
            std::cerr << "Message id for resource tree req: " << message_id << std::endl;
            response = fmt::format("{{\"id\":{}", message_id);
            response += ",\"result\":{\"frameTree\":{\"frame\":{\"id\":\"46273.1\",\"loaderId\":\"46273.1\",\"url\":\"file:///Users/xaxxon/Desktop/foobar.html\",\"securityOrigin\":\"file://\",\"mimeType\":\"text/html\"},\"resources\":[{\"url\":\"file:///Users/xaxxon/Desktop/foobar.html\",\"type\":\"Document\",\"mimeType\":\"text/html\"}]}}}";
        } else if (method_name == "Page.getResourceContent") {
            response = fmt::format("{{\"id\":{},{}}}", message_id, Page_Content());
        }

        this->debug_server.send(hdl, response, msg->get_opcode());


    } else {
        // unknown message format
        assert(false);
    }

}


Debugger::Debugger(v8toolkit::ContextPtr & context,
                   unsigned short port) :
        context(context),
        port(port) {
    this->debug_server.set_message_handler(bind(&Debugger::on_message, this, websocketpp::lib::placeholders::_1,websocketpp::lib::placeholders::_2));
    this->debug_server.set_open_handler(websocketpp::lib::bind(&Debugger::on_open, this, websocketpp::lib::placeholders::_1));

    this->debug_server.init_asio();
    std::cerr << "Listening on port " << this->port << std::endl;
    this->debug_server.listen(this->port);

    this->debug_server.start_accept();
}

void Debugger::poll() {
    this->debug_server.poll();
}


