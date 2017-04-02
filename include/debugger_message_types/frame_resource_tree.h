
#pragma once

#include <string>

#include "javascript.h"
#include "debugger.h"

namespace v8toolkit {


class Page_GetResourceTree : public RequestMessage {

public:
    Page_GetResourceTree(v8toolkit::DebugContext & debug_context, nlohmann::json const & json);


    static std::string const message_name;
};

struct PageFrame {
    PageFrame(v8toolkit::DebugContext & context);

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
    FrameResource(DebugContext & debug_context, v8toolkit::Script const &script);

    std::string url;
    std::string type = "Script";
    std::string mime_type = "text/javascript";
};

std::ostream &operator<<(std::ostream &os, const FrameResource &frame_resource);


struct FrameResourceTree : public ResponseMessage {

public:
    FrameResourceTree(RequestMessage const & request_message);

    PageFrame page_frame;
    std::vector<FrameResourceTree> child_frames;
    std::vector<FrameResource> resources;
};

std::ostream &operator<<(std::ostream &os, const FrameResourceTree &frame_resource_tree);


} // end v8toolkit namespace