
#pragma once

#include <sstream>
#include <set>

#include "parsed_method.h"
#include "annotations.h"


namespace v8toolkit::class_parser {


extern int MAX_DECLARATIONS_PER_FILE;

// should be named "ParsedClass" or something, since both classes that will and will not be wrapped
//   are put into this data structure
class MemberFunction;

class StaticFunction;

struct WrappedClass {
private:

    bool methods_parsed = false;
    set<unique_ptr<MemberFunction>> member_functions;
    set<unique_ptr<StaticFunction>> static_functions;

    bool members_parsed = false;
    set<unique_ptr<DataMember>> members;

    bool enums_parsed = false;
    map<string, map<string, int>> enums;

    set<unique_ptr<ConstructorFunction>> constructors;


public:

    // hack because if this is cleaned up it crashes program, so just leak it.
    //   probably due to different copies of malloc/free being linked in but not sure how
    static inline vector<WrappedClass> * wrapped_classes_ptr = new vector<WrappedClass>();

    // reference so the "right" way of doing this is still in the code and if the problem is fixed
    //   just this one remains
    static inline vector<WrappedClass> & wrapped_classes = *wrapped_classes_ptr;

    CXXRecordDecl const * decl = nullptr;

    // if this wrapped class is a template instantiation, what was it patterned from -- else nullptr
    CXXRecordDecl const * instantiation_pattern = nullptr;

    /**
     * Builds data structures associated with the methods of this class
     */
    void parse_all_methods();


    // name of type that is guaranteed valid c++ (with appropriate included headers)
    string class_name;

    /// this is the possibly shortened javascript name of the type - not necessarily valid in generated c++
    string name_alias;

    set<string> include_files;

    // value here is the "compilation cost" of creating the class itself even if it's empty.
    // increase if too many empty classes end up in one file and make the compilation too big/take too long
    int declaration_count = 3;

    string my_header_filename = "";

    set<unique_ptr<MemberFunction>> const & get_member_functions();

    set<unique_ptr<StaticFunction>> const & get_static_functions();

    set<unique_ptr<DataMember>> & get_members();
    map<string, map<string, int>> const & get_enums();

    set<unique_ptr<ConstructorFunction>> const & get_constructors();

    set<string> used_member_names;
    set<string> used_static_names;
    vector<string> data_errors;
    set<WrappedClass *> derived_types;

    /// tracked base_types - if it's more than one, that's a data error because javascript only allows one
    set<WrappedClass *> base_types;

    set<string> wrapper_extension_methods;
    set<string> wrapper_custom_extensions;
    CompilerInstance & compiler_instance;

    string my_include; // the include for getting my type, including "" or <>
    bool done = false;
    bool valid = false; // guilty until proven innocent - don't delete !valid classes because they may be base classes for valid types
    Annotations annotations;
    bool dumped = false; // this class has been dumped to file
    set<WrappedClass *> used_classes; // classes this class uses in its wrapped functions/members/etc
    FOUND_METHOD found_method;
    bool force_no_constructors = false;

    // it's ok for types that won't be exposed to javascript to have wrapping errors associated with them
    void set_error(string const & error_message);

    bool bidirectional = false;
    CXXConstructorDecl const * bidirectional_constructor = nullptr;

    std::string get_short_name() const {
        if (decl == nullptr) {
            llvm::report_fatal_error(
                fmt::format("Tried to get_short_name on 'fake' WrappedClass {}", class_name).c_str());
        }
        return decl->getNameAsString();
    }

    bool has_static_method() { return !this->static_functions.empty(); }

    /**
     * @return whether this type is a specialization of a template
     */
    bool is_template_specialization();

    /**
     * @param callback called on each parameterized type for this template specialization
     */
    void foreach_inheritance_level(function<void(WrappedClass &)> callback);


    /**
     * Sets a member name as being in use and sets valid = false if it was already in use
     * For member functions and data members (not constructors or static methods)
     * @param name name to add
     */
    void add_member_name(string const & name);

    void add_static_name(string const & name);

    // all the correct annotations and name overrides may not be available when the WrappedObject is initially created
    void update_data() {
        cerr << "Updating wrapped class data for " << class_name << endl;
        string new_name = Annotations::names_for_record_decls[decl];
        if (!new_name.empty()) {
            cerr << "Got type alias: " << new_name << endl;
            name_alias = new_name;
        } else {
            cerr << "no type alias" << endl;
        }
        cerr << "Went from " << this->annotations.get().size() << " annotations to ";
        this->annotations = Annotations(this->decl);
        cerr << this->annotations.get().size() << endl;
    }

    std::string make_sfinae_to_match_wrapped_class() const {

        // if it was found by annotation, there's no way for V8ClassWrapper to know about it other than
        //   explicit sfinae for the specific class.  Inheritance can be handled via a single std::is_base_of
        if (found_method == FOUND_ANNOTATION) {
            return fmt::format("std::is_same<T, {}>::value", class_name);
        } else {
            return "";
        }
    }


    bool should_be_wrapped() const;

    bool ready_for_wrapping(set<WrappedClass *> dumped_classes) const;

    // return all the header files for all the types used by all the base types of the specified type
    std::set<string> get_base_type_includes();

    std::set<string> get_derived_type_includes();


    WrappedClass(const WrappedClass &) = delete;
    WrappedClass(WrappedClass &&) = default;

    WrappedClass & operator=(const WrappedClass &) = delete;

    // for newly created classes --- used for bidirectional classes that don't actually exist in the AST
    WrappedClass(const std::string class_name, CompilerInstance & compiler_instance);


    // for classes actually in the code being parsed
    WrappedClass(const CXXRecordDecl * decl, CompilerInstance & compiler_instance, FOUND_METHOD found_method);

    std::string generate_js_stub();

    std::string get_derived_classes_string(int level = 0, const std::string indent = "") {
        vector<string> results;
        //            printf("%s In (%d) %s looking at %d derived classes\n", indent.c_str(), level, class_name.c_str(), (int)derived_types.size());
        for (WrappedClass * derived_class : derived_types) {
            results.push_back(derived_class->class_name);
            // only use directly derived types now
            //results.push_back(derived_class->get_derived_classes_string(level + 1, indent + "  "));
        }
        //            printf("%s Returning %s\n", indent.c_str(), join(results).c_str());
        return join(results);
    }

    std::string get_base_class_string() {
        auto i = base_types.begin();
        while (i != base_types.end()) {
            if (std::find(base_types_to_ignore.begin(), base_types_to_ignore.end(), (*i)->class_name) !=
                base_types_to_ignore.end()) {
                base_types.erase(i++); // move iterator before erasing
            } else {
                i++;
            }
        };
        if (base_types.size() > 1) {
            data_error(fmt::format(
                "Type {} has more than one base class - this isn't supported because javascript doesn't support MI\n",
                class_name));

        }
        return base_types.size() ? (*base_types.begin())->class_name : "";
    }

    std::string get_bindings();

    /**
     * Returns the wrapped class corresponding to the decl if it exists
     * @param decl the decl to search for in existing wrapped classes
     * @return the existing wrapped class or nullptr if no match found
     */
    static WrappedClass * get_if_exists(const CXXRecordDecl * decl) {
        if (decl == nullptr) {
            return nullptr;
        }
        for (auto & wrapped_class : WrappedClass::wrapped_classes) {
            if (wrapped_class.decl == decl) {
                return &wrapped_class;
            }
        }
        return nullptr;
    }

    static WrappedClass & get_or_insert_wrapped_class(const CXXRecordDecl * decl,
                                                      CompilerInstance & compiler_instance,
                                                      FOUND_METHOD found_method) {

        if (decl->isDependentType()) {
            llvm::report_fatal_error("unpexpected dependent type");
        }

        auto class_name = get_canonical_name_for_decl(decl);

        // if this decl isn't a definition, get the actual definition
        if (!decl->isThisDeclarationADefinition()) {

            cerr << class_name << " is not a definition - getting definition..." << endl;
            if (!decl->hasDefinition()) {
                llvm::report_fatal_error(fmt::format("{} doesn't have a definition", class_name).c_str());
            }

            decl = decl->getDefinition();
        }


        for (auto & wrapped_class : wrapped_classes) {

            if (wrapped_class.class_name == class_name) {

                // promote found_method if FOUND_BASE_CLASS is specified - the type must ALWAYS be wrapped
                //   if it is the base of a wrapped type
                if (found_method == FOUND_BASE_CLASS) {

                    // if the class wouldn't otherwise be wrapped, need to make sure no constructors are created
                    if (!wrapped_class.should_be_wrapped()) {
                        wrapped_class.force_no_constructors = true;
                    }
                    wrapped_class.found_method = FOUND_BASE_CLASS;

                    // if a type was adjusted, make sure to adjust it's base types as well
                    for(auto & base : wrapped_class.base_types) {
                        get_or_insert_wrapped_class(base->decl, compiler_instance, FOUND_BASE_CLASS);
                    }
                }
                //fprintf(stderr, "returning existing object: %p\n", (void *)wrapped_class.get());
                return wrapped_class;
            }
        }
        return WrappedClass::wrapped_classes.emplace_back(decl, compiler_instance, found_method);

    }

    // returns true if the found_method on this class means the class will be wrapped
    bool found_method_means_wrapped();
};


}