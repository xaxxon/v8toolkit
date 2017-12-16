#pragma once

#include <list>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <deque>
#include <array>

#include <v8.h>

#include "cast_to_js.h"


namespace v8toolkit {


/**
 * For all pointers that aren't char (const) * strings
 */
template<class T>
struct CastToJS<T *, std::enable_if_t<!is_wrapped_type_v<T> && !std::is_same_v<std::remove_const_t<T>, char>>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, T * const value) const {
        if (value == nullptr) {
            return v8::Undefined(isolate);
        } else {
            return CastToJS<T>()(isolate, *value);
        }
    }
};


/**
 * For all enum types
 */
template<class T>
struct CastToJS<T, std::enable_if_t<std::is_enum_v<T>>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, T const value) const {
        return v8::Number::New(isolate, static_cast<std::underlying_type_t<T>>(value));
    }
};



CAST_TO_JS(bool, { return v8::Boolean::New(isolate, value); });

// integers
CAST_TO_JS(char, { return v8::Integer::New(isolate, value); });

CAST_TO_JS(unsigned char, { return v8::Integer::New(isolate, value); });

CAST_TO_JS(wchar_t, { return v8::Number::New(isolate, value); });

CAST_TO_JS(char16_t, { return v8::Integer::New(isolate, value); });

CAST_TO_JS(char32_t, { return v8::Integer::New(isolate, value); });

CAST_TO_JS(short, { return v8::Integer::New(isolate, value); });

CAST_TO_JS(unsigned short, { return v8::Integer::New(isolate, value); });


CAST_TO_JS(int, { return v8::Number::New(isolate, value); });

CAST_TO_JS(unsigned int, { return v8::Number::New(isolate, value); });

CAST_TO_JS(long, { return v8::Number::New(isolate, value); });

CAST_TO_JS(unsigned long, { return v8::Number::New(isolate, value); });

CAST_TO_JS(long long, { return v8::Number::New(isolate, static_cast<double>(value)); });

CAST_TO_JS(unsigned long long, { return v8::Number::New(isolate, static_cast<double>(value)); });



// floats
CAST_TO_JS(float, { return v8::Number::New(isolate, value); });

CAST_TO_JS(double, { return v8::Number::New(isolate, value); });

CAST_TO_JS(long double, { return v8::Number::New(isolate, value); });


CAST_TO_JS(std::string, { return v8::String::NewFromUtf8(isolate, value.c_str(), v8::String::kNormalString, value.length()); });

CAST_TO_JS(char *, { return v8::String::NewFromUtf8(isolate, value); });

CAST_TO_JS(char const *, { return v8::String::NewFromUtf8(isolate, value); });

template<class T>
struct CastToJS<T **> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, const T ** multi_pointer) {
        return CastToJS<T *>(isolate, *multi_pointer);
    }
};


template<class R, class... Params>
struct CastToJS<std::function<R(Params...)>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::function<R(Params...)> & function) {
        return v8::String::NewFromUtf8(isolate, "CastToJS of std::function not supported yet");
    }
};


/**
* Special passthrough type for objects that want to take javascript object objects directly
*/
template<class T>
struct CastToJS<v8::Local<T>> {
    v8::Local<T> operator()(v8::Isolate * isolate, v8::Local<T> value) {
        //return v8::Local<v8::Value>::New(isolate, object);
        return value;
    }

    v8::Local<T> operator()(v8::Isolate * isolate, v8::Global<T> && value) {
        //return v8::Local<v8::Value>::New(isolate, object);
        return value.Get(isolate);
    }
};


template<class T>
struct CastToJS<v8::Global<T>> {
    v8::Local<T> operator()(v8::Isolate * isolate, v8::Local<T> & value) {
        value;
    }

    v8::Local<T> operator()(v8::Isolate * isolate, v8::Global<T> const & value) {
        return value.Get(isolate);
    }
};


// CastToJS<std::pair<>>
template<typename T>
struct CastToJS<T, std::enable_if_t<xl::is_template_for_v<std::pair, T>>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, T const & pair) const {

        using T1 = typename T::first_type;
        using T2 = typename T::second_type;

        assert(isolate->InContext());
        auto context = isolate->GetCurrentContext();
        auto array = v8::Array::New(isolate);
        (void) array->Set(context, 0, CastToJS<T1 &>()(isolate, pair.first));
        (void) array->Set(context, 1, CastToJS<T2 &>()(isolate, pair.second));
        return array;
    }
};


template<typename T>
v8::Local<v8::Value> cast_to_js_map_helper(v8::Isolate * isolate, T map) {
    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    auto object = v8::Object::New(isolate);

    using NoRefT = std::remove_reference_t<T>;

    using KeyType = typename NoRefT::key_type;
    using ValueType = typename NoRefT::mapped_type;
    using KeyForwardT = std::conditional_t<std::is_rvalue_reference_v<T>, std::add_rvalue_reference_t<KeyType>, std::add_lvalue_reference_t<KeyType>>;
    using ValueForwardT = std::conditional_t<std::is_rvalue_reference_v<T>, std::add_rvalue_reference_t<ValueType>, std::add_lvalue_reference_t<ValueType>>;


    for (auto & pair : map) {
        (void) object->Set(context,
                           CastToJS<std::remove_reference_t<KeyType>>()(isolate, std::forward<KeyForwardT>(
                               const_cast<KeyForwardT>  (pair.first ))),
                           CastToJS<std::remove_reference_t<ValueType>>()(isolate, std::forward<ValueForwardT>(
                               const_cast<ValueForwardT>(pair.second)))
        );
    }
    return object;
}


template<typename T>
v8::Local<v8::Value> cast_to_js_vector_helper(v8::Isolate * isolate, T vector) {

    using NoRefT = std::remove_reference_t<T>;

    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    auto array = v8::Array::New(isolate);

    using RefMatchedValueType = std::conditional_t<std::is_lvalue_reference_v<T>, typename NoRefT::value_type &, typename NoRefT::value_type &&>;

    int i = 0;
    for (auto & element : vector) {
        (void) array->Set(context, i, CastToJS<std::remove_reference_t<typename NoRefT::value_type>>()(isolate,
            const_cast<RefMatchedValueType>(element)));
        i++;
    }
    return array;
}


// CastToJS<std::vector<>>
template<class T>
struct CastToJS<T, std::enable_if_t<xl::is_template_for_v<std::vector, T>>> {
    using NoRefT = std::remove_reference_t<T>;
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, NoRefT && vector) const {
        return cast_to_js_vector_helper<NoRefT &&>(isolate, std::move(vector));
    }

    v8::Local<v8::Value> operator()(v8::Isolate * isolate, NoRefT const & vector) const {
        return cast_to_js_vector_helper<NoRefT const &>(isolate, vector);
    }


//
//
//    template<class T, class... Rest>
//    v8::Local<v8::Value>
//    CastToJS<std::vector<T, Rest...>>::operator()(v8::Isolate * isolate, std::vector<T, Rest...> const & vector) {
//        return cast_to_js_vector_helper<std::vector, T &, Rest...>(isolate, vector);
//    }
//
//    template<class T, class... Rest>
//    v8::Local<v8::Value>
//    CastToJS<std::vector<T, Rest...>>::operator()(v8::Isolate * isolate, std::vector<T, Rest...> && vector) {
//        return cast_to_js_vector_helper<std::vector, T &&, Rest...>(isolate, vector);
//    }

};


// CastToJS<std::list>
template<class U, class... Rest>
struct CastToJS<std::list<U, Rest...>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::list<U, Rest...> & list);

    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::list<U, Rest...> && list) {
        return this->operator()(isolate, list);
    }
};

// CastToJS<std::map>
template<typename T>
struct CastToJS<T, std::enable_if_t<xl::is_template_for_v<std::map, T>>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, T const & map) {
        return cast_to_js_map_helper<T>(isolate, map);
    }

    v8::Local<v8::Value> operator()(v8::Isolate * isolate, T && map) {
        return cast_to_js_map_helper<T>(isolate, map);
    }

};

// CastToJS<std::multimap>
template<class A, class B, class... Rest>
struct CastToJS<std::multimap<A, B, Rest...>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::multimap<A, B, Rest...> & multimap);

    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::multimap<A, B, Rest...> && multimap) {
        return this->operator()(isolate, multimap);
    }
};


// CastToJS<std::undordered:map>
template<class A, class B, class... Rest>
struct CastToJS<std::unordered_map<A, B, Rest...>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::unordered_map<A, B, Rest...> & unorderedmap);

    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::unordered_map<A, B, Rest...> && unorderedmap) {
        return this->operator()(isolate, unorderedmap);
    }
};

// CastToJS<std::deque>
template<class T, class... Rest>
struct CastToJS<std::deque<T, Rest...>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::deque<T, Rest...> & deque);

    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::deque<T, Rest...> && deque) {
        return this->operator()(isolate, deque);
    }
};

// CastToJS<std::array>
template<class T, std::size_t N>
struct CastToJS<std::array<T, N>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::array<T, N> & arr);

    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::array<T, N> && arr) {
        return this->operator()(isolate, arr);
    }
};


/**
 * This is for when a function returns a std::unique - meaning it likely allocated new memory on its own
 * If this is being sent back to JS, the unique_ptr must release the memory, because the unique_ptr is going to
 * go out of scope immediately
 *
 * These functions are not const because they call unique_ptr::release
 */
template<class T, class... Rest>
struct CastToJS<std::unique_ptr<T, Rest...>, std::enable_if_t<!is_wrapped_type_v<T>>> {

    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::unique_ptr<T, Rest...> & unique_ptr) {
        return CastToJS<T>()(isolate, *unique_ptr.get());
    }


    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::unique_ptr<T, Rest...> && unique_ptr) {
        auto result = CastToJS<T>()(isolate, std::move(*unique_ptr));
        unique_ptr.reset();
        return result;
    }

};


/**
 * If a data structure contains a unique_ptr and that is being returned, the unique_ptr should not ::release()
 * its memory.  This is treated just as if the call were returning a T* instead of a unique_ptr<T>
 */
template<class T, class... Rest>
struct CastToJS<std::unique_ptr<T, Rest...> &, std::enable_if_t<!is_wrapped_type_v<std::unique_ptr<T, Rest...>>>> {
v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::unique_ptr<T, Rest...> const & unique_ptr) {
//    fprintf(stderr, "**NOT** releasing UNIQUE_PTR MEMORY for ptr type %s\n", demangle<T>().c_str());
    if (unique_ptr.get() == nullptr) {
        return v8::Undefined(isolate);
    } else {
        return CastToJS<T *>()(isolate, unique_ptr.get());
    }
}

};
template<class T, class... Rest>
struct CastToJS<std::unique_ptr<T, Rest...> const &, std::enable_if_t<!is_wrapped_type_v<std::unique_ptr<T, Rest...> const>>> {
v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::unique_ptr<T, Rest...> const & unique_ptr) {
//    fprintf(stderr, "**NOT** releasing UNIQUE_PTR MEMORY for ptr type %s\n", demangle<T>().c_str());

    if (unique_ptr.get() == nullptr) {
        return v8::Undefined(isolate);
    } else {
        return CastToJS<T *>()(isolate, unique_ptr.get());
    }
}

};


template<class T>
struct CastToJS<std::shared_ptr<T>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::shared_ptr<T> const & shared_ptr) {
        return CastToJS<T>()(isolate, *shared_ptr.get());
    }
};



/**
* supports lists containing any type also supported by CastToJS to javascript arrays
*/
template<class T, class... Rest>
v8::Local<v8::Value>
CastToJS<std::list<T, Rest...>>::operator()(v8::Isolate * isolate, std::list<T, Rest...> & list) {
    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    auto array = v8::Array::New(isolate);
    int i = 0;
    for (auto & element : list) {
        (void) array->Set(context, i, CastToJS<T>()(isolate, element));
        i++;
    }
    return array;
}


template<template<class, class, class...> class MultiMapLike, class A, class B, class... Rest>
v8::Local<v8::Object> casttojs_multimaplike(v8::Isolate * isolate, MultiMapLike<A, B, Rest...> const & map) {
    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    auto object = v8::Object::New(isolate);
    for (auto & pair : map) {
        auto key = CastToJS<A>()(isolate, const_cast<A &>(pair.first));
        auto value = CastToJS<B>()(isolate, const_cast<B &>(pair.second));

        // check to see if a value with this key has already been added
        bool default_value = true;
        bool object_has_key = object->Has(context, key).FromMaybe(default_value);
        if (!object_has_key) {
            // get the existing array, add this value to the end
            auto array = v8::Array::New(isolate);
            (void) array->Set(context, 0, value);
            (void) object->Set(context, key, array);
        } else {
            // create an array, add the current value to it, then add it to the object
            auto existing_array_value = object->Get(context, key).ToLocalChecked();
            v8::Handle<v8::Array> existing_array = v8::Handle<v8::Array>::Cast(existing_array_value);

            //find next array position to insert into (is there no better way to push onto the end of an array?)
            int i = 0;
            while (existing_array->Has(context, i).FromMaybe(default_value)) { i++; }
            (void) existing_array->Set(context, i, value);
        }
    }
    return object;
}

/**
* supports maps containing any type also supported by CastToJS to javascript arrays
* It creates an object of key => [values...]
* All values are arrays, even if there is only one value in the array.
*/
template<class A, class B, class... Rest>
v8::Local<v8::Value>
CastToJS<std::multimap<A, B, Rest...>>::operator()(v8::Isolate * isolate, std::multimap<A, B, Rest...> & map) {
    return casttojs_multimaplike(isolate, map);
}




template<int position, class T>
struct CastTupleToJS;

template<class... Args>
struct CastTupleToJS<0, std::tuple<Args...>> {
    v8::Local<v8::Array> operator()(v8::Isolate * isolate, std::tuple<Args...> & tuple) {
        constexpr const int array_position = sizeof...(Args) - 0 - 1;

        assert(isolate->InContext());
        auto context = isolate->GetCurrentContext();
        auto array = v8::Array::New(isolate);
        using TuplePositionType = typename std::tuple_element_t<array_position, std::tuple<Args...>>;

        (void) array->Set(context,
                          array_position,
                          CastToJS<TuplePositionType>()(isolate,
                                                        std::get<array_position>(tuple)));
        return array;
    }
};

template<int position, class... Args>
struct CastTupleToJS<position, std::tuple<Args...>> {
    v8::Local<v8::Array> operator()(v8::Isolate * isolate, std::tuple<Args...> & tuple) {
        constexpr const int array_position = sizeof...(Args) - position - 1;
        using TuplePositionType = typename std::tuple_element_t<array_position, std::tuple<Args...>>;

        assert(isolate->InContext());
        auto context = isolate->GetCurrentContext();
        auto array = CastTupleToJS<position - 1, std::tuple<Args...>>()(isolate, tuple);
        (void) array->Set(context,
                          array_position,
                          CastToJS<TuplePositionType>()(isolate,
                                                        std::get<array_position>(tuple)));
        return array;
    }
};


template<class... Args>
struct CastToJS<std::tuple<Args...>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::tuple<Args...> tuple) {
        return CastTupleToJS<sizeof...(Args) - 1, std::tuple<Args...>>()(isolate, tuple);
    }
};


/**
* supports unordered_maps containing any type also supported by CastToJS to javascript arrays
*/
template<class A, class B, class... Rest>
v8::Local<v8::Value>
CastToJS<std::unordered_map<A, B, Rest...>>::operator()(v8::Isolate * isolate,
                                                        std::unordered_map<A, B, Rest...> & map) {
    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    auto object = v8::Object::New(isolate);
    for (auto pair : map) {
        (void) object->Set(context, CastToJS<A &>()(isolate, pair.first), CastToJS<B &>()(isolate, pair.second));
    }
    return object;
}


/**
* supports deques containing any type also supported by CastToJS to javascript arrays
*/
template<class T, class... Rest>
v8::Local<v8::Value>
CastToJS<std::deque<T, Rest...>>::operator()(v8::Isolate * isolate, std::deque<T, Rest...> & deque) {
    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    auto array = v8::Array::New(isolate);
    auto size = deque.size();
    for (unsigned int i = 0; i < size; i++) {
        (void) array->Set(context, i, CastToJS<T>()(isolate, deque.at(i)));
    }
    return array;
}


template<class T, std::size_t N>
v8::Local<v8::Value>
CastToJS<std::array<T, N>>::operator()(v8::Isolate * isolate, std::array<T, N> & arr) {
    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    auto array = v8::Array::New(isolate);
    // auto size = arr.size();
    for (unsigned int i = 0; i < N; i++) {
        (void) array->Set(context, i, CastToJS<T>()(isolate, arr[i]));
    }
    return array;
}



//TODO: forward_list

//TODO: stack



template<class T>
struct CastToJS<T, std::enable_if_t<xl::is_template_for_v<std::set, T>>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, T const & set) {
        return cast_to_js_vector_helper<T>(isolate, set);
    }

    v8::Local<v8::Value> operator()(v8::Isolate * isolate, T && set) {
        return cast_to_js_vector_helper<T>(isolate, set);
    }
};


template<typename T>
struct CastToJS<T, std::enable_if_t<xl::is_template_for_v<std::optional, T>>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, T optional) {
        using NoRefT = std::remove_reference_t<T>;
        using ConstMatchedValueType = xl::match_const_of_t<typename NoRefT::value_type, T>;
        if (optional) {
            return CastToJS<ConstMatchedValueType>()(isolate, *optional);
        } else {
            return v8::Undefined(isolate);
        }
    }
};


} // end namespace v8toolkit