
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

std::vector<std::string> Annotations::get_regex(const std::string & regex_string) const {
    auto re = std::regex(regex_string);
    std::vector<std::string> results;

    std::cerr << fmt::format("annotations count: {}", this->annotations.size()) << std::endl;
    if (this->annotations.size() > 0) {
        std::cerr << fmt::format("some annotations {}", this->annotations.size()) << std::endl;
        if (this->annotations.size() > 100) {
            std::cerr << fmt::format("way too big {}", this->annotations.size()) << std::endl;
        }
    }
    for (auto & annotation : this->annotations) {
        if (this->annotations.size() == 0) {
            std::cerr << fmt::format("weird") << std::endl;
        }
        std::smatch matches;
//            std::cerr << fmt::format("on annotation {}", annotation) << std::endl;
        if (std::regex_match(annotation, matches, re)) {
            if (matches.size() > 1) {
                results.emplace_back(matches[1]);
            }
        }
    }
//        std::cerr << fmt::format("done with annotations") << std::endl;
    return results;
}


const std::vector<std::string> Annotations::get() const {
    std::vector<std::string> results;

    for (auto & annotation : annotations) {
        results.push_back(annotation);
    }
    return results;
}


} // end v8toolkit::class_parser namespace