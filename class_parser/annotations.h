#pragma once

#include "clang_fwd.h"

#include <regex>
#include <iostream>
#include <fmt/ostream.h>
#include <set>
#include <string>
#include <map>

using namespace std;

namespace v8toolkit::class_parser {


class Annotations {
    set<string> annotations;

    void get_annotations_for_decl(const Decl * decl_to_check);


public:

    Annotations(const Decl * decl_to_check) {
        get_annotations_for_decl(decl_to_check);
    }


    Annotations(const CXXMethodDecl * decl_to_check);

    Annotations() = default;

    const vector<string> get() const {
        std::vector<string> results;

        for (auto & annotation : annotations) {
            results.push_back(annotation);
        }
        return results;
    }

    std::vector<string> get_regex(const string & regex_string) const {
        auto re = std::regex(regex_string);
        std::vector<string> results;

        for (auto & annotation : annotations) {
            std::smatch matches;
            if (std::regex_match(annotation, matches, re)) {
                // printf("GOT %d MATCHES\n", (int)matches.size());
                if (matches.size() > 1) {
                    results.emplace_back(matches[1]);
                }
            }
        }
        return results;
    }

    bool has(const std::string & target) const {
        return std::find(annotations.begin(), annotations.end(), target) != annotations.end();
    }

    void merge(const Annotations & other) {
//        cerr << fmt::format("Merging in {} annotations onto {} existing ones", other.get().size(), this->get().size()) << endl;
        annotations.insert(other.annotations.begin(), other.annotations.end());
    }


    // holds a list of templates and associated annotations.  These annotations will be merged with classes created
    //   from the template.  This allows metadata associated with all instantiations of a template
    static inline map<const ClassTemplateDecl *, Annotations> annotations_for_class_templates;

    // any annotations on 'using' statements should be applied to the actual CXXRecordDecl being aliased (the right side)
    static inline map<const CXXRecordDecl *, Annotations> annotations_for_record_decls;


    // if a template instantiation is named with a 'using' statement, use that alias for the type isntead of the template/class name itself
    //   this stops them all from being named the same thing - aka CppFactory, CppFactory, ...  instead of MyThingFactory, MyOtherThingFactory, ...
    static inline map<const CXXRecordDecl *, string> names_for_record_decls;

    Annotations(const CXXRecordDecl * decl_to_check);
};

}