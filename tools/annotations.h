#pragma once


class Annotations {
    set <string> annotations;

    void get_annotations_for_decl(const Decl *decl) {
        if (!decl) { return; }
        for (auto attr : decl->getAttrs()) {
            AnnotateAttr *annotation = dyn_cast<AnnotateAttr>(attr);
            if (annotation) {
                auto attribute_attr = dyn_cast<AnnotateAttr>(attr);
                auto annotation_string = attribute_attr->getAnnotation().str();
                //if (print_logging) cerr << "Got annotation " << annotation_string << endl;
                annotations.emplace(annotation->getAnnotation().str());
            }
        }
    }


public:

    Annotations(const Decl *decl) {
        get_annotations_for_decl(decl);
    }


    Annotations(const CXXMethodDecl *decl) {
        get_annotations_for_decl(decl);

    }

    Annotations() = default;

    const vector <string> get() const {
        std::vector <string> results;

        for (auto &annotation : annotations) {
            results.push_back(annotation);
        }
        return results;
    }

    std::vector <string> get_regex(const string &regex_string) const {
        auto regex = std::regex(regex_string);
        std::vector <string> results;

        for (auto &annotation : annotations) {
            std::smatch matches;
            if (std::regex_match(annotation, matches, regex)) {
                // printf("GOT %d MATCHES\n", (int)matches.size());
                if (matches.size() > 1) {
                    results.emplace_back(matches[1]);
                }
            }
        }
        return results;
    }

    bool has(const std::string &target) const {
        return std::find(annotations.begin(), annotations.end(), target) != annotations.end();
    }

    void merge(const Annotations &other) {
        cerr << fmt::format("Merging in {} annotations onto {} existing ones", other.get().size(), this->get().size())
             << endl;
        annotations.insert(other.annotations.begin(), other.annotations.end());
    }


    static map<const ClassTemplateDecl *, Annotations> annotations_for_class_templates;

    // any annotations on 'using' statements should be applied to the actual CXXRecordDecl being aliased (the right side)
    static map<const CXXRecordDecl *, Annotations> annotations_for_record_decls;


    // if a template instantiation is named with a 'using' statement, use that alias for the type isntead of the template/class name itself
    //   this stops them all from being named the same thing - aka CppFactory, CppFactory, ...  instead of MyThingFactory, MyOtherThingFactory, ...
    static map<const CXXRecordDecl *, string> names_for_record_decls;

    Annotations(const CXXRecordDecl *decl) {
        auto name = get_canonical_name_for_decl(decl);
        get_annotations_for_decl(decl);
        cerr << "Making annotations object for " << name << endl;
        if (auto spec_decl = dyn_cast<ClassTemplateSpecializationDecl>(decl)) {
            cerr << fmt::format("{} is a template, getting any tmeplate annotations available", name) << endl;
            cerr << annotations_for_class_templates[spec_decl->getSpecializedTemplate()].get().size()
                 << " annotations available" << endl;
            merge(annotations_for_class_templates[spec_decl->getSpecializedTemplate()]);
        } else {
            cerr << "Not a template" << endl;
        }

    }
};