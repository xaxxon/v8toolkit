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
#include <EASTL/bonus/ring_buffer.h>
#include <EASTL/map.h>

#include "cast_to_native_impl.h"

namespace v8toolkit {



template<class FirstT, class SecondT>
struct CastToNative<eastl::pair<FirstT, SecondT>>{
    eastl::pair<FirstT, SecondT> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        return pair_type_helper<eastl::pair, FirstT, SecondT>(isolate, value);
    }
};


template<typename T, typename Behavior>
struct CastToJS<T, Behavior, std::enable_if_t<
    xl::is_template_for_v<eastl::pair, T>
>> {
    
    using NoRefT = std::remove_reference_t<T>;
    
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, T const & pair) const {

        
        using T1 = typename NoRefT::first_type;
        using T2 = typename NoRefT::second_type;

        assert(isolate->InContext());
        auto context = isolate->GetCurrentContext();
        auto array = v8::Array::New(isolate);
        (void) array->Set(context, 0, CastToJS<T1 &, Behavior>()(isolate, pair.first));
        (void) array->Set(context, 1, CastToJS<T2 &, Behavior>()(isolate, pair.second));
        return array;
    }
};



// EASTL VECTOR_MAP
template<typename T>
struct CastToNative<T, std::enable_if_t<
    xl::is_template_for_v<eastl::vector_map, T>
>> {
    using NoRefT = std::remove_reference_t<T>;
    using key_type = typename NoRefT::key_type;
    using mapped_type = typename NoRefT::mapped_type;
    T operator()(v8::Isolate *isolate, v8::Local <v8::Value> value) const {
        return map_type_helper<eastl::vector_map, key_type, mapped_type>(isolate, value);
    }
};


template<class T>
struct CastToNative<T, std::enable_if_t<
    xl::is_template_for_v<eastl::vector, T> ||
    xl::is_template_for_v<eastl::ring_buffer, T>
>> 
{
    using NoRefT = std::remove_reference_t<T>;
    using value_type = typename NoRefT::value_type;
    using allocator = typename NoRefT::allocator_type;
    
    auto operator()(v8::Isolate *isolate, v8::Local <v8::Value> value) const {
        return vector_type_helper<eastl::vector, value_type, allocator>(isolate, value);
    }
};



// EASTL VECTOR_SET
template<class T, class... Args>
struct CastToNative<eastl::vector_set<T, Args...>> {
    eastl::vector_set<T, Args...> operator()(v8::Isolate *isolate, v8::Local <v8::Value> value) const {
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
        return multimap_type_helper<eastl::vector_multimap, Key, Value, Args...>(isolate, value);
    }
};

CAST_TO_NATIVE(eastl::string, {return eastl::string(*v8::String::Utf8Value(value)); });

CAST_TO_JS(eastl::string, {return v8::String::NewFromUtf8(isolate, value.c_str());});


template<typename T>
struct IsEastlFixedString_helper : std::false_type {};

template<typename T, int nodeCount, bool bEnableOverflow, typename OverflowAllocator>
struct IsEastlFixedString_helper<eastl::fixed_string<T, nodeCount, bEnableOverflow, OverflowAllocator>> : std::true_type {};

template<typename T>
struct IsEastlFixedString : IsEastlFixedString_helper<std::decay_t<T>> {};

template<typename T>
constexpr bool IsEastlFixedString_v = IsEastlFixedString<T>::value;


template<typename T, typename Behavior>
struct CastToJS<T, Behavior, std::enable_if_t<
    IsEastlFixedString_v<T>
>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, T const & value) const {
        return v8::String::NewFromUtf8(isolate, value.c_str(), v8::String::kNormalString, value.length());
    }
};


template<typename T>
struct CastToNative<T, std::enable_if_t<
    IsEastlFixedString_v<T>
>> {
    T operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        return T(*v8::String::Utf8Value(value));
    }
};


// CastToJS<eastl::vector<>>
template<class T, typename Behavior>
struct CastToJS<T, Behavior, std::enable_if_t<
    xl::is_template_for_v<eastl::vector, T> ||
    xl::is_template_for_v<eastl::ring_buffer, T>
        
    >
> {

    using NoRefT = std::remove_reference_t<T>;
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, NoRefT const & vector) {
        return cast_to_js_vector_helper<NoRefT const &, Behavior>(isolate, vector);
    }

    v8::Local<v8::Value> operator()(v8::Isolate *isolate, NoRefT && vector) {
        return cast_to_js_vector_helper<NoRefT &&, Behavior>(isolate, vector);
    }
};





template<typename T, typename Behavior>
struct CastToJS<T, Behavior, std::enable_if_t<
    xl::is_template_for_v<eastl::vector_multimap, T> ||
    xl::is_template_for_v<eastl::multimap, T>
    > 
> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, T const & multimap) {
        return casttojs_multimaplike<Behavior>(isolate, multimap);
    }
};


template<typename T, typename Behavior>
struct CastToJS<T, Behavior, std::enable_if_t<xl::is_template_for_v<eastl::vector_map, T>>> {
    using NoRefT = std::remove_reference_t<T>;
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, NoRefT const & map) const {
        return cast_to_js_map_helper<NoRefT const &, Behavior>(isolate, map);
    }
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, NoRefT && map) const{
        return cast_to_js_map_helper<NoRefT &&, Behavior>(isolate, std::move(map));
    }

};



template<class T, typename Behavior>
struct CastToJS<T, Behavior, std::enable_if_t<xl::is_template_for_v<eastl::vector_set, T>>> {
    using NoRefT = std::remove_reference_t<T>;
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, NoRefT const & set) {
        return cast_to_js_vector_helper<NoRefT const &, Behavior>(isolate, set);
    }
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, NoRefT && set) {
        return cast_to_js_vector_helper<NoRefT &&, Behavior>(isolate, set);
    }
};





} // namespace v8toolkit

