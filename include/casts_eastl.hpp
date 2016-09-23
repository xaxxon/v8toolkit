#pragma once

// Tell EASTL not to redefine initializer_list
#define EA_HAVE_CPP11_INITIALIZER_LIST 1

#include <EASTL/vector_map.h>
#include <EASTL/vector_multimap.h>
#include <EASTL/utility.h>
#include <EASTL/string.h>
#include <EASTL/fixed_string.h>

namespace v8toolkit {



template<class FirstT, class SecondT>
struct v8toolkit::CastToNative<eastl::pair<FirstT, SecondT>>{
    eastl::pair<FirstT, SecondT> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        pair_type_helper<eastl::pair, FirstT, SecondT>(isolate, value);
    }
};


template<class Key, class Value, class... Args>
struct CastToNative<eastl::vector_map<Key, Value, Args...>> {
    eastl::vector_map<Key, Value, Args...> operator()(v8::Isolate *isolate, v8::Local <v8::Value> value) const {
        return map_type_helper<eastl::vector_map, Key, Value, Args...>(isolate, value);
    }
};

template<class Key, class Value, class... Args>
struct CastToNative<eastl::vector_multimap<Key, Value, Args...>> {
    eastl::vector_multimap<Key, Value, Args...> operator()(v8::Isolate *isolate, v8::Local <v8::Value> value) const {
        return multimap_type_helper<eastl::vector_multimap, Key, Value, Args...>(isolate, value);
    }
};

CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(eastl::string)
    return eastl::string(*v8::String::Utf8Value(value));
}

CAST_TO_JS(eastl::string){return v8::String::NewFromUtf8(isolate, value.c_str());}


CAST_TO_NATIVE_WITH_CONST(eastl::fixed_string<CharType V8TOOLKIT_COMMA Length V8TOOLKIT_COMMA Overflow V8TOOLKIT_COMMA Allocator>, class CharType V8TOOLKIT_COMMA int Length V8TOOLKIT_COMMA bool Overflow V8TOOLKIT_COMMA class Allocator)
    return eastl::fixed_string<CharType, Length, Overflow, Allocator>(*v8::String::Utf8Value(value));
    }
};

CAST_TO_JS_TEMPLATED(eastl::fixed_string<CharType V8TOOLKIT_COMMA Length V8TOOLKIT_COMMA Overflow V8TOOLKIT_COMMA Allocator>, class CharType V8TOOLKIT_COMMA int Length V8TOOLKIT_COMMA bool Overflow V8TOOLKIT_COMMA class Allocator){
    return v8::String::NewFromUtf8(isolate, value.c_str());
}
//
//template<class CharType, int Length, bool Overflow, class Allocator>
//struct CastToNative<eastl::fixed_string<CharType, Length, Overflow, Allocator>> {
//    using ResultType = eastl::fixed_string<CharType, Length, Overflow, Allocator>;
//    ResultType operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
//        return ResultType(*v8::String::Utf8Value(value));
//    }
//};



};

