
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

#define V8TOOLKIT_NONE __attribute__((annotate(V8TOOLKIT_NONE_STRING)))
#define V8TOOLKIT_WRAPPED_CLASS __attribute__((annotate(V8TOOLKIT_ALL_STRING)))

/**
 * Use this to create a JavaScript constructor function with the specified name
 */
#define V8TOOLKIT_CONSTRUCTOR_PREFIX "v8toolkit_bidirectional_constructor_"
#define V8TOOLKIT_CONSTRUCTOR(name) \
    __attribute__((annotate(V8TOOLKIT_CONSTRUCTOR_PREFIX #name)))




/**
 * Use these to automatically generate bindings allowing javascript to subclass c++
 * types via the "bidirectional" process
 */
#define V8TOOLKIT_BIDIRECTIONAL_CLASS_STRING "v8toolkit_generate_bidirectional"
#define V8TOOLKIT_BIDIRECTIONAL_CLASS __attribute__((annotate(V8TOOLKIT_BIDIRECTIONAL_CLASS_STRING)))

