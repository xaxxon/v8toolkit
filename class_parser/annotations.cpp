
#include "annotations.h"
#include "helper_functions.h"

namespace v8toolkit::class_parser {


Annotations::Annotations(const CXXRecordDecl * decl_to_check) {
    auto name = get_canonical_name_for_decl(decl_to_check);
    get_annotations_for_decl(decl_to_check);
//    cerr << "Making annotations object for " << name << endl;
    if (auto spec_decl = dyn_cast<ClassTemplateSpecializationDecl>(decl_to_check)) {
//        cerr << fmt::format("{} is a template, getting any tmeplate annotations available", name) << endl;
//        cerr << annotations_for_class_templates[spec_decl->getSpecializedTemplate()].get().size()
//             << " annotations available" << endl;
        merge(annotations_for_class_templates[spec_decl->getSpecializedTemplate()]);
    } else {
//        cerr << "Not a template" << endl;
    }

}

} // end v8toolkit::class_parser namespace