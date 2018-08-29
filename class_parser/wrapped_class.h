
#pragma once

#include <sstream>
#include <set>

#include <xl/library_extensions.h>

#include "parsed_method.h"
#include "annotations.h"

#include "log.h"


namespace v8toolkit::class_parser {


// should be named "ParsedClass" or something, since both classes that will and will not be wrapped
//   are put into this data structure
class MemberFunction;

class StaticFunction;



// how was a wrapped class determined to be a wrapped class?
enum FOUND_METHOD {
    FOUND_UNSPECIFIED = 0, // no information on why this class is being wrapped - may change later if more information found
    FOUND_PIMPL, // a pimple member of a class - "partial wrapped-ness"
    FOUND_ANNOTATION, // this class was annotated as being wrapped
    FOUND_INHERITANCE, // this class is a base of a function that is wrapped
    FOUND_GENERATED,
    FOUND_BASE_CLASS, // if the class is a base class of a wrapped type, the class must be wrapped
    FOUND_NEVER_WRAP,
};


struct WrappedClass {
    friend class std::allocator<WrappedClass>;
private:
    bool methods_parsed = false;
    vector<unique_ptr<MemberFunction>> member_functions;
    vector<unique_ptr<StaticFunction>> static_functions;

    bool members_parsed = false;
    std::vector<std::unique_ptr<DataMember>> members;

    std::vector<Enum> enums;
    bool enums_parsed = false;

    std::vector<std::unique_ptr<ConstructorFunction>> constructors;


    // can't call this directly, must use factory
    WrappedClass(const CXXRecordDecl * decl, FOUND_METHOD found_method);

    // data members marked as being pimpl to be exposed as if part of the main class
    std::vector<std::string> pimpl_data_member_names; // names stored from class attribute
    std::vector<std::unique_ptr<DataMember>> pimpl_data_members; // actual data member objects in class


public:
    // name of type that is guaranteed valid c++ (with appropriate included headers)
    std::string class_name;

    // without class/struct or namespace
    std::string short_name = class_name;

private:

    /// this is the possibly shortened javascript name of the type - not necessarily valid in generated c++
    mutable std::string js_name;

public:
    LogWatcher<LogT> log_watcher;

    static WrappedClass & make_wrapped_class(const CXXRecordDecl * decl, FOUND_METHOD found_method);

    ~WrappedClass();

    static inline vector<unique_ptr<WrappedClass>> wrapped_classes;
    static inline std::vector<std::string> used_constructor_names;

    static WrappedClass * get_wrapped_class(CXXRecordDecl const * decl);
    static WrappedClass * get_wrapped_class(TypeInfo const & type_info);


    CXXRecordDecl const * decl = nullptr;

    // if this wrapped class is a template instantiation, what was it patterned from -- else nullptr
    CXXRecordDecl const * instantiation_pattern = nullptr;

    /**
     * Builds data structures associated with the methods of this class
     */
    void parse_all_methods();

    bool has_errors() const;
    decltype(log_watcher.errors) const & get_errors() const;

    std::string const & get_js_name() const;


    void force_recache_js_name() {
        this->js_name = "";
    }

    string my_include; // the include for getting my type, including "" or <>
    set<string> include_files; // everything this types needs to be wrapped

    // value here is the "compilation cost" of creating the class itself even if it's empty.
    // increase if too many empty classes end up in one file and make the compilation too big/take too long
    int declaration_count = 3;

    string my_header_filename = "";

    vector<unique_ptr<MemberFunction>> const & get_member_functions() const;

    // stores an operator() member function - more than one not skipped in a class is an error
    unique_ptr<MemberFunction> call_operator_member_function;

    vector<unique_ptr<StaticFunction>> const & get_static_functions() const;

    std::vector<DataMember *> get_members() const;
    void parse_members();

    void parse_enums();
    std::vector<Enum> const & get_enums() const;

    vector<unique_ptr<ConstructorFunction>> const & get_constructors() const;
    set<WrappedClass *> derived_types;

    /// tracked base_types - if it's more than one, that's a data error because javascript only allows one
    set<WrappedClass *> base_types;

    set<string> wrapper_extension_methods;
    set<string> wrapper_custom_extensions;

    /// doxygen-style comment associated with the class
    std::string comment;

    bool done = false;

    Annotations annotations;
    set<WrappedClass *> used_classes; // classes this class uses in its wrapped functions/members/etc
    FOUND_METHOD found_method;
    bool force_no_constructors = false;

    bool bidirectional = false;
    CXXConstructorDecl const * bidirectional_constructor = nullptr;

    std::string get_short_name() const;

    bool has_static_method() { return !this->static_functions.empty(); }

    /**
     * Whether the type has annotations specifying any PIMPL members to be exposed
     * @return true if it has 1 or more, false if it has 0
     */
    bool has_pimpl_members() const;
    
    std::vector<DataMember *> get_pimpl_data_members(bool with_inherited_members = true) const;

    /**
     * @return whether this type is a specialization of a template
     */
    bool is_template_specialization();

    /**
     * @param callback called on each parameterized type for this template specialization
     */
    template <typename T = void>
    T foreach_inheritance_level(function<T(WrappedClass &, T)> callback) {
        assert(this->base_types.size() <= 1);

        if (!this->base_types.empty()) {
            return callback(*this, (*base_types.begin())->foreach_inheritance_level(callback));
        } else {
            return callback(*this, T());
        }
    }

    void foreach_inheritance_level(function<void(WrappedClass &)> callback, bool base_first = true) {
        assert(this->base_types.size() <= 1);

        if (!base_first) {
            callback(*this);
        }

        if (!this->base_types.empty()) {
            (*base_types.begin())->foreach_inheritance_level(callback, base_first);
        }

        if (base_first) {
            callback(*this);
        }
    }

    template <typename T = void>
    T foreach_inheritance_level(function<T(WrappedClass const &, T)> callback) const {
        assert(this->base_types.size() <= 1);
        if (!this->base_types.empty()) {
            return callback(*this, (*base_types.begin())->foreach_inheritance_level(callback));
        } else {
            return callback(*this, T());
        }
    }

    void foreach_inheritance_level(function<void(WrappedClass const &)> callback, bool base_first = true) const {
        assert(this->base_types.size() <= 1);

        if (!base_first) {
            callback(*this);
        }

        if (!this->base_types.empty()) {
            ((WrappedClass const *)(*base_types.begin()))->foreach_inheritance_level(callback, base_first);
        }
        if (base_first) {
            callback(*this);
        }
    }

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
    bool should_be_parsed() const;

    bool ready_for_wrapping(set<WrappedClass const *> dumped_classes) const;

    WrappedClass(const WrappedClass &) = delete;
    WrappedClass(WrappedClass &&) = default;

    WrappedClass & operator=(const WrappedClass &) = delete;

    // for newly created classes --- used for bidirectional classes that don't actually exist in the AST
    explicit WrappedClass(const std::string class_name);

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
                                                      FOUND_METHOD found_method);

    // returns true if the found_method on this class means the class will be wrapped
    bool found_method_means_wrapped();

    set<ClassFunction const *> get_all_functions_from_class_hierarchy() const;

    std::string class_or_struct = "";
    std::string namespace_name = "";

    // validates class data in preparation for wrapping it for JavaScript
    //   Requirements here aren't important for classes not being wrapped
    void validate_data();


    }; // end class WrappedClass


}