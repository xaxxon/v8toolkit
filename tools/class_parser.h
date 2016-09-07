
#pragma once


// this only works on clang
#ifndef __clang__
#ifndef __attribute__
#define __attribute__(x) // only used by libclang plugin, so it only needs to exist under clang
#endif
#endif



/**
 * Use these to expose classes/class functions/class data members via javascript
 */
#define V8TOOLKIT_NONE_STRING "v8toolkit_generate_bindings_none"
#define V8TOOLKIT_ALL_STRING "v8toolkit_generate_bindings_all"
#define V8TOOLKIT_READONLY_STRING "v8toolkit_generate_bindings_readonly"
#define V8TOOLKIT_EXTEND_WRAPPER_STRING "v8toolkit_extend_wrapper"

/**
 * Generate V8ClassWrapper code for the annotated class
 * ex: class V8TOOLKIT_WRAPPED_CLASS MyClassName {...};
 */
#define V8TOOLKIT_WRAPPED_CLASS __attribute__((annotate(V8TOOLKIT_ALL_STRING)))

/** Skip an entry in a class being wrapped and/or bidirectional
 * ex: struct V8TOOLKIT_WRAPPED_CLASS MyClassName {
 *         V8TOOLKIT_SKIP void do_not_make_binding_for_me();
 *     };
 */
#define V8TOOLKIT_SKIP __attribute__((annotate(V8TOOLKIT_NONE_STRING)))


/**
 * This member cannot be assigned to.
 * However, it is not "const", as its contents can be changed.
 */
#define V8TOOLKIT_READONLY __attribute__((annotate(V8TOOLKIT_READONLY_STRING)))

/**
 * This function should be called while creating the mapping for this class before it is
 * finalize()'d
 */
#define V8TOOLKIT_EXTEND_WRAPPER __attribute__((annotate(V8TOOLKIT_EXTEND_WRAPPER_STRING)))


/**
 * For setting a name alias to be used for javascript to refer to the type as
 * -- basically sets a different constructor name when you don't have control
 *    over the class definition
 * Usage: using MyTypeInt V8TOOLKIT_NAME_ALIAS = MyType<int>;
 *        using MyTypeChar V8TOOLKIT_NAME_ALIAS = MyType<char>;
 * Otherwise both of those would get the same constructor name (MyType) and code generation would fail
 */
#define V8TOOLKIT_NAME_ALIAS_STRING "v8toolkit_name_alias"
#define V8TOOLKIT_NAME_ALIAS __attribute__((annotate(V8TOOLKIT_NAME_ALIAS_STRING)))


/**
 * Use this to create a JavaScript constructor function with the specified name
 */
#define V8TOOLKIT_CONSTRUCTOR_PREFIX "v8toolkit_bidirectional_constructor_"
#define V8TOOLKIT_CONSTRUCTOR(name) \
    __attribute__((annotate(V8TOOLKIT_CONSTRUCTOR_PREFIX #name)))


/**
 * Use this to create a JavaScript constructor function with the specified name
 */
#define V8TOOLKIT_EXPOSE_STATIC_METHODS_AS_PREFIX "v8toolkit_expose_static_methods_as_"
#define V8TOOLKIT_EXPOSE_STATIC_METHODS_AS(name) \
    __attribute__((annotate(V8TOOLKIT_EXPOSE_STATIC_METHODS_AS_PREFIX #name)))


/**
 * For classes with multiple inheritance, allows you to specify type(s) not to use.
 * Templates should be specified with only the base template name, not with template parameters
 *   e.g. MyTemplatedType not MyTemplatedType<int, char*> - does not support
 *        MI to select one where type inherits from two different versions of same template
 */
#define V8TOOLKIT_IGNORE_BASE_TYPE_PREFIX "v8toolkit_ignore_base_type_"
#define V8TOOLKIT_IGNORE_BASE_TYPE(name) \
    __attribute__((annotate(V8TOOLKIT_IGNORE_BASE_TYPE_PREFIX #name)))

/**
 * For classes with multiple inheritance, allows you to specify which one to use
 * Templates should be specified with only the base template name, not with template parameters
 *   e.g. MyTemplatedType not MyTemplatedType<int, char*> - does not support
 *        MI to select one where type inherits from two different specializations of same template
 */
#define V8TOOLKIT_USE_BASE_TYPE_PREFIX "v8toolkit_use_base_type_"
#define V8TOOLKIT_USE_BASE_TYPE(name) \
    __attribute__((annotate(V8TOOLKIT_USE_BASE_TYPE_PREFIX #name)))


/**
 * This can be specified in a forward declaration of a type to eliminate all constructors from being wrapped
 */
#define V8TOOLKIT_DO_NOT_WRAP_CONSTRUCTORS_STRING "v8toolkit_do_not_wrap_constructors"
#define V8TOOLKIT_DO_NOT_WRAP_CONSTRUCTORS __attribute__((annotate(V8TOOLKIT_DO_NOT_WRAP_CONSTRUCTORS_STRING)))


#define V8TOOLKIT_BIDIRECTIONAL_CLASS_STRING "v8toolkit_generate_bidirectional"
#define V8TOOLKIT_BIDIRECTIONAL_CONSTRUCTOR_STRING "v8toolkit_generate_bidirectional_constructor"
#define V8TOOLKIT_BIDIRECTIONAL_INTERNAL_PARAMETER_STRING "V8toolkit_generate_bidirectional_internal_parameter"
/**
 * Generate JSWrapper class for the annotated class
 * ex: class V8TOOLKIT_BIDIRECTIONAL_CLASS MyClassName {...};
 */
#define V8TOOLKIT_BIDIRECTIONAL_CLASS __attribute__((annotate(V8TOOLKIT_BIDIRECTIONAL_CLASS_STRING)))

/**
 * Annotate the constructor bidirectional should use with this
 */
#define V8TOOLKIT_BIDIRECTIONAL_CONSTRUCTOR __attribute__((annotate(V8TOOLKIT_BIDIRECTIONAL_CONSTRUCTOR_STRING)))

/**
 * Unused, but may come back.
 * Marks a parameter as one that is always the same, not something that will change per instance
 */
#define V8TOOLKIT_BIDIRECTIONAL_INTERNAL_PARAMETER __attribute__((annotate(V8TOOLKIT_BIDIRECTIONAL_INTERNAL_PARAMETER_STRING)))

