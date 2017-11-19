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
* Populates an array of v8::Values with the values of the tuple, casted by the tuple element's type
*/
template<int position, class Tuple>
struct TupleForEach : public TupleForEach<position - 1, Tuple> {
    using super = TupleForEach<position - 1, Tuple>;

    void operator()(v8::Isolate * isolate, v8::Local <v8::Value> * params, const Tuple & tuple) {
        constexpr const int array_position = position - 1;
        params[array_position] = CastToJS<typename std::tuple_element<array_position, Tuple>::type>()(isolate,
                                                                                                      std::get<array_position>(
                                                                                                          tuple));
        super::operator()(isolate, params, tuple);
    }
};

/**
* Base case for no remaining elements to parse (as determined by the position being 0)
*/
template<class Tuple>
struct TupleForEach<0, Tuple> {
    void operator()(v8::Isolate *, v8::Local <v8::Value> *, const Tuple &) {}
};


/**
 *
 */
template<class... OriginalTypes, class... Ts>
v8::Local <v8::Value> call_javascript_function_with_vars(const v8::Local <v8::Context> context,
                                                         const v8::Local <v8::Function> function,
                                                         const v8::Local <v8::Object> receiver,
                                                         const TypeList<OriginalTypes...> & type_list,
                                                         Ts && ... ts) {
    auto isolate = context->GetIsolate();
    std::vector<v8::Local < v8::Value>>
    parameters {CastToJS<OriginalTypes>()(isolate, std::forward<Ts>(ts))...};
    auto parameter_count = sizeof...(ts);
    v8::TryCatch tc(isolate);
    auto maybe_result = function->Call(context, receiver, parameter_count, &parameters[0]);
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
template<class TupleType = std::tuple<>>
v8::Local <v8::Value> call_javascript_function(const v8::Local <v8::Context> context,
                                               const v8::Local <v8::Function> function,
                                               const v8::Local <v8::Object> receiver,
                                               const TupleType & tuple = {}) {
    constexpr const int tuple_size = std::tuple_size<TupleType>::value;
    std::array<v8::Local < v8::Value>, tuple_size > parameters;
    auto isolate = context->GetIsolate();
    TupleForEach<tuple_size, TupleType>()(isolate, parameters.data(), tuple);

    v8::TryCatch tc(isolate);

    // printf("\n\n**** Call_javascript_function with receiver: %s\n", stringify_value(isolate, v8::Local<v8::Value>::Cast(receiver)).c_str());
    auto maybe_result = function->Call(context, receiver, tuple_size, parameters.data());
    if (tc.HasCaught() || maybe_result.IsEmpty()) {
        ReportException(isolate, &tc);
        throw V8ExecutionException(isolate, tc);
    }
    return maybe_result.ToLocalChecked();
}

/**
* Returns true on success with the result in the "result" parameter
*/
template<class TupleType = std::tuple<>>
v8::Local <v8::Value> call_javascript_function(const v8::Local <v8::Context> context,
                                               const std::string & function_name,
                                               const v8::Local <v8::Object> receiver,
                                               const TupleType & tuple = {}) {
    auto maybe_value = receiver->Get(context, v8::String::NewFromUtf8(context->GetIsolate(), function_name.c_str()));
    if (maybe_value.IsEmpty()) {
        throw InvalidCallException(fmt::format("Function name {} could not be found", function_name));
    }

    auto value = maybe_value.ToLocalChecked();
    if (!value->IsFunction()) {
        throw InvalidCallException(fmt::format("{} was found but is not a function", function_name));;
    }

    return call_javascript_function(context, v8::Local<v8::Function>::Cast(value), receiver, tuple);
}


} // end namespace v8toolkit