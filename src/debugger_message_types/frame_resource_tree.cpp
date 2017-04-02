
#include "debugger_message_types/frame_resource_tree.h"

using namespace v8toolkit;

std::string const Page_GetResourceTree::message_name = "Page.getResourceTree";

Page_GetResourceTree::Page_GetResourceTree(v8toolkit::DebugContext & debug_context, nlohmann::json const & json) :
        RequestMessage(debug_context, json)
{}


PageFrame::PageFrame(v8toolkit::DebugContext & debug_context) :
    frame_id(debug_context.get_frame_id()),
    network_loader_id(debug_context.get_frame_id()),
    security_origin(fmt::format("v8toolkit://{}", debug_context.get_base_url())),
    url(fmt::format("v8toolkit://{}/", debug_context.get_base_url()))
{}



FrameResource::FrameResource(v8toolkit::DebugContext & debug_context, v8toolkit::Script const &script) : url(
    script.get_source_location())
{}


FrameResourceTree::FrameResourceTree(RequestMessage const & request_message) :
    ResponseMessage(request_message),
    page_frame(this->debug_context)
{
    // nothing to do for child frames at this point, it will always be empty for now

    for (auto &script : this->debug_context.get_scripts()) {
        this->resources.emplace_back(FrameResource(this->debug_context, *script));
    }
}

