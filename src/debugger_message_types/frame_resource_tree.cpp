//
//#include "debugger_message_types/frame_resource_tree.h"
//
//namespace v8toolkit {
//
//std::string const Page_GetResourceTree::message_name = "Page.getResourceTree";
//
//Page_GetResourceTree::Page_GetResourceTree(v8toolkit::DebugContext &debug_context, nlohmann::json const &json) :
//    RequestMessage(debug_context, json) {}
//
//
//std::unique_ptr<ResponseMessage> Page_GetResourceTree::generate_response_message() const {
//    return std::make_unique<Page_FrameResourceTree>(*this);
//}
//
//
//std::vector<std::unique_ptr<InformationalMessage> > Page_GetResourceTree::generate_additional_messages() const {
//    return {};
//}
//
//
//PageFrame::PageFrame(v8toolkit::DebugContext &debug_context) :
//    frame_id(debug_context.get_frame_id()),
//    network_loader_id(debug_context.get_frame_id()),
//    security_origin(fmt::format("v8toolkit://{}", debug_context.get_base_url())),
//    url(fmt::format("v8toolkit://{}/", debug_context.get_base_url())) {}
//
//
//FrameResource::FrameResource(v8toolkit::DebugContext &debug_context, v8toolkit::Script const &script) : url(
//    script.get_source_location()) {}
//
//
//FrameResourceTree::FrameResourceTree(DebugContext &debug_context) :
//    debug_context(debug_context),
//    page_frame(this->debug_context) {
//    // nothing to do for child frames at this point, it will always be empty for now
//
//    for (auto &script : this->debug_context.get_scripts()) {
//        this->resources.emplace_back(FrameResource(this->debug_context, *script));
//    }
//}
//
//
//Page_FrameResourceTree::Page_FrameResourceTree(RequestMessage const &request_message) :
//    ResponseMessage(request_message),
//    frame_resource_tree(this->debug_context) {
//
//}
//
//nlohmann::json Page_FrameResourceTree::to_json() const {
//    return {this->frame_resource_tree};
//}
//
//
//void to_json(nlohmann::json &j, const FrameResourceTree &frame_resource_tree) {
//    j = {
//        "frameTree", {{"frame", frame_resource_tree.page_frame},
//                       {"childFrames", frame_resource_tree.child_frames},
//                          {"resources", frame_resource_tree.resources}}
//    };
//}
//
//
//void to_json(nlohmann::json &j, const PageFrame & page_frame) {
//    j = {{"id", page_frame.frame_id},
//         {"parentId", page_frame.parent_id},
//         {"loaderId", page_frame.network_loader_id},
//         {"name", page_frame.name},
//         {"url", page_frame.url},
//         {"securityOrigin", page_frame.security_origin},
//         {"mimeType", page_frame.mime_type}
//    };
//}
//
//
//void to_json(nlohmann::json &j, const FrameResource & frame_resource) {
//    j = {{"url", frame_resource.url},
//         {"type", frame_resource.type},
//         {"mimeType", frame_resource.mime_type}
//    };
//}
//
//} // end v8toolkit namespace