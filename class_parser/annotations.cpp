
#include "annotations.h"
#include "helper_functions.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#include "clang/AST/DeclBase.h"
#include "clang/AST/Type.h"
#include "clang/AST/Attr.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#pragma clang diagnostic pop


namespace v8toolkit::class_parser {


void Annotations::get_annotations_for_decl(const Decl * decl_to_check) {
    if (!decl_to_check) { return; }
    for (auto attr : decl_to_check->getAttrs()) {
        if (AnnotateAttr * annotation = dyn_cast<AnnotateAttr>(attr)) {
            auto attribute_attr = dyn_cast<AnnotateAttr>(attr);
            auto annotation_string = attribute_attr->getAnnotation().str();
//            std::cerr << "Got annotation " << annotation_string << std::endl;
            annotations.emplace(annotation->getAnnotation().str());
        }
    }
}


Annotations::Annotations(const CXXRecordDecl * decl_to_check) {
    auto name = get_canonical_name_for_decl(decl_to_check);
//    std::cerr << "Making annotations object for " << name << std::endl;


    get_annotations_for_decl(decl_to_check);
    if (auto spec_decl = dyn_cast<ClassTemplateSpecializationDecl>(decl_to_check)) {
//        std::cerr << fmt::format("{} is a template, getting any template annotations available", name) << std::endl;
//        std::cerr << annotations_for_class_templates[spec_decl->getSpecializedTemplate()].get().size() << " annotations available" << std::endl;
        merge(annotations_for_class_templates[spec_decl->getSpecializedTemplate()]);
    } else {
//        std::cerr << "Not a template" << std::endl;
    }
}




Annotations::Annotations(const CXXMethodDecl * decl_to_check) {
    get_annotations_for_decl(decl_to_check);
}






} // end v8toolkit::class_parser namespace