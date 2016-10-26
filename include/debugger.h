#pragma once


#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <string>
#include <sstream>
#include <fmt/ostream.h>

#include <v8-debug.h>


#include "javascript.h"

namespace v8toolkit {

using namespace ::std::chrono_literals;

class Debugger;

template<class T>
std::string make_response(int message_id, T const &response_object) {
    return fmt::format("{{\"id\":{},\"result\":{}}}", message_id, response_object);
}

template<class T>
std::string make_method(T const &method_object) {
    return fmt::format("{{\"method\":\"{}\",\"params\":{}}}", method_object.get_name(), method_object);
}




// https://chromedevtools.github.io/debugger-protocol-viewer/

struct PageFrame {
    PageFrame(Debugger const &debugger);

    std::string frame_id;
    std::string parent_id = "";
    std::string network_loader_id;
    std::string name; // optional
    std::string url;
    std::string security_origin;
    std::string mime_type = "text/html";
};

std::ostream &operator<<(std::ostream &os, const PageFrame &page_frame);


struct FrameResource {
    FrameResource(Debugger const &debugger, v8toolkit::Script const &script);

    std::string url;
    std::string type = "Script";
    std::string mime_type = "text/javascript";
    //bool failed = false;
    //bool canceled = false;
};

std::ostream &operator<<(std::ostream &os, const FrameResource &frame_resource);

struct Runtime_ExecutionContextDescription {
    Runtime_ExecutionContextDescription(Debugger const &debugger);

    int execution_context_id = 1;
    bool is_default = true;
    std::string origin;
    std::string name = "";
    std::string frame_id;
};

std::ostream &operator<<(std::ostream &os, const Runtime_ExecutionContextDescription &context);

struct Runtime_ExecutionContextCreated {
    Runtime_ExecutionContextCreated(Debugger const &debugger);

    Runtime_ExecutionContextDescription execution_context_description;

    std::string get_name() const { return "Runtime.executionContextCreated"; }
};

std::ostream &operator<<(std::ostream &os, const Runtime_ExecutionContextCreated &context);



struct FrameResourceTree {
    FrameResourceTree(Debugger const &debugger);

    PageFrame page_frame;
    std::vector<FrameResourceTree> child_frames;
    std::vector<FrameResource> resources;
};

std::ostream &operator<<(std::ostream &os, const FrameResourceTree &frame_resource_tree);

struct ScriptSource {
    ScriptSource(v8toolkit::Script const &script);
    ScriptSource(v8::Local<v8::Function> function);

    std::string source;
};

std::ostream &operator<<(std::ostream &os, const ScriptSource &script_source);


struct Debugger_ScriptParsed {
    Debugger_ScriptParsed(Debugger const &debugger, v8toolkit::Script const &script);
    Debugger_ScriptParsed(Debugger const &debugger, v8::Local<v8::Function> const function);

    int64_t script_id;
    std::string url; // optional
    int start_line = 0;
    int start_column = 0;
    int end_line;
    int end_column; // length of last line of script
    bool is_content_script; // optional
    std::string source_map_url;
    bool has_source_url = false;

    std::string get_name() const { return "Debugger.scriptParsed"; }
};

std::ostream &operator<<(std::ostream &os, const Debugger_ScriptParsed &script_parsed);

struct Network_LoadingFinished {
    std::string request_id;
    double timestamp; // fractional seconds since epoch
};


// response to a getResourceContent request
struct Page_Content {
    Page_Content(std::string const &content);

    std::string content;
    bool base64_encoded = false;
};

std::ostream &operator<<(std::ostream &os, const Page_Content &content);


// https://chromedevtools.github.io/debugger-protocol-viewer/tot/Runtime/#type-RemoteObject
struct RemoteObject {
    RemoteObject(v8::Isolate *isolate, v8::Local<v8::Value> value);

    // object, function, undefined, string, number, boolean, symbol
    std::string type;
    std::string value_string;
    std::string subtype; // optional - only used for object types
    std::string className; // constructor name
    std::string description; // string representation of the value
    bool exception_thrown = false;
};

std::ostream &operator<<(std::ostream &os, const RemoteObject &remote_object);

struct Location {
    Location(int64_t script_id, int line_number, int column_number = 0);

    int64_t script_id;
    int line_number;
    int column_number;
};

std::ostream &operator<<(std::ostream &os, const Location &location);


struct Breakpoint {

    Breakpoint(std::string const & location, int64_t script_id, int line_number, int column_number = 0  );

    std::string breakpoint_id;
    std::vector<Location> locations;
};

std::ostream &operator<<(std::ostream &os, const Breakpoint &breakpoint);


struct Scope {
};

struct CallFrame {
    CallFrame(v8::Local<v8::StackFrame> stack_frame, v8::Isolate *isolate, v8::Local<v8::Value>);

    std::string call_frame_id = "bogus call frame id";
    std::string function_name;
    Location location;
    std::vector<Scope> scope_chain; // ?
    RemoteObject javascript_this; // attribute name is just 'this'
};

std::ostream &operator<<(std::ostream &os, const CallFrame &call_frame);


struct Debugger_Paused {
    Debugger_Paused(Debugger const &debugger, v8::Local<v8::StackTrace> stack_trace, int64_t script_id,
                    int line_number, int column_number = 0);

    Debugger_Paused(Debugger const &debugger, v8::Local<v8::StackTrace> stack_trace);

    std::vector<CallFrame> call_frames;

    // XHR, DOM, EventListener, exception, assert, debugCommand, promiseRejection, other.
    std::string reason = "other";
    std::vector<std::string> hit_breakpoints;

    static std::string get_name() { return "Debugger.paused"; }
};

std::ostream &operator<<(std::ostream &os, const Debugger_Paused &paused);

struct Debugger_Resumed {
    // No fields
};

std::ostream &operator<<(std::ostream &os, const Debugger_Resumed &resumed);


class Debugger {
    using DebugServerType = websocketpp::server<websocketpp::config::asio>;

    DebugServerType debug_server;
    unsigned short port = 0;

    using WebSocketConnections = std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>>;
    WebSocketConnections connections;

    void send_message(std::string const &message);


    bool websocket_validation_handler(websocketpp::connection_hdl hdl);

    void on_open(websocketpp::connection_hdl hdl);

    void on_close(websocketpp::connection_hdl hdl);

    void on_message(websocketpp::connection_hdl hdl, DebugServerType::message_ptr msg);

    void on_http(websocketpp::connection_hdl hdl);

    v8toolkit::ContextPtr context;

    std::string frame_id = "12345.1";


    struct DebugEventCallbackData {
        DebugEventCallbackData(Debugger *debugger) : debugger(debugger) {}

        Debugger *debugger;
    };


    static void debug_event_callback(v8::Debug::EventDetails const &event_details);

    bool paused_on_breakpoint = false;

    // only valid if paused_on_breakpoint == true
    int64_t breakpoint_paused_on = -1;

    // only valid if paused_on_breakpoint == true
    v8::Global<v8::Object> breakpoint_execution_state;

    // helper function that set everything to be ready for resuming javascript execution
    void resume_execution();

public:
    static int STACK_TRACE_DEPTH;

    Debugger(v8toolkit::ContextPtr &context, unsigned short port);
    Debugger(Debugger const &) = delete; // not copyable
    Debugger & operator=(Debugger const &) = delete;
    Debugger(Debugger &&) = default; // moveable
    Debugger & operator=(Debugger &&) = default;

    void poll();
    void poll_one();

    void helper(websocketpp::connection_hdl hdl);

    std::string const &get_frame_id() const;

    std::string get_base_url() const;

    v8toolkit::Context &get_context() const;

    void wait_for_connection(std::chrono::duration<float> sleep_between_polls = std::chrono::milliseconds(200));
};


};