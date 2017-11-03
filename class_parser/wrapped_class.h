
#pragma once

#include <sstream>
#include <set>

#include <xl/library_extensions.h>

#include "parsed_method.h"
#include "annotations.h"

#include "log.h"


namespace v8toolkit::class_parser {


extern int MAX_DECLARATIONS_PER_FILE;

// should be named "ParsedClass" or something, since both classes that will and will not be wrapped
//   are put into this data structure
class MemberFunction;

class StaticFunction;



// how was a wrapped class determined to be a wrapped class?
enum FOUND_METHOD {
    FOUND_UNSPECIFIED = 0, // no information on why this class is being wrapped - may change later if more information found
    FOUND_ANNOTATION, // this class was annotated as being wrapped
    FOUND_INHERITANCE, // this class is a base of a function that is wrapped
    FOUND_GENERATED,
    FOUND_BASE_CLASS, // if the class is a base class of a wrapped type, the class must be wrapped
    FOUND_NEVER_WRAP
};


struct WrappedClass {
    friend class std::allocator<WrappedClass>;
private:
    bool methods_parsed = false;
    vector<unique_ptr<MemberFunction>> member_functions;
    vector<unique_ptr<StaticFunction>> static_functions;

    bool members_parsed = false;
    vector<unique_ptr<DataMember>> members;

    bool enums_parsed = false;
    map<string, map<string, int>> enums;

    vector<unique_ptr<ConstructorFunction>> constructors;


    // can't call this directly, must use factory
    WrappedClass(const CXXRecordDecl * decl, CompilerInstance & compiler_instance, FOUND_METHOD found_method);


    /// this is the possibly shortened javascript name of the type - not necessarily valid in generated c++
    string name_alias;
    bool name_alias_is_default = true;

public:
    LogWatcher<LogT> log_watcher;

    static WrappedClass & make_wrapped_class(const CXXRecordDecl * decl, CompilerInstance & compiler_instance, FOUND_METHOD found_method);

    ~WrappedClass();

    static inline vector<unique_ptr<WrappedClass>> wrapped_classes;
    static inline std::vector<std::string> used_constructor_names;


    CXXRecordDecl const * decl = nullptr;

    // if this wrapped class is a template instantiation, what was it patterned from -- else nullptr
    CXXRecordDecl const * instantiation_pattern = nullptr;

    /**
     * Builds data structures associated with the methods of this class
     */
    void parse_all_methods();


    // name of type that is guaranteed valid c++ (with appropriate included headers)
    string class_name;

    std::string const & get_name_alias() const {
        return this->name_alias;
    };
    void set_name_alias(std::string const & new_name_alias) {
        this->name_alias = new_name_alias;
        this->name_alias_is_default = false;
    }
    bool is_name_alias_default() const {
        return this->name_alias_is_default;
    }

    string get_jsdoc_name() const;

    set<string> include_files;

    // value here is the "compilation cost" of creating the class itself even if it's empty.
    // increase if too many empty classes end up in one file and make the compilation too big/take too long
    int declaration_count = 3;

    string my_header_filename = "";

    vector<unique_ptr<MemberFunction>> const & get_member_functions() const;

    vector<unique_ptr<StaticFunction>> const & get_static_functions() const;

    vector<unique_ptr<DataMember>> const & get_members() const;
    void parse_members();

    void parse_enums();
    map<string, map<string, int>> const & get_enums() const;

    vector<unique_ptr<ConstructorFunction>> const & get_constructors() const;

    set<string> used_member_names;
    set<string> used_static_names;
    vector<string> data_errors;
    set<WrappedClass *> derived_types;

    /// tracked base_types - if it's more than one, that's a data error because javascript only allows one
    set<WrappedClass *> base_types;

    set<string> wrapper_extension_methods;
    set<string> wrapper_custom_extensions;
    CompilerInstance & compiler_instance;

    /// doxygen-style comment associated with the class
    std::string comment;

    string my_include; // the include for getting my type, including "" or <>
    bool done = false;
    bool valid = false; // guilty until proven innocent - don't delete !valid classes because they may be base classes for valid types
    Annotations annotations;
    set<WrappedClass *> used_classes; // classes this class uses in its wrapped functions/members/etc
    FOUND_METHOD found_method;
    bool force_no_constructors = false;

    // it's ok for types that won't be exposed to javascript to have wrapping errors associated with them
    void set_error(string const & error_message);

    bool bidirectional = false;
    CXXConstructorDecl const * bidirectional_constructor = nullptr;

    std::string get_short_name() const;

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
    void update_data();

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

    bool ready_for_wrapping(set<WrappedClass const *> dumped_classes) const;

    // return all the header files for all the types used by all the base types of the specified type
    std::set<string> get_base_type_includes() const;

    std::set<string> get_derived_type_includes() const;


    WrappedClass(const WrappedClass &) = delete;
    WrappedClass(WrappedClass &&) = default;

    WrappedClass & operator=(const WrappedClass &) = delete;

    // for newly created classes --- used for bidirectional classes that don't actually exist in the AST
    WrappedClass(const std::string class_name, CompilerInstance & compiler_instance);

    std::string get_derived_classes_string(int level = 0, const std::string indent = "") const;

    void add_base_type(WrappedClass & base_type);

    std::string get_base_class_string() const;

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
            if (wrapped_class->decl == decl) {
                return wrapped_class.get();
            }
        }
        return nullptr;
    }

    void make_bidirectional_wrapped_class_if_needed();

    static WrappedClass & get_or_insert_wrapped_class(const CXXRecordDecl * decl,
                                                      CompilerInstance & compiler_instance,
                                                      FOUND_METHOD found_method);

    // returns true if the found_method on this class means the class will be wrapped
    bool found_method_means_wrapped();

}; // end class WrappedClass


}