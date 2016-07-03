#pragma once

namespace v8toolkit {

template<class Return, class... Params>
std::function<Return(Params...)> CastToNative<std::function<Return(Params...)>>::operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
    auto js_function = v8toolkit::get_value_as<v8::Function>(value);

    // v8::Global's aren't copyable, but shared pointers to them are. std::functions need everything in them to be copyable
    auto context = isolate->GetCurrentContext();
    auto shared_global_function = std::make_shared<v8::Global<v8::Function>>(isolate, js_function);
    auto shared_global_context = std::make_shared<v8::Global<v8::Context>>(isolate, context);

    return [isolate, shared_global_function, shared_global_context](Params... params) ->Return {
        v8::Locker locker(isolate);
        v8::HandleScope sc(isolate);
        auto context = shared_global_context->Get(isolate);
        return v8toolkit::scoped_run(isolate, context, [&]() ->Return {
            assert(!context.IsEmpty());
            auto result = v8toolkit::call_javascript_function(context,
                                                              shared_global_function->Get(isolate),
                                                              context->Global(),
                                                              std::tuple<Params...>(params...));
            return CastToNative<typename std::remove_reference<Return>::type>()(isolate, result);
        });
    };
}

} // end v8toolkit namespace