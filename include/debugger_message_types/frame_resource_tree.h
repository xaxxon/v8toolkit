
#pragma once

#include <string>

#include "javascript.h"
#include "debugger.h"

namespace v8toolkit {


class Page_GetResourceTree : public RequestMessage {

public:
    Page_GetResourceTree(v8toolkit::DebugContext & debug_context, nlohmann::json const & json);


    std::unique_ptr<ResponseMessage> generate_response_message() const override;
    std::vector<std::unique_ptr<InformationalMessage> > generate_additional_messages() const override;

    static std::string const message_name;
};

struct PageFrame {
    PageFrame();
    PageFrame(v8toolkit::DebugContext & context);

    std::string frame_id;
    std::string parent_id = "";
    std::string network_loader_id;
    std::string name; // optional
    std::string url;
    std::string security_origin;
    std::string mime_type = "text/html";
};

void to_json(nlohmann::json& j, const PageFrame& p);


std::ostream &operator<<(std::ostream &os, const PageFrame &page_frame);


struct FrameResource {
    FrameResource(DebugContext & debug_context, v8toolkit::Script const &script);

    std::string url;
    std::string type = "Script";
    std::string mime_type = "text/javascript";
};

void to_json(nlohmann::json& j, const FrameResource& p);

std::ostream &operator<<(std::ostream &os, const FrameResource &frame_resource);


struct FrameResourceTree {

public:
    FrameResourceTree(DebugContext & debug_context);
    DebugContext & debug_context;
    PageFrame page_frame;
    std::vector<FrameResourceTree> child_frames;
    std::vector<FrameResource> resources;
};

void to_json(nlohmann::json& j, const FrameResourceTree & frame_resource_tree);


class Page_FrameResourceTree : public ResponseMessage {

public:
    Page_FrameResourceTree(RequestMessage const &);
    FrameResourceTree const frame_resource_tree;

    nlohmann::json to_json() const override;
};

std::ostream &operator<<(std::ostream &os, const FrameResourceTree &frame_resource_tree);


} // end v8toolkit namespace