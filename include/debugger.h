#pragma once


#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <string>
#include <fmt/ostream.h>

#include "javascript.h"





// https://chromedevtools.github.io/debugger-protocol-viewer/

struct PageFrame {
    std::string id;
    std::string parent_id;
    std::string network_loader_id;
    std::string name; // optional
    std::string url;
    std::string security_origin;
    std::string mime_type;
};
std::ostream& operator<<(std::ostream& os, const PageFrame & page_frame) {
    os << fmt::format("{{\"id\":\"{}\",\"parentId\":\"{}\",\"loaderId\":\"{}\",\"name\":\"{}\",\"url\":\"{}\",\"securityOrigin\":\"{}\",\"mimeType\":\"{}\"}}",
        page_frame.id, page_frame.parent_id, page_frame.network_loader_id, page_frame.name, page_frame.url, page_frame.security_origin, page_frame.mime_type);
    return os;
}

struct FrameResource {
    std::string url;
    std::string type;
    std::string mime_type;
    bool failed;
    bool canceled;
};
std::ostream& operator<<(std::ostream& os, const FrameResource & frame_resource) {
    os << fmt::format("{{\"url\":\"{}\",\"type\":\"{}\",\"mimeType\":\"{}\",\"failed\":\"{}\",\"canceled\":\"{}\"}}",
                      frame_resource.url, frame_resource.type, frame_resource.mime_type, frame_resource.failed, frame_resource.canceled);
    return os;
}


struct FrameResourceTree {
    PageFrame page_frame;
    std::vector<FrameResourceTree> child_frames;
    std::vector<FrameResource> resources;
};

std::ostream& operator<<(std::ostream& os, const FrameResourceTree & frame_resource_tree) {
    os << "{";
    os << frame_resource_tree.page_frame;
    os << ", {\"childFrames\": [";
    bool first = true;
    for (auto & child_frame : frame_resource_tree.child_frames) {
        if (!first) {
            os << ",";
        }
        first = false;
        os << child_frame;
    }
    os << "],";
    first = true;
    for (auto & resource : frame_resource_tree.resources) {
        if (!first) {
            os << ",";
        }
        first = false;
        os << resource;
    }
    os << "}";
    return os;
}

struct ExecutionContextDescription {
    std::string id;
    bool is_page_context;
    std::string name;
    std::string frame_id;
};


struct ScriptParsed {
    std::string script_id;
    std::string url; // optional
    int start_line = 0;
    int start_column = 0;
    int end_line;
    int end_column; // length of last line of script
    bool is_content_script; // optional
    std::string source_map_url;
    std::string has_source_url;
};

struct Network_LoadingFinished {
    std::string request_id;
    double timestamp; // fractional seconds since epoch
};


// response to a getResourceContent request
struct Page_Content {
    std::string content = "<html><head><title>fake title</title></head><body>fake body</body></html>";
    bool base64_encoded = false;
};
std::ostream& operator<<(std::ostream& os, const Page_Content & content) {
    os << "\"result\":{";
    os << fmt::format("\"content\":\"{}\",\"base64Encoded\":{}", content.content, (content.base64_encoded ? "true" : "false"));
    os << "}";
    return os;
}


class Debugger {
    using DebugServerType = websocketpp::server<websocketpp::config::asio>;

    DebugServerType debug_server;
    unsigned short port = 0;

    void on_open(websocketpp::connection_hdl hdl);
    void on_message(websocketpp::connection_hdl hdl, DebugServerType::message_ptr msg);

    v8toolkit::ContextPtr context;

public:
    Debugger(v8toolkit::ContextPtr & context, unsigned short port);

    void poll();


};