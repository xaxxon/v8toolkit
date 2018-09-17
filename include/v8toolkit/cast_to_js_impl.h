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
template<typename T, typename Behavior>
struct CastToJS<T *, Behavior, std::enable_if_t<!is_wrapped_type_v<T> && !std::is_same_v<std::remove_const_t<T>, char>>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, T * const value) const {
        if (value == nullptr) {
            return v8::Undefined(isolate);
        } else {
            return Behavior()(*value);
        }
    }
};


/**
 * For all enum types
 */
template<typename T, typename Behavior>
struct CastToJS<T, Behavior, std::enable_if_t<std::is_enum_v<T>>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, T const value) const {
        return v8::Number::New(isolate, static_cast<std::underlying_type_t<T>>(value));
    }
};


template<>
struct CastToJS<void *> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, void * value) const {
        return v8::String::NewFromUtf8(isolate, fmt::format("{}", value).c_str());
    }
    static constexpr bool callable(){return true;}
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

template<typename T, typename Behavior>
struct CastToJS<T **, Behavior> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, const T ** multi_pointer) {
        return CastToJS<T *, Behavior>(isolate, *multi_pointer);
    }
};


template<class R, class... Params, typename Behavior>
struct CastToJS<std::function<R(Params...)>, Behavior> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::function<R(Params...)> & function) {
        return v8::String::NewFromUtf8(isolate, "CastToJS of std::function not supported yet");
    }
};


/**
* Special passthrough type for objects that want to take javascript object objects directly
*/
template<typename T, typename Behavior>
struct CastToJS<T, Behavior, std::enable_if_t<xl::is_template_for_v<v8::Local, T>>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, T value) {
        //return v8::Local<v8::Value>::New(isolate, object);
        return value;
    }
};


template<typename T, typename Behavior>
struct CastToJS<T, Behavior, std::enable_if_t<xl::is_template_for_v<v8::Global, T>>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, T value) {
        return value.Get(isolate);
    }
};





// CastToJS<std::pair<>>
template<typename T, typename Behavior>
struct CastToJS<T, Behavior, std::enable_if_t<xl::is_template_for_v<std::pair, T>>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, T const & pair) const {

//        using T1 = typename T::first_type;
//        using T2 = typename T::second_type;

        assert(isolate->InContext());
        auto context = isolate->GetCurrentContext();
        auto array = v8::Array::New(isolate);
        (void) array->Set(context, 0, Behavior()(pair.first));
        (void) array->Set(context, 1, Behavior()(pair.second));
        return array;
    }
};


template<typename T, typename Behavior>
v8::Local<v8::Value> cast_to_js_map_helper(v8::Isolate * isolate, T map) {
    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    auto object = v8::Map::New(isolate);

    using NoRefT = std::remove_reference_t<T>;

    using KeyType = typename NoRefT::key_type;
    using ValueType = typename NoRefT::mapped_type;
    using KeyForwardT = std::conditional_t<std::is_rvalue_reference_v<T>, std::add_rvalue_reference_t<KeyType>, std::add_lvalue_reference_t<KeyType>>;
    using ValueForwardT = std::conditional_t<std::is_rvalue_reference_v<T>, std::add_rvalue_reference_t<ValueType>, std::add_lvalue_reference_t<ValueType>>;


    for (auto & pair : map) {
        (void) object->Set(context,
                           Behavior()(std::forward<KeyForwardT>(const_cast<KeyForwardT>  (pair.first ))),
                           Behavior()(std::forward<ValueForwardT>(const_cast<ValueForwardT>(pair.second)))
        );
    }
    return object;
}


template<typename T, typename Behavior>
v8::Local<v8::Value> cast_to_js_vector_helper(v8::Isolate * isolate, T vector) {

    using NoRefT = std::remove_reference_t<T>;

    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    auto array = v8::Array::New(isolate);

    using RefMatchedValueType = std::conditional_t<std::is_lvalue_reference_v<T>, typename NoRefT::value_type &, typename NoRefT::value_type &&>;

    int i = 0;
    for (auto & element : vector) {
        (void) array->Set(context, i, Behavior()(const_cast<RefMatchedValueType>(element)));
        i++;
    }
    return array;
}


// CastToJS<std::vector<>>
template<typename T, typename Behavior>
struct CastToJS<T, Behavior, std::enable_if_t<xl::is_template_for_v<std::vector, T>>> {
    using NoRefT = std::remove_reference_t<T>;
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, NoRefT && vector) const {
        return cast_to_js_vector_helper<NoRefT &&, Behavior>(isolate, std::move(vector));
    }

    v8::Local<v8::Value> operator()(v8::Isolate * isolate, NoRefT const & vector) const {
        return cast_to_js_vector_helper<NoRefT const &, Behavior>(isolate, vector);
    }
};


// CastToJS<std::list>
template<typename T, typename Behavior>
struct CastToJS<T, Behavior, std::enable_if_t<xl::is_template_for_v<std::list, T>>> {
    using NoRefT = std::remove_reference_t<T>;
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, NoRefT const & list) {
            assert(isolate->InContext());
            auto context = isolate->GetCurrentContext();
            auto array = v8::Array::New(isolate);
            int i = 0;
            for (auto & element : list) {
                (void) array->Set(context, i, Behavior()(element));
                i++;
            }
            return array;
    }

    v8::Local<v8::Value> operator()(v8::Isolate * isolate, NoRefT && list) {
        return this->operator()(isolate, list);
    }
};


// CastToJS<std::map>
template<typename T, typename Behavior>
struct CastToJS<T, Behavior, std::enable_if_t<xl::is_template_for_v<std::map, T>>> {
    using NoRefT = std::remove_reference_t<T>;

    v8::Local<v8::Value> operator()(v8::Isolate * isolate, NoRefT const & map) {
        return cast_to_js_map_helper<T, Behavior>(isolate, map);
    }

    v8::Local<v8::Value> operator()(v8::Isolate * isolate, NoRefT && map) {
        return cast_to_js_map_helper<T, Behavior>(isolate, map);
    }

};



template<typename Behavior, typename T>
v8::Local<v8::Object> casttojs_multimaplike(v8::Isolate * isolate, T const & multimap) {
    using A = typename T::key_type;
    using B = typename T::mapped_type;
//    using Compare = typename T::key_compare;
//    using Allocator = typename T::allocator_type;
    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    auto map = v8::Map::New(isolate);
    for (auto & pair : multimap) {
        auto key = Behavior{}.template operator()(const_cast<A &>(pair.first));
        auto value = Behavior{}.template operator()(const_cast<B &>(pair.second));

        // check to see if a value with this key has already been added
        bool default_value = true;
        bool map_has_key = map->Has(context, key).FromMaybe(default_value);
        if (!map_has_key) {
            // get the existing array, add this value to the end
            auto array = v8::Array::New(isolate);
            (void) array->Set(context, 0, value);
            (void) map->Set(context, key, array);
        } else {
            // create an array, add the current value to it, then add it to the object
            auto existing_array_value = map->Get(context, key).ToLocalChecked();
            v8::Handle<v8::Array> existing_array = v8::Local<v8::Array>::Cast(existing_array_value);

            //find next array position to insert into (is there no better way to push onto the end of an array?)
//            int i = 0;
//            while (existing_array->Has(context, i).FromMaybe(default_value)) { i++; }
            (void) existing_array->Set(context, existing_array->Length(), value);
        }
    }
    return map;
}


// CastToJS<std::multimap>
template<typename T, typename Behavior>
struct CastToJS<T, Behavior, std::enable_if_t<xl::is_template_for_v<std::multimap, T>>> {
    using NoRefT = std::remove_reference_t<T>;

    v8::Local<v8::Value> operator()(v8::Isolate * isolate, NoRefT & multimap) {
        return casttojs_multimaplike<Behavior>(isolate, multimap);
    }

    v8::Local<v8::Value> operator()(v8::Isolate * isolate, NoRefT && multimap) {
        return this->operator()(isolate, multimap);
    }
};


// CastToJS<std::undordered:map>
template<typename T, typename Behavior>
struct CastToJS<T, Behavior, std::enable_if_t<xl::is_template_for_v<std::unordered_map, T>>> {
    using NoRefT = std::remove_reference_t<T>;

    v8::Local<v8::Value> operator()(v8::Isolate * isolate, NoRefT & unordered_map) {
        assert(isolate->InContext());
        auto context = isolate->GetCurrentContext();
        auto object = v8::Map::New(isolate);
        for (auto pair : unordered_map) {
            (void) object->Set(context,
                               Behavior()(pair.first),
                               Behavior()(pair.second));
        }
        return object;
    }

    v8::Local<v8::Value> operator()(v8::Isolate * isolate, NoRefT && unordered_map) {
        return this->operator()(isolate, unordered_map);
    }
};

// CastToJS<std::deque>
template<typename T, typename Behavior>
struct CastToJS<T, Behavior, std::enable_if_t<xl::is_template_for_v<std::deque, T>>> {
    using NoRefT = std::remove_reference_t<T>;

    v8::Local<v8::Value> operator()(v8::Isolate * isolate, NoRefT & deque) {
        assert(isolate->InContext());
        auto context = isolate->GetCurrentContext();
        auto array = v8::Array::New(isolate);
        auto size = deque.size();
        for (unsigned int i = 0; i < size; i++) {
            (void) array->Set(context, i, Behavior()(deque[i]));
        }
        return array;
    }

    v8::Local<v8::Value> operator()(v8::Isolate * isolate, NoRefT && deque) {
        return this->operator()(isolate, deque);
    }
};

// CastToJS<std::array>
template<typename T, typename Behavior>
struct CastToJS<T, Behavior, std::enable_if_t<xl::is_std_array_v<T>>> {
    using NoRefT = std::remove_reference_t<T>;

    v8::Local<v8::Value> operator()(v8::Isolate * isolate, NoRefT & arr) {
        assert(isolate->InContext());
        auto context = isolate->GetCurrentContext();
        auto array = v8::Array::New(isolate);
        // auto size = arr.size();
        for (unsigned int i = 0; i < arr.size(); i++) {
            (void) array->Set(context, i, Behavior()(arr[i]));
        }
        return array;
    }

    v8::Local<v8::Value> operator()(v8::Isolate * isolate, NoRefT && arr) {
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
template<typename T, typename Behavior>
struct CastToJS<T, Behavior, std::enable_if_t<
    xl::is_template_for_v<std::unique_ptr, T> && 
    !is_wrapped_type_v<typename std::remove_reference_t<T>::element_type>
>> 
{
    using NoRefT = std::remove_reference_t<T>;
    using NoRefNoConstT = std::remove_const_t<NoRefT>;

    v8::Local<v8::Value> operator()(v8::Isolate * isolate, NoRefT const & unique_ptr) {
        return Behavior()(*unique_ptr.get());
    }

    // if T is const, then don't allow moving of any sort
    template<typename U = NoRefT>
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::remove_reference_t<U> && unique_ptr) {
        static_assert(!std::is_const_v<U>, "cannot pass an rvalue when the type is specified as const");
        auto result = Behavior()(std::move(*unique_ptr));
        unique_ptr.reset();
        return result;
    }
};




template<typename T, typename Behavior>
struct CastToJS<std::shared_ptr<T>, Behavior> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::shared_ptr<T> const & shared_ptr) {
        return Behavior()(*shared_ptr.get());
    }
};



template<typename Behavior, typename... Args, size_t... Is>
v8::Local<v8::Array> cast_tuple_to_js(v8::Isolate * isolate, std::tuple<Args...> const & tuple, std::index_sequence<Is...>) {
    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    v8::Local<v8::Array> array = v8::Array::New(isolate);
    ((void)array->Set(context, Is, Behavior()(std::get<Is>(tuple))),...);
    return array;
}



template<typename T, typename Behavior>
struct CastToJS<T, Behavior, std::enable_if_t<xl::is_template_for_v<std::tuple, T>>> {

private:

    // private helper gets a parameter pack of the types in the tuple to fold over
    template<typename... Args>
    v8::Local<v8::Value> private_helper(v8::Isolate * isolate, std::tuple<Args...> const & tuple) {
        return cast_tuple_to_js<Behavior>(isolate, tuple, std::index_sequence_for<Args...>());
    }

public:
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, T const & tuple) {
        return private_helper(isolate, tuple);
    }
};




//TODO: forward_list

//TODO: stack



template<typename T, typename Behavior>
struct CastToJS<T, Behavior, std::enable_if_t<xl::is_template_for_v<std::set, T>>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::remove_reference_t<T> const & set) {
        return cast_to_js_vector_helper<T, Behavior>(isolate, set);
    }

    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::remove_reference_t<T> && set) {
        return cast_to_js_vector_helper<T, Behavior>(isolate, set);
    }
};

//
//template<typename T, typename Behavior>
//struct CastToJS<T, Behavior, std::enable_if_t<xl::is_template_for_v<std::optional, T>>> {
//    v8::Local<v8::Value> operator()(v8::Isolate * isolate, T optional) {
////        using NoRefT = std::remove_reference_t<T>;
////        using ConstMatchedValueType = xl::match_const_of_t<typename NoRefT::value_type, T>;
//        if (optional) {
//            return Behavior()(*optional);
//        } else {
//            return v8::Undefined(isolate);
//        }
//    }
//};


// a "maybe" type is something that acts like an optional or
//   pointer which can either be false/null or otherwise contains a value
// *except* owning types which can transfer ownership like std::unique_ptr
template<typename T, typename = void>
struct is_maybe_type : std::false_type {};

template<typename T>
struct is_maybe_type<T, std::enable_if_t<
    !std::is_pointer_v<std::remove_reference_t<T>> &&
    !is_owning_type_v<T> &&
    (true || static_cast<bool>(std::declval<T>())) &&
    std::is_same_v<void, std::void_t<decltype(*std::declval<T>())>>
    >> : std::true_type {};

template<typename T>
constexpr bool is_maybe_type_v = is_maybe_type<T>::value;

template<typename T, typename Behavior>
struct CastToJS<T, Behavior, std::enable_if_t<is_maybe_type_v<T>>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, T optional) {
//        using NoRefT = std::remove_reference_t<T>;
//        using ConstMatchedValueType = xl::match_const_of_t<typename NoRefT::value_type, T>;
        if (optional) {
            return Behavior()(*optional);
        } else {
            return v8::Undefined(isolate);
        }
    }
};

static_assert(
    !is_maybe_type_v<char *> &&
    !is_maybe_type_v<int *> &&
    !is_maybe_type_v<std::unique_ptr<int>> && // because it's an owning type
    is_maybe_type_v<std::optional<int>>);


template<typename T, typename Behavior>
struct CastToJS<T, Behavior, std::enable_if_t<std::is_same_v<nullptr_t, std::decay_t<T>>>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, nullptr_t) {
        return v8::Undefined(isolate);
    }
};


} // end namespace v8toolkit