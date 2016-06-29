
// this is supposed to be a copy of NONE
#define SKIP __attribute__((annotate("v8toolkit_generate_bindings_none")))

// this is supposed to be a copy of ALL
#define EXPORT __attribute__((annotate("v8toolkit_generate_bindings_all")))

#define NONE __attribute__((annotate("v8toolkit_generate_bindings_none")))
#define SOME __attribute__((annotate("v8toolkit_generate_bindings_some")))
#define EXCEPT __attribute__((annotate("v8toolkit_generate_bindings_except")))
#define ALL __attribute__((annotate("v8toolkit_generate_bindings_all")))
#define NOT_SPECIAL __attribute__((annotate("else")))



#define V8TOOLKIT_BIDIRECTIONAL __attribute__((annotate("v8toolkit_generate_bidirectional")))
