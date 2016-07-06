
#pragma once


// This may be the wrong check - may need to do something like #ifndef __GNUC__
#ifndef __GNUC__
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
 * Use this to create a JavaScript constructor function with the specified name
 */
#define V8TOOLKIT_CONSTRUCTOR_PREFIX "v8toolkit_bidirectional_constructor_"
#define V8TOOLKIT_CONSTRUCTOR(name) \
    __attribute__((annotate(V8TOOLKIT_CONSTRUCTOR_PREFIX #name)))




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
 * Marks a parameter as one that is always the same, not something that will change per instance
 */
#define V8TOOLKIT_BIDIRECTIONAL_INTERNAL_PARAMETER __attribute__((annotate(V8TOOLKIT_BIDIRECTIONAL_INTERNAL_PARAMETER_STRING)))

