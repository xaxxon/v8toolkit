
#pragma once

#include "parsed_method.h"
#include "annotations.h"

#include <sstream>

// should be named "ParsedClass" or something, since both classes that will and will not be wrapped
//   are put into this data structure
struct WrappedClass {
private:

    bool methods_parsed = false;
    set<unique_ptr<ParsedMethod>> methods;

    bool members_parsed = false;
    set<unique_ptr<DataMember>> members;

public:
    static vector<WrappedClass *> wrapped_classes;

    CXXRecordDecl const * decl = nullptr;

    // if this wrapped class is a template instantiation, what was it patterned from -- else nullptr
    CXXRecordDecl const * instantiation_pattern = nullptr;
    string class_name;
    string name_alias; // if no alias, is equal to class_name
    set<string> include_files;

     // value here is the "compilation cost" of creating the class itself even if it's empty.
    // increase if too many empty classes end up in one file and make the compilation too big/take too long
    int declaration_count = 3;

    string my_header_filename = "";

    set<unique_ptr<ParsedMethod>> & get_methods();
    set<unique_ptr<DataMember>> & get_members();

    set<string> constructors;
    set<string> used_names;
    vector<string> data_errors;
    set<WrappedClass *> derived_types;
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
    bool has_static_method = false;
    FOUND_METHOD found_method;
    bool force_no_constructors = false;

    // it's ok for types that won't be exposed to javascript to have wrapping errors associated with them
    void set_error(string const & error_message);

    bool bidirectional = false;
    CXXConstructorDecl const * bidirectional_constructor = nullptr;

    std::string get_short_name() const {
        if (decl == nullptr) {
            llvm::report_fatal_error(fmt::format("Tried to get_short_name on 'fake' WrappedClass {}", class_name).c_str());
        }
        return decl->getNameAsString();
    }

    bool is_template_specialization();

    /**
     * Adds the specified name and sets valid = false if it's alrady used
     * @param name name to add
     */
    void add_name(string const & name);

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
    WrappedClass & operator=(const WrappedClass &) = delete;

    // for newly created classes --- used for bidirectional classes that don't actually exist in the AST
    WrappedClass(const std::string class_name, CompilerInstance & compiler_instance);


    // for classes actually in the code being parsed
    WrappedClass(const CXXRecordDecl * decl, CompilerInstance & compiler_instance, FOUND_METHOD found_method);

    std::string generate_js_stub();

    std::string get_derived_classes_string(int level = 0, const std::string indent = ""){
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

    std::string get_base_class_string(){
        auto i = base_types.begin();
        while(i != base_types.end()) {
            if (std::find(base_types_to_ignore.begin(), base_types_to_ignore.end(), (*i)->class_name) !=
                base_types_to_ignore.end()) {
                base_types.erase(i++); // move iterator before erasing
            } else {
                i++;
            }
        };
        if (base_types.size() > 1) {
            data_error(fmt::format("Type {} has more than one base class - this isn't supported because javascript doesn't support MI\n", class_name));

        }
        return base_types.size() ? (*base_types.begin())->class_name : "";
    }

    std::string get_bindings();

    static void insert_wrapped_class(WrappedClass * wrapped_class) {
        WrappedClass::wrapped_classes.push_back(wrapped_class);
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


        //fprintf(stderr, "get or insert wrapped class %p\n", (void*)decl);
        //fprintf(stderr, " -- class name %s\n", class_name.c_str());
        for (auto & wrapped_class : wrapped_classes) {

            if (wrapped_class->class_name == class_name) {

                // promote found_method if FOUND_BASE_CLASS is specified - the type must ALWAYS be wrapped
                //   if it is the base of a wrapped type
                if (found_method == FOUND_BASE_CLASS) {

                    // if the class wouldn't otherwise be wrapped, need to make sure no constructors are created
                    if (!wrapped_class->should_be_wrapped()) {
                        wrapped_class->force_no_constructors = true;
                    }
                    wrapped_class->found_method = FOUND_BASE_CLASS;
                }
                //fprintf(stderr, "returning existing object: %p\n", (void *)wrapped_class.get());
                return *wrapped_class;
            }
        }
        auto new_wrapped_class = new WrappedClass(decl, compiler_instance, found_method);
        WrappedClass::wrapped_classes.push_back(new_wrapped_class);
        return *new_wrapped_class;

    }

    // returns true if the found_method on this class means the class will be wrapped
    bool found_method_means_wrapped();


};
