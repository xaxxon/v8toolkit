#pragma once

#include <tuple>

#include <v8.h>

#include "cast_to_js_impl.h"
#include "exceptions.h"
#include "v8helpers.h"

namespace v8toolkit {

template<int position, class Tuple>
struct TupleForEach;


/**
 * Creates a vector of JS values corresponding to the values in the specified tuple.  Useful for
 * calling a JS function with
 * @param tuple tuple of values to convert to JS
 * @return vector of JS objects corresponding to the provided tuple values
 */
template<typename... Ts, size_t... Is>
std::vector<v8::Local<v8::Value>> tuple_to_js_values(std::tuple<Ts...> const & tuple, std::index_sequence<Is...>) {
    
    auto isolate = v8::Isolate::GetCurrent();
    using Tuple = std::tuple<Ts...>;
    std::vector<v8::Local<v8::Value>> result;
    result.reserve(sizeof...(Ts));
    
    (result.push_back(CastToJS<std::tuple_element_t<Is, Tuple>>()(isolate, std::get<Is>(tuple))), ...);
    
    return result;
}


/**
 * Convenience version of tuple_to_js_values which creates the index_sequence for you
 * @param tuple tuple of values to convert to JS
 * @return vector of JS objects corresponding to the provided tuple values
 */
template<typename... Ts>
std::vector<v8::Local<v8::Value>> tuple_to_js_values(std::tuple<Ts...> const & tuple) {
    return tuple_to_js_values(tuple, std::index_sequence_for<Ts...>());
}



/**
 *
 */
template<class R, class... OriginalTypes, class... Ts>
v8::Local <v8::Value> call_javascript_function_with_vars(v8::Local <v8::Context> context,
                                                         v8::Local <v8::Function> function,
                                                         R && receiver_object,
                                                         TypeList<OriginalTypes...> type_list,
                                                         Ts && ... ts) {
    auto receiver = make_local(receiver_object);
    auto isolate = context->GetIsolate();
    std::vector<v8::Local < v8::Value>>
    parameters {CastToJS<OriginalTypes>()(isolate, std::forward<Ts>(ts))...};
    auto parameter_count = sizeof...(ts);
    v8::TryCatch tc(isolate);
    
    // make sure not to call operator[] on an empty vector
    v8::Local<v8::Value> * parameter_pointer = parameter_count > 0 ? &parameters[0] : nullptr;
    
    auto maybe_result = function->Call(context, receiver, parameter_count, parameter_pointer);
    if (tc.HasCaught() || maybe_result.IsEmpty()) {
        ReportException(isolate, &tc);
        if (v8toolkit::static_any<std::is_const<std::remove_reference_t<OriginalTypes>>::value...>::value) {
            log.info(LoggingSubjects::Subjects::RUNTIME_EXCEPTION,
                "Some of the types are const, make sure what you are using them for is available on the const type\n");
        }
//        ReportException(isolate, &tc);
        throw V8ExecutionException(isolate, tc);
    }
    return maybe_result.ToLocalChecked();

}

/**
* Returns true on success with the result in the "result" parameter
*/
template<typename T, class TupleType = std::tuple<>>
v8::Local <v8::Value> call_javascript_function(v8::Local<v8::Context> context,
                                               v8::Local<v8::Function> function,
                                               T && receiver_object,
                                               TupleType const & tuple = {}) {
    auto receiver = make_local<v8::Object>(receiver_object);
    constexpr const int tuple_size = std::tuple_size<TupleType>::value;
    std::array<v8::Local < v8::Value>, tuple_size > parameters;
    auto isolate = context->GetIsolate();
    auto v8_values = tuple_to_js_values(tuple);

    v8::TryCatch tc(isolate);

    // printf("\n\n**** Call_javascript_function with receiver: %s\n", stringify_value(v8::Local<v8::Value>::Cast(receiver)).c_str());
    auto maybe_result = function->Call(context, receiver, tuple_size, v8_values.data());
    if (tc.HasCaught() || maybe_result.IsEmpty()) {
        ReportException(isolate, &tc);
        throw V8ExecutionException(isolate, tc);
    }
    return maybe_result.ToLocalChecked();
}

/**
* Returns true on success with the result in the "result" parameter
*/
template<typename T, class TupleType = std::tuple<>>
v8::Local <v8::Value> call_javascript_function(v8::Local <v8::Context> context,
                                               std::string_view function_name,
                                               T && receiver_object,
                                               TupleType const & tuple = {}) {
    auto isolate = context->GetIsolate();
    auto receiver = make_local(receiver_object);

    auto has_own_property_result = receiver->HasOwnProperty(context,
                                           v8::String::NewFromUtf8(isolate,
                                                                   function_name.data(),
                                                                   v8::String::NewStringType::kNormalString,
                                                                   function_name.length()));
    if (!has_own_property_result.IsNothing()) {
        if (has_own_property_result.ToChecked()) {

            ;
            if (auto function_result = get_property_as<v8::Function>(receiver, function_name)) {
                return call_javascript_function(context, *function_result, receiver, tuple);
            } else {
                throw InvalidCallException(
                    fmt::format("receiver has property {} but it is not a function", function_name));
            }
        } else {
            throw InvalidCallException(fmt::format("receiver doesn't have a property named: {}", function_name));
        }
    } else {
        throw InvalidCallException(fmt::format("Failed to check if receiver has function property named: {}", function_name));
    }

}


} // end namespace v8toolkit