#pragma once


#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <string>
#include <fmt/ostream.h>

#include "javascript.h"

class Debugger;

template<class T>
std::string make_response(int message_id, T const & response_object) {
    return fmt::format("{{\"id\":{},\"result\":{}}}", message_id, response_object);
}

template<class T>
std::string make_method(T const & method_object) {
    return fmt::format("{{\"method\":\"{}\",\"params\":{}}}", method_object.get_name(), method_object);
}




// https://chromedevtools.github.io/debugger-protocol-viewer/

struct PageFrame {
    PageFrame(Debugger const & debugger);
    std::string frame_id;
    std::string parent_id = "";
    std::string network_loader_id;
    std::string name; // optional
    std::string url;
    std::string security_origin;
    std::string mime_type = "text/html";
};
std::ostream& operator<<(std::ostream& os, const PageFrame & page_frame) {
    os << fmt::format("{{\"id\":\"{}\",\"parentId\":\"{}\",\"loaderId\":\"{}\",\"name\":\"{}\",\"url\":\"{}\",\"securityOrigin\":\"{}\",\"mimeType\":\"{}\"}}",
        page_frame.frame_id, page_frame.parent_id, page_frame.network_loader_id, page_frame.name, page_frame.url, page_frame.security_origin, page_frame.mime_type);
    return os;
}

struct FrameResource {
    FrameResource(Debugger const & debugger, v8toolkit::Script const & script);
    std::string url;
    std::string type = "Script";
    std::string mime_type = "text/javascript";
    //bool failed = false;
    //bool canceled = false;
};
std::ostream& operator<<(std::ostream& os, const FrameResource & frame_resource) {
    os << fmt::format("{{\"url\":\"{}\",\"type\":\"{}\",\"mimeType\":\"{}\""/*,\"failed\":{},\"canceled\":{}*/"}}",
                      frame_resource.url, frame_resource.type, frame_resource.mime_type/*, frame_resource.failed, frame_resource.canceled*/);
    return os;
}


struct Runtime_ExecutionContextDescription {
    Runtime_ExecutionContextDescription(Debugger const & debugger);
    int execution_context_id = 1;
    bool is_default=true;
    std::string origin;
    std::string name="";
    std::string frame_id;
};
std::ostream& operator<<(std::ostream& os, const Runtime_ExecutionContextDescription & context) {
    os << fmt::format("{{\"id\":{},\"isDefault\":{},\"name\":\"{}\",\"frameId\":\"{}\",\"origin\":\"{}\"}}",
                      context.execution_context_id, context.is_default, context.name, context.frame_id, context.origin);
    return os;
}

struct Runtime_ExecutionContextCreated {
    Runtime_ExecutionContextCreated(Debugger const & debugger);
    Runtime_ExecutionContextDescription execution_context_description;
    std::string get_name() const {return "Runtime.executionContextCreated";}
};
std::ostream& operator<<(std::ostream& os, const Runtime_ExecutionContextCreated & context) {
    os << fmt::format("{{\"context\":{}}}", context.execution_context_description);
    return os;
}

struct FrameResourceTree {
    FrameResourceTree(Debugger const & debugger);

    PageFrame page_frame;
    std::vector<FrameResourceTree> child_frames;
    std::vector<FrameResource> resources;
};

std::ostream& operator<<(std::ostream& os, const FrameResourceTree & frame_resource_tree) {
    os << fmt::format("{{\"frameTree\":{{\"frame\":{}", frame_resource_tree.page_frame);
    os << ",\"childFrames\":[";
    bool first = true;
    for (auto & child_frame : frame_resource_tree.child_frames) {
        if (!first) {
            os << ",";
        }
        first = false;
        os << child_frame;
    }
    os << "],";
    os << "\"resources\":[";
    first = true;
    for (auto & resource : frame_resource_tree.resources) {
        if (!first) {
            os << ",";
        }
        first = false;
        os << resource;
    }
    os << "]}}";
    return os;
}


struct Debugger_ScriptParsed {
    Debugger_ScriptParsed(v8toolkit::Script const & script);

    std::string script_id;
    std::string url; // optional
    int start_line = 0;
    int start_column = 0;
    int end_line=1;
    int end_column=4; // length of last line of script
    bool is_content_script; // optional
    std::string source_map_url;
    bool has_source_url = false;

    std::string get_name() const {return "Debugger.scriptParsed";}
};
std::ostream& operator<<(std::ostream& os, const Debugger_ScriptParsed & script_parsed) {
    os << fmt::format("{{\"scriptId\":\"{}\",\"url\":\"{}\",\"startLine\":{},\"startColumn\":{},\"endLine\":{},\"endColumn\":{}}}",
                      script_parsed.script_id, script_parsed.url, script_parsed.start_line,
                      script_parsed.start_column, script_parsed.end_line, script_parsed.end_column);
    return os;
}

struct Network_LoadingFinished {
    std::string request_id;
    double timestamp; // fractional seconds since epoch
};


// response to a getResourceContent request
struct Page_Content {
    Page_Content(std::string const & content);
    std::string content;
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
    void on_http(websocketpp::connection_hdl hdl);
    v8toolkit::ContextPtr context;

    std::string frame_id = "12345.1";
    std::string base_url = "https://dummy_base_url"; // set this from constructor

public:
    Debugger(v8toolkit::ContextPtr & context, unsigned short port);

    void poll();
    void helper(websocketpp::connection_hdl hdl);

    std::string const & get_frame_id() const;
    std::string const & get_base_url() const;
    v8toolkit::Context const & get_context() const;

};