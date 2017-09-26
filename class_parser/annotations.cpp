
#include "annotations.h"
#include "helper_functions.h"

namespace v8toolkit::class_parser {

map<const ClassTemplateDecl *, Annotations> Annotations::annotations_for_class_templates;

// any annotations on 'using' statements should be applied to the actual CXXRecordDecl being aliased (the right side)
map<const CXXRecordDecl *, Annotations> Annotations::annotations_for_record_decls;


// if a template instantiation is named with a 'using' statement, use that alias for the type isntead of the template/class name itself
//   this stops them all from being named the same thing - aka CppFactory, CppFactory, ...  instead of MyThingFactory, MyOtherThingFactory, ...
map<const CXXRecordDecl *, string> Annotations::names_for_record_decls;

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