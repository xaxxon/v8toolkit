#pragma once


#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <string>
#include <sstream>
#include <fmt/ostream.h>

#include <v8-debug.h>


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

struct ScriptSource {
    ScriptSource(v8toolkit::Script const & script);
    std::string source;
};
std::ostream& operator<<(std::ostream& os, const ScriptSource & script_source) {
    os << fmt::format("{{\"scriptSource\":{}}}", script_source.source);
    return os;
}


struct Debugger_ScriptParsed {
    Debugger_ScriptParsed(Debugger const & debugger, v8toolkit::Script const & script);

    int64_t script_id;
    std::string url; // optional
    int start_line = 0;
    int start_column = 0;
    int end_line;
    int end_column; // length of last line of script
    bool is_content_script; // optional
    std::string source_map_url;
    bool has_source_url = false;

    std::string get_name() const {return "Debugger.scriptParsed";}
};
std::ostream& operator<<(std::ostream& os, const Debugger_ScriptParsed & script_parsed) {
    os << fmt::format("{{\"scriptId\":\"{}\",\"url\":\"{}\",\"startLine\":{},\"startColumn\":{}"/*,\"endLine\":{},\"endColumn\":{}*/"}}",
                      script_parsed.script_id, script_parsed.url, script_parsed.start_line,
                      script_parsed.start_column/*, script_parsed.end_line, script_parsed.end_column*/);
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

// https://chromedevtools.github.io/debugger-protocol-viewer/tot/Runtime/#type-RemoteObject
struct RemoteObject {
    RemoteObject(v8::Isolate * isolate, v8::Local<v8::Value> value);
    // object, function, undefined, string, number, boolean, symbol
    std::string type;
    std::string value_string;
    std::string subtype; // optional - only used for object types
    std::string className; // constructor name
    std::string description; // string representation of the value
    bool exception_thrown = false;
};
std::ostream& operator<<(std::ostream& os, const RemoteObject & remote_object) {
    os << fmt::format("{{\"result\":{{\"type\":\"{}\",\"value\":{},\"description\":\"{}\"}},\"wasThrown\":{}}}", remote_object.type, remote_object.value_string, remote_object.description, remote_object.exception_thrown);
    return os;
}

struct Location {
    Location(int64_t script_id, int line_number, int column_number);
    int64_t script_id;
    int line_number;
    int column_number;
};
std::ostream& operator<<(std::ostream& os, const Location & location) {
    os << fmt::format("{{\"scriptId\":\"{}\",\"lineNumber\":{},\"columnNumber\":{}}}",
                      location.script_id, location.line_number, location.column_number);
    return os;
}


struct Breakpoint {
    Breakpoint(v8toolkit::Script const & script, int line_number, int column_number = 0);
    std::string breakpoint_id;
    std::vector<Location> locations;
};
std::ostream& operator<<(std::ostream& os, const Breakpoint & breakpoint) {
    std::stringstream locations;
    locations << "[";
    bool first = true;
    for (auto const & location : breakpoint.locations) {
        if (!first) {
            locations << ",";
        }
        first = false;
        locations << location;
    }
    locations << "]";

    os << fmt::format("{{\"breakpointId\":\"{}\",\"locations\":{}}}", breakpoint.breakpoint_id, locations.str());
    return os;
}


struct Scope {};

struct CallFrame {
    std::string call_frame_id;
    std::string function_name;
    Location location;
    std::vector<Scope> scope_chain; // ?
    RemoteObject javascript_this; // attribute name is just 'this'
};
std::ostream& operator<<(std::ostream& os, const CallFrame & call_frame) {
    assert(false);
}

struct Debugger_Paused {
    Debugger_Paused(Debugger const & debugger, int64_t script_id, int line_number, int column_number = 0);
    std::vector<CallFrame> call_frames;

    // XHR, DOM, EventListener, exception, assert, debugCommand, promiseRejection, other.
    std::string reason = "other";
    std::vector<std::string> hit_breakpoints;

    static std::string get_name(){return "Debugger.paused";}
};
std::ostream& operator<<(std::ostream& os, const Debugger_Paused & paused) {
    /*
     {
         "method":"Debugger.paused",
         "params":{
            "callFrames":[
                {
                    "callFrameId":"{\"ordinal\":0,\"injectedScriptId\":2}",
                     "functionName":"",
                     "functionLocation":{"scriptId":"70","lineNumber":0,"columnNumber":38},
                     "location":{"scriptId":"70","lineNumber":1,"columnNumber":0},
                     "scopeChain":[
                        {
                            "type":"local",
                            "object":{
                                "type":"object",
                                "className":"Object",
                                "description":"Object",
                                "objectId":"{\"injectedScriptId\":2,\"id\":1}"
                            },
                            "startLocation":{
                                "scriptId":"70",
                                "lineNumber":0,
                                "columnNumber":38
                            },
                            "endLocation":{
                                "scriptId":"70",
                                "lineNumber":517,
                                "columnNumber":126
                            }
                        },
                        {"type":"global","object":{"type":"object","className":"Window","description":"Window","objectId":"{\"injectedScriptId\":2,\"id\":2}"}}
                    ],
                    "this":{
                        "type":"object",
                        "className":"Window",
                        "description":"Window",
                        "objectId":"{\"injectedScriptId\":2,\"id\":3}"
                    }
                },
                // another call frame on this line, same as above
                {"callFrameId":"{\"ordinal\":1,\"injectedScriptId\":2}","functionName":"","functionLocation":{"scriptId":"70","lineNumber":0,"columnNumber":0},"location":{"scriptId":"70","lineNumber":517,"columnNumber":127},"scopeChain":[{"type":"global","object":{"type":"object","className":"Window","description":"Window","objectId":"{\"injectedScriptId\":2,\"id\":4}"}}],"this":{"type":"object","className":"Window","description":"Window","objectId":"{\"injectedScriptId\":2,\"id\":5}"}}
            ], // end callFrames
            "reason":"other",
            "hitBreakpoints":[
                "https://ssl.gstatic.com/sites/p/2a2c4f/system/js/jot_min_view__en.js:1:0"
            ] // end hitBreakpoints
        } // end params
     } // end message
     */
    // callFrames array should be populated, but not implemented yet, don't know how, not sure if absolutely req'd
    os << fmt::format("{{\"callFrames\":[],\"reason\":\"{}\",\"hitBreakpoints\":[", paused.reason);
    bool first = true;
    for (auto const & breakpoint : paused.hit_breakpoints) {
        if (!first) {
            os << ",";
        }
        first = false;
        os << breakpoint;
    }
    os << "]}";
    return os;
}

struct Debugger_Resumed {
    // No fields
};
std::ostream& operator<<(std::ostream& os, const Debugger_Resumed & resumed) {
    assert(false);
}



class Debugger {
    using DebugServerType = websocketpp::server<websocketpp::config::asio>;

    DebugServerType debug_server;
    unsigned short port = 0;

    using WebSocketConnections = std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>>;
    WebSocketConnections connections;
    void send_message(std::string const & message);


    bool websocket_validation_handler(websocketpp::connection_hdl hdl);

    void on_open(websocketpp::connection_hdl hdl);
    void on_close(websocketpp::connection_hdl hdl);
    void on_message(websocketpp::connection_hdl hdl, DebugServerType::message_ptr msg);
    void on_http(websocketpp::connection_hdl hdl);
    v8toolkit::ContextPtr context;

    std::string frame_id = "12345.1";


    struct DebugEventCallbackData {
        DebugEventCallbackData(Debugger * debugger) : debugger(debugger) {}
        Debugger * debugger;
    };


    static void debug_event_callback(v8::Debug::EventDetails const & event_details);

    bool paused_on_breakpoint = false;

public:
    Debugger(v8toolkit::ContextPtr & context, unsigned short port);

    void poll();
    void helper(websocketpp::connection_hdl hdl);

    std::string const & get_frame_id() const;
    std::string get_base_url() const;
    v8toolkit::Context & get_context() const;



};