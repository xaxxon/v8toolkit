#ifndef V8TOOLKIT_WRAPPED_CLASS_BASE_H
#define V8TOOLKIT_WRAPPED_CLASS_BASE_H

namespace v8toolkit {
    /**
     * Inheriting from this tells v8toolkit the derived type is to be wrapped for use in JavaScript
     * Alternatives if altering inheritance isn't possible (such as a type from a third-party library):
     * (preferred): specialize v8toolkit::is_wrapped_type type trait for type to inherit from std::true_type
     * (last resort): #define V8TOOLKIT_V8CLASSWRAPPER_FULL_TEMPLATE_SFINAE_PREFIX with appropriate SFINAE
     */
    class WrappedClassBase {};

}

#endif