
#pragma once

#include "parsed_method.h"

struct WrappedClass {
    CXXRecordDecl const * decl = nullptr;

    // if this wrapped class is a template instantiation, what was it patterned from -- else nullptr
    CXXRecordDecl const * instantiation_pattern = nullptr;
    string class_name;
    string name_alias; // if no alias, is equal to class_name
    set<string> include_files;
    int declaration_count = 3;

    set<ParsedMethod *> methods;
    set<string> members;
    set<string> constructors;
    set<string> names;
    set<WrappedClass *> derived_types;
    set<WrappedClass *> base_types;
    set<FieldDecl *> fields;
    set<string> wrapper_extension_methods;
    set<string> wrapper_custom_extensions;
    CompilerInstance & compiler_instance;
    string my_include; // the include for getting my type
    bool done = false;
    bool valid = false; // guilty until proven innocent - don't delete !valid classes because they may be base classes for valid types
    Annotations annotations;
    bool dumped = false; // this class has been dumped to file
    set<WrappedClass *> used_classes; // classes this class uses in its wrapped functions/members/etc
    bool has_static_method = false;
    FOUND_METHOD found_method;
    bool force_no_constructors = false;

    std::string get_short_name() const {
        if (decl == nullptr) {
            llvm::report_fatal_error(fmt::format("Tried to get_short_name on 'fake' WrappedClass {}", class_name).c_str());
        }
        return decl->getNameAsString();
    }

    bool is_template_specialization() {
        return dyn_cast<ClassTemplateSpecializationDecl>(decl);
    }


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


    bool should_be_wrapped() const {
        auto a = class_name;
        auto b = found_method;
        auto c = join(annotations.get());
        cerr << fmt::format("In should be wrapped with class {}, found_method: {}, annotations: {}", a, b, c) << endl;

        if (annotations.has(V8TOOLKIT_NONE_STRING) &&
            annotations.has(V8TOOLKIT_ALL_STRING)) {
            data_error(fmt::format("type has both NONE_STRING and ALL_STRING - this makes no sense", class_name));
        }

        if (found_method == FOUND_BASE_CLASS) {
            return true;
        }
        if (found_method == FOUND_GENERATED) {
            return true;
        }

        if (found_method == FOUND_INHERITANCE) {
            if (annotations.has(V8TOOLKIT_NONE_STRING)) {
                cerr << "Found NONE_STRING" << endl;
                return false;
            }
        } else if (found_method == FOUND_ANNOTATION) {
            if (annotations.has(V8TOOLKIT_NONE_STRING)) {
                cerr << "Found NONE_STRING" << endl;
                return false;
            }
            if (!annotations.has(V8TOOLKIT_ALL_STRING)) {
                llvm::report_fatal_error(fmt::format("Type was supposedly found by annotation, but annotation not found: {}", class_name));
            }
        } else if (found_method == FOUND_UNSPECIFIED) {
            if (annotations.has(V8TOOLKIT_NONE_STRING)) {
                cerr << "Found NONE_STRING on UNSPECIFIED" << endl;
                return false;
            }
            if (!annotations.has(V8TOOLKIT_ALL_STRING)) {
                cerr << "didn't find all string on UNSPECIFIED" << endl;
                return false;
            }
        }

        if (base_types.size() > 1) {
            data_error(fmt::format("trying to see if type should be wrapped but it has more than one base type -- unsupported", class_name));
        }

        /*
          // *** IF A TYPE SHOULD BE WRAPPED THAT FORCES ITS PARENT TYPE TO BE WRAPPED ***


        if (base_types.empty()) {
        cerr << "no base typ so SHOULD BE WRAPPED" << endl;
        return true;
        } else {
        cerr << "Checking should_be_wrapped for base type" << endl;
        auto & base_type_wrapped_class = **base_types.begin();
        cerr << "base type is '" << base_type_wrapped_class.class_name << "'" << endl;
        return base_type_wrapped_class.should_be_wrapped();
        }
        */

        return true;
    }

    bool ready_for_wrapping(set<WrappedClass *> dumped_classes) const {


        if (!valid) {
            cerr << "'invalid' class" << endl;
            return false;
        }


        // don't double wrap yourself
        if (find(dumped_classes.begin(), dumped_classes.end(), this) != dumped_classes.end()) {
            printf("Already wrapped %s\n", class_name.c_str());
            return false;
        }

        if (!should_be_wrapped()) {
            cerr << "should be wrapped returned false" << endl;
            return false;
        }

        /*
            // if all this class's directly derived types have been wrapped, then we're good since their
            //   dependencies would have to be met for them to be wrapped
            for (auto derived_type : derived_types) {
                if (find(dumped_classes.begin(), dumped_classes.end(), derived_type) == dumped_classes.end()) {
                    printf("Couldn't find %s\n", derived_type->class_name.c_str());
                    return false;
                }
            }
        */
        for (auto base_type : base_types) {
            if (find(dumped_classes.begin(), dumped_classes.end(), base_type) == dumped_classes.end()) {
                printf("base type %s not already wrapped - return false\n", base_type->class_name.c_str());
                return false;
            }
        }

        printf("Ready to wrap %s\n", class_name.c_str());

        return true;
    }

    // return all the header files for all the types used by all the base types of the specified type
    std::set<string> get_base_type_includes() {
        set<string> results{this->my_include};
        results.insert(this->include_files.begin(), this->include_files.end());
        std::cerr << fmt::format("adding base type include for {}", this->class_name) << std::endl;

        for (WrappedClass * base_class : this->base_types) {
            auto base_results = base_class->get_base_type_includes();
            results.insert(base_results.begin(), base_results.end());
        }

        return results;
    }

    std::set<string> get_derived_type_includes() {
        cerr << fmt::format("Getting derived type includes for {}", name_alias) << endl;
        set<string> results;
        results.insert(my_include);
        for (auto derived_type : derived_types) {

            results.insert(derived_type->include_files.begin(), derived_type->include_files.end());

            auto derived_includes = derived_type->get_derived_type_includes();
            results.insert(derived_includes.begin(), derived_includes.end());

            cerr << fmt::format("{}: Derived type includes for subclass {} and all its derived classes: {}", name_alias, derived_type->class_name, join(derived_includes)) << endl;

        }
        return results;
    }


    WrappedClass(const WrappedClass &) = delete;
    WrappedClass & operator=(const WrappedClass &) = delete;

    // for newly created classes --- used for bidirectional classes that don't actually exist in the AST
    WrappedClass(const std::string class_name, CompilerInstance & compiler_instance) :
        decl(nullptr),
        class_name(class_name),
        name_alias(class_name),
        compiler_instance(compiler_instance),
        valid(true), // explicitly generated, so must be valid
        found_method(FOUND_GENERATED)
    {}


    std::string generate_js_stub() {
        struct MethodParam {
            string type = "";
            string name = "";
            string description = "no description available";

            void convert_type(std::string const & indentation = "") {
                std::smatch matches;
                std::cerr << fmt::format("{} converting {}...", indentation, this->type) << std::endl;
                this->type = regex_replace(type, std::regex("^(struct|class) "), "");
                for (auto &pair : cpp_to_js_type_conversions) {
                    if (regex_match(this->type, matches, std::regex(pair.first))) {
                        std::cerr << fmt::format("{} matched {}, converting to {}", indentation, pair.first, pair.second) << std::endl;
                        auto new_type = pair.second; // need a temp because the regex matches point into the current this->type

                        // look for $1, $2, etc in resplacement and substitute in the matching position
                        for (size_t i = 1; i < matches.size(); i++) {
                            // recursively convert the matched type
                            MethodParam mp;
                            mp.type = matches[i].str();
                            mp.convert_type(indentation + "   ");
                            new_type = std::regex_replace(new_type, std::regex(fmt::format("[$]{}", i)),
                                                          mp.type);
                        }
                        this->type = new_type;
                        std::cerr << fmt::format("{}... final conversion to: {}", indentation, this->type) << std::endl;
                    }
                }
            }
        }; //  MethodParam

        stringstream result;
        string indentation = "    ";

        result << "/**\n";
        result << fmt::format(" * @class {}\n", this->name_alias);

        for (auto field : this->fields) {
            MethodParam field_type;
            field_type.name = field->getNameAsString();
            field_type.type = field->getType().getAsString();
            field_type.convert_type();
            result << fmt::format(" * @property {{{}}} {} \n", field_type.type, field_type.name);
        }
        result << fmt::format(" **/\n", indentation);


        result << fmt::format("class {}", this->name_alias);

        if (this->base_types.size() == 1) {
            result << fmt::format(" extends {}", (*this->base_types.begin())->name_alias);
        }
        result << "{\n";

        for (auto method : this->methods) {

            auto  method_decl = method->method_decl;
            vector<MethodParam> parameters;
            MethodParam return_value_info;
            string method_description;

            auto parameter_count = method_decl->getNumParams();
            for (unsigned int i = 0; i < parameter_count; i++) {
                MethodParam this_param;
                auto param_decl = method_decl->getParamDecl(i);
                auto parameter_name = param_decl->getNameAsString();
                if (parameter_name == "") {
                    data_warning(fmt::format("class {} method {} parameter index {} has no variable name",
                                             this->name_alias, method_decl->getNameAsString(), i));
                    parameter_name = fmt::format("unspecified_position_{}", parameters.size());
                }
                this_param.name = parameter_name;
                auto type = get_plain_type(param_decl->getType());
                this_param.type = type.getAsString();
                parameters.push_back(this_param);
            }

            return_value_info.type = get_plain_type(method_decl->getReturnType()).getAsString();

            FullComment *comment = this->compiler_instance.getASTContext().getCommentForDecl(method_decl, nullptr);
            if (comment != nullptr) {
                cerr << "**1" << endl;
                auto comment_text = get_source_for_source_range(
                    this->compiler_instance.getPreprocessor().getSourceManager(), comment->getSourceRange());

                cerr << "FullComment: " << comment_text << endl;
                for (auto i = comment->child_begin(); i != comment->child_end(); i++) {
                    cerr << "**2.1" << endl;

                    auto child_comment_source_range = (*i)->getSourceRange();
                    if (child_comment_source_range.isValid()) {
                        cerr << "**2.2" << endl;

                        auto child_comment_text = get_source_for_source_range(
                            this->compiler_instance.getPreprocessor().getSourceManager(),
                            child_comment_source_range);

                        if (auto param_command = dyn_cast<ParamCommandComment>(*i)) {
                            cerr << "Is ParamCommandComment" << endl;
                            auto command_param_name = param_command->getParamName(comment).str();

                            auto matching_param_iterator =
                                std::find_if(parameters.begin(), parameters.end(),
                                             [&command_param_name](auto &param) {
                                                 return command_param_name == param.name;
                                             });

                            if (param_command->hasParamName() && matching_param_iterator != parameters.end()) {

                                auto &param_info = *matching_param_iterator;
                                if (param_command->getParagraph() != nullptr) {
                                    cerr << "**3" << endl;
                                    param_info.description = get_source_for_source_range(
                                        this->compiler_instance.getPreprocessor().getSourceManager(),
                                        param_command->getParagraph()->getSourceRange());
                                }
                            } else {
                                data_warning(
                                    fmt::format("method parameter comment name doesn't match any parameter {}",
                                                command_param_name));
                            }
                        } else {
                            cerr << "is not param command comment" << endl;
                        }
                        cerr << "Child comment " << (*i)->getCommentKind() << ": " << child_comment_text << endl;
                    }
                }
            } else {
                cerr << "No comment on " << method_decl->getNameAsString() << endl;
            }


            result << fmt::format("{}/**\n", indentation);
            for (auto &param : parameters) {
                param.convert_type(); // change to JS types
                result << fmt::format("{} * @param {} {{{}}} {}\n", indentation, param.name, param.type,
                                      param.description);
            }
            return_value_info.convert_type();
            result << fmt::format("{} * @return {{{}}} {}\n", indentation, return_value_info.type,
                                  return_value_info.description);
            result << fmt::format("{} */\n", indentation);
            if (method_decl->isStatic()) {
                result << fmt::format("{}static ", indentation);
            }

            result << fmt::format("{}{}(", indentation, method_decl->getNameAsString());
            bool first_parameter = true;
            for (auto &param : parameters) {
                if (!first_parameter) {
                    result << ", ";
                }
                first_parameter = false;
                result << fmt::format("{}", param.name);
            }
            result << fmt::format("){{}}\n\n");

        }


        result << fmt::format("}}\n\n\n");
        fprintf(stderr, "%s", result.str().c_str());
        return result.str();
    }


    // for classes actually in the code being parsed
    WrappedClass(const CXXRecordDecl * decl, CompilerInstance & compiler_instance, FOUND_METHOD found_method) :
        decl(decl),
        class_name(get_canonical_name_for_decl(decl)),
        name_alias(decl->getNameAsString()),
        compiler_instance(compiler_instance),

        my_include(get_include_for_type_decl(compiler_instance, decl)),
        annotations(decl),
        found_method(found_method)
    {
        fprintf(stderr, "Creating WrappedClass for record decl ptr: %p\n", (void *)decl);
        string using_name = Annotations::names_for_record_decls[decl];
        if (!using_name.empty()) {
            cerr << fmt::format("Setting name alias for {} to {} because of a 'using' statement", class_name, using_name) << endl;
            name_alias = using_name;
        }


        cerr << "Top of WrappedClass constructor body" << endl;
        if (class_name == "") {
            fprintf(stderr, "%p\n", (void *)decl);
            llvm::report_fatal_error("Empty name string for decl");
        }

        auto pattern = this->decl->getTemplateInstantiationPattern();
        if (pattern && pattern != this->decl) {
            if (!pattern->isDependentType()) {
                llvm::report_fatal_error(fmt::format("template instantiation class's pattern isn't dependent - this is not expected from my understanding: {}", class_name));
            }
        }

        //	    instantiation_pattern = pattern;
        // annotations.merge(Annotations(instantiation_pattern));



        if (auto specialization = dyn_cast<ClassTemplateSpecializationDecl>(this->decl)) {
            annotations.merge(Annotations(specialization->getSpecializedTemplate()));
        }

        cerr << "Final wrapped class annotations: " << endl;
        print_vector(annotations.get());
    }

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

    std::string get_wrapper_string(){
        stringstream result;
        string indentation = "  ";

        result << indentation << "{\n";
        result << fmt::format("{}  // {}", indentation, class_name) << "\n";
        result << fmt::format("{}  v8toolkit::V8ClassWrapper<{}> & class_wrapper = isolate.wrap_class<{}>();\n",
                              indentation, class_name, class_name);
        result << fmt::format("{}  class_wrapper.set_class_name(\"{}\");\n", indentation, name_alias);

        for(auto & method : methods) {
            result << method;
        }
        for(auto & member : members) {
            result << member;
        }
        for(auto & wrapper_extension_method : wrapper_extension_methods) {
            result << fmt::format("{}  {}\n", indentation, wrapper_extension_method);
        }
        for(auto & wrapper_custom_extension : wrapper_custom_extensions) {
            result << fmt::format("{}  {}\n", indentation, wrapper_custom_extension);
        }

        if (!derived_types.empty()) {
            result << fmt::format("{}  class_wrapper.set_compatible_types<{}>();\n", indentation,
                                  get_derived_classes_string());
        }
        if (get_base_class_string() != "") {
            result << fmt::format("{}  class_wrapper.set_parent_type<{}>();\n", indentation,
                                  get_base_class_string());
        }
        result << fmt::format("{}  class_wrapper.finalize(true);\n", indentation);

        for(auto & constructor : constructors) {
            result << constructor;
        }

        result << indentation << "}\n\n";
        return result.str();
    }

};
