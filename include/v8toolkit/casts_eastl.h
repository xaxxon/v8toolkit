#pragma once

#include "casts.h"

// Tell EASTL not to redefine initializer_list
#define EA_HAVE_CPP11_INITIALIZER_LIST 1

#include <EASTL/vector_map.h>
#include <EASTL/vector_multimap.h>
#include <EASTL/utility.h>
#include <EASTL/string.h>
#include <EASTL/fixed_string.h>
#include <EASTL/vector_set.h>
#include <EASTL/vector.h>

namespace v8toolkit {



template<class FirstT, class SecondT>
struct CastToNative<eastl::pair<FirstT, SecondT>>{
    eastl::pair<FirstT, SecondT> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        HANDLE_FUNCTION_VALUES;
        pair_type_helper<eastl::pair, FirstT, SecondT>(isolate, value);
    }
};




// EASTL VECTOR_MAP
template<class Key, class Value, class... Args>
struct CastToNative<eastl::vector_map<Key, Value, Args...>> {
    eastl::vector_map<Key, Value, Args...> operator()(v8::Isolate *isolate, v8::Local <v8::Value> value) const {
        HANDLE_FUNCTION_VALUES;
        return map_type_helper<eastl::vector_map, Key, Value, Args...>(isolate, value);
    }
};


// EASTL VECTOR
template<class T, class... Args>
struct CastToNative<eastl::vector<T, Args...>> {
    eastl::vector<T, Args...> operator()(v8::Isolate *isolate, v8::Local <v8::Value> value) const {
        HANDLE_FUNCTION_VALUES;
        return vector_type_helper<eastl::vector, T, Args...>(isolate, value);
    }
};



// EASTL VECTOR_SET
template<class T, class... Args>
struct CastToNative<eastl::vector_set<T, Args...>> {
    eastl::vector_set<T, Args...> operator()(v8::Isolate *isolate, v8::Local <v8::Value> value) const {
        HANDLE_FUNCTION_VALUES;
        auto vector = vector_type_helper<eastl::vector, T>(isolate, value);
        eastl::vector_set<T, Args...> set;
        for(auto & i : vector) {
            set.emplace(std::move(i));
        }
        return set;
    }
};





template<class Key, class Value, class... Args>
struct CastToNative<eastl::vector_multimap<Key, Value, Args...>> {
    eastl::vector_multimap<Key, Value, Args...> operator()(v8::Isolate *isolate, v8::Local <v8::Value> value) const {
        HANDLE_FUNCTION_VALUES;
        return multimap_type_helper<eastl::vector_multimap, Key, Value, Args...>(isolate, value);
    }
};

CAST_TO_NATIVE(eastl::string, {HANDLE_FUNCTION_VALUES; return eastl::string(*v8::String::Utf8Value(value)); });

CAST_TO_JS(eastl::string, {return v8::String::NewFromUtf8(isolate, value.c_str());});



template<class CharType, int Length, bool Overflow, class Allocator>
struct CastToJS<eastl::fixed_string<CharType, Length, Overflow, Allocator>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, eastl::fixed_string<CharType, Length, Overflow, Allocator> const & value) const {
        return v8::String::NewFromUtf8(isolate, value.c_str(), v8::String::kNormalString, value.length());
    }
};

template<class CharType, int Length, bool Overflow, class Allocator>
struct CastToNative<eastl::fixed_string<CharType, Length, Overflow, Allocator>> {
    eastl::fixed_string<CharType, Length, Overflow, Allocator> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        HANDLE_FUNCTION_VALUES;
        return eastl::fixed_string<CharType, Length, Overflow, Allocator>(*v8::String::Utf8Value(value));
    }
};


// CastToJS<eastl::vector<>>
template<class T, class... Rest>
struct CastToJS<T, std::enable_if_t<xl::is_template_for_v<eastl::vector, T>>> {

    using NoRefT = std::remove_reference_t<T>;
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, NoRefT const & vector) {
        return cast_to_js_vector_helper<NoRefT const &>(isolate, vector);
    }

    v8::Local<v8::Value> operator()(v8::Isolate *isolate, NoRefT && vector) {
        return cast_to_js_vector_helper<NoRefT &&>(isolate, vector);
    }
};





template<class A, class B, class... Rest>
struct CastToJS<eastl::vector_multimap<A, B, Rest...>> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, eastl::vector_multimap<A, B, Rest...> const & multimap) {
        return casttojs_multimaplike(isolate, multimap);
    }
};


template<typename T>
struct CastToJS<T, std::enable_if_t<xl::is_template_for_v<eastl::vector_map, T>>> {
    using NoRefT = std::remove_reference_t<T>;
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, NoRefT const & map) const {
        return cast_to_js_map_helper<NoRefT const &>(isolate, map);
    }
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, NoRefT && map) const{
        return cast_to_js_map_helper<NoRefT &&>(isolate, std::move(map));
    }

};



template<class T>
struct CastToJS<T, std::enable_if_t<xl::is_template_for_v<eastl::vector_set, T>>> {
    using NoRefT = std::remove_reference_t<T>;
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, NoRefT const & set) {
        return cast_to_js_vector_helper<NoRefT const &>(isolate, set);
    }
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, NoRefT && set) {
        return cast_to_js_vector_helper<NoRefT &&>(isolate, set);
    }

};


} // namespace v8toolkit

