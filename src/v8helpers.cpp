#include <assert.h>
#include <sstream>
#include <set>

#include "v8helpers.h"

namespace v8toolkit {


std::string demangle_typeid_name(const std::string & mangled_name) {

#ifdef V8TOOLKIT_DEMANGLE_NAMES
//    printf("Starting name demangling\n");
    std::string result;
    int status;
    auto demangled_name_needs_to_be_freed = abi::__cxa_demangle(mangled_name.c_str(), nullptr, 0, &status);
    result = demangled_name_needs_to_be_freed;

    if (demangled_name_needs_to_be_freed == nullptr) {
	return mangled_name;
    }
    
    if (status == 0) {
        result = demangled_name_needs_to_be_freed;
    } else {
        // https://gcc.gnu.org/onlinedocs/libstdc++/libstdc++-html-USERS-4.3/a01696.html
        //-1: A memory allocation failiure occurred.
        //-2: mangled_name is not a valid name under the C++ ABI mangling rules.
        //-3: One of the arguments is invalid.
        result = mangled_name;
    }
    if (demangled_name_needs_to_be_freed) {
        free(demangled_name_needs_to_be_freed);
    }
    return result;

#else
    return mangled_name;
#endif
}


int get_array_length(v8::Isolate * isolate, v8::Local<v8::Array> array) {
    auto context = isolate->GetCurrentContext();
    return array->Get(context, v8::String::NewFromUtf8(isolate, "length")).ToLocalChecked()->Uint32Value(); 
}


int get_array_length(v8::Isolate * isolate, v8::Local<v8::Value> array_value)
{
    if(array_value->IsArray()) {
        return get_array_length(isolate, v8::Local<v8::Array>::Cast(array_value));
    } else {
        // TODO: probably throw?
        assert(array_value->IsArray());
    }
    assert(false); // shut up the compiler
}


void set_global_object_alias(v8::Isolate * isolate, const v8::Local<v8::Context> context, std::string alias_name)
{
    auto global_object = context->Global();
    (void)global_object->Set(context, v8::String::NewFromUtf8(isolate, alias_name.c_str()), global_object);
    
}



void print_v8_value_details(v8::Local<v8::Value> local_value) {
    
    auto value = *local_value;
    
    std::cout << "undefined: " << value->IsUndefined() << std::endl;
    std::cout << "null: " << value->IsNull() << std::endl;
    std::cout << "true: " << value->IsTrue() << std::endl;
    std::cout << "false: " << value->IsFalse() << std::endl;
    std::cout << "name: " << value->IsName() << std::endl;
    std::cout << "string: " << value->IsString() << std::endl;
    std::cout << "symbol: " << value->IsSymbol() << std::endl;
    std::cout << "function: " << value->IsFunction() << std::endl;
    std::cout << "array: " << value->IsArray() << std::endl;
    std::cout << "object: " << value->IsObject() << std::endl;
    std::cout << "boolean: " << value->IsBoolean() << std::endl;
    std::cout << "number: " << value->IsNumber() << std::endl;
    std::cout << "external: " << value->IsExternal() << std::endl;
    std::cout << "isint32: " << value->IsInt32() << std::endl;
    std::cout << "isuint32: " << value->IsUint32() << std::endl;
    std::cout << "date: " << value->IsDate() << std::endl;
    std::cout << "argument object: " << value->IsArgumentsObject() << std::endl;
    std::cout << "boolean object: " << value->IsBooleanObject() << std::endl;
    std::cout << "number object: " << value->IsNumberObject() << std::endl;
    std::cout << "string object: " << value->IsStringObject() << std::endl;
    std::cout << "symbol object: " << value->IsSymbolObject() << std::endl;
    std::cout << "native error: " << value->IsNativeError() << std::endl;
    std::cout << "regexp: " << value->IsRegExp() << std::endl;
    std::cout << "generator function: " << value->IsGeneratorFunction() << std::endl;
    std::cout << "generator object: " << value->IsGeneratorObject() << std::endl;
}

std::set<std::string> make_set_from_object_keys(v8::Isolate * isolate,
                                                v8::Local<v8::Object> & object,
                                                bool own_properties_only)
{
    auto context = isolate->GetCurrentContext();
    v8::Local<v8::Array> properties;
    if (own_properties_only) {
        properties = object->GetOwnPropertyNames(context).ToLocalChecked();
    } else {
        properties = object->GetPropertyNames(context).ToLocalChecked();
    }
    auto array_length = get_array_length(isolate, properties);

    std::set<std::string> keys;

    for (int i = 0; i < array_length; i++) {
        keys.insert(*v8::String::Utf8Value(properties->Get(context, i).ToLocalChecked()));
    }

    return keys;
}




#define STRINGIFY_VALUE_DEBUG false

std::string stringify_value(v8::Isolate * isolate, const v8::Local<v8::Value> & value, bool top_level, bool show_all_properties)
{
    static std::vector<v8::Local<v8::Value>> processed_values;


    if (top_level) {
        processed_values.clear();
    };

    auto context = isolate->GetCurrentContext();

    std::stringstream output;

    // Only protect against cycles on container types - otherwise a numeric value with
    //   the same number won't get shown twice
    if (value->IsObject() || value->IsArray()) {
        for(auto processed_value : processed_values) {
            if(processed_value == value) {
                if (STRINGIFY_VALUE_DEBUG) print_v8_value_details(value);
                if (STRINGIFY_VALUE_DEBUG) printf("Skipping previously processed value\n");
                return "";
            }
        }
        processed_values.push_back(value);
    }


    if(value.IsEmpty()) {
        if (STRINGIFY_VALUE_DEBUG) printf("Value IsEmpty\n");
        return "Value specified as an empty v8::Local";
    }

    // if the left is a bool, return true if right is a bool and they match, otherwise false
    if (value->IsBoolean() || value->IsNumber() || value->IsFunction() || value->IsUndefined() || value->IsNull()) {
        if (STRINGIFY_VALUE_DEBUG) printf("Stringify: treating value as 'normal'\n");
        output << *v8::String::Utf8Value(value);
    } else if (value->IsString()) {
        output << "\"" << *v8::String::Utf8Value(value) << "\"";
    } else if (value->IsArray()) {
        // printf("Stringify: treating value as array\n");
        auto array = v8::Local<v8::Array>::Cast(value);
        auto array_length = get_array_length(isolate, array);

        output << "[";
        auto first_element = true;
        for (int i = 0; i < array_length; i++) {
            if (!first_element) {
                output << ", ";
            }
            first_element = false;
            auto value = array->Get(context, i);
            output << stringify_value(isolate, value.ToLocalChecked(), false, show_all_properties);
        }
        output << "]";
    } else {
        // printf("Stringify: treating value as object\n");
        // check this last in case it's some other type of more specialized object we will test the specialization instead (like an array)
        // objects must have all the same keys and each key must have the same as determined by calling this function on each value
        // printf("About to see if we can stringify this as an object\n");
        // print_v8_value_details(value);
        auto object = v8::Local<v8::Object>::Cast(value);
        if(value->IsObject() && !object.IsEmpty()) {
            if (STRINGIFY_VALUE_DEBUG) printf("Stringifying object\n");
            output << "{";
            auto keys = make_set_from_object_keys(isolate, object, !show_all_properties);
            auto first_key = true;
            for(auto key : keys) {
                if (STRINGIFY_VALUE_DEBUG) printf("Stringify: object key %s\n", key.c_str());
                if (!first_key) {
                    output << ", ";
                }
                first_key = false;
                output << key;
                output << ": ";
                auto value = object->Get(context, v8::String::NewFromUtf8(isolate, key.c_str()));
                output << stringify_value(isolate, value.ToLocalChecked(), false, show_all_properties);
            }
            output << "}";
        }
    }
    return output.str();
}


v8::Local<v8::Value> get_key(v8::Local<v8::Context> context, v8::Local<v8::Object> object, std::string key) {
    return get_key_as<v8::Value>(context, object, key);
}

v8::Local<v8::Value> get_key(v8::Local<v8::Context> context, v8::Local<v8::Value> value, std::string key) {
    return get_key_as<v8::Value>(context, get_value_as<v8::Object>(value), key);
}



v8::Local<v8::Value> call_simple_javascript_function(v8::Isolate * isolate,
						     v8::Local<v8::Function> function) {

    v8::Local<v8::Context> context(isolate->GetCurrentContext());
    v8::TryCatch tc(isolate);

    auto maybe_result = function->Call(context, context->Global(), 0, nullptr);
    if(tc.HasCaught() || maybe_result.IsEmpty()) {
	ReportException(isolate, &tc);
	printf("Error running javascript function: '%s'\n", *v8::String::Utf8Value(tc.Exception()));
	throw InvalidCallException(*v8::String::Utf8Value(tc.Exception()));
    }
    return maybe_result.ToLocalChecked();
}


// copied from shell_cc example
void ReportException(v8::Isolate* isolate, v8::TryCatch* try_catch) {
    v8::HandleScope handle_scope(isolate);
    v8::String::Utf8Value exception(try_catch->Exception());
    const char* exception_string = *exception;
    v8::Local<v8::Message> message = try_catch->Message();
    if (message.IsEmpty()) {
        // V8 didn't provide any extra information about this error; just
        // print the exception.
        fprintf(stderr, "%s\n", exception_string);
    } else {
        // Print (filename):(line number): (message).
        v8::String::Utf8Value filename(message->GetScriptOrigin().ResourceName());
        v8::Local<v8::Context> context(isolate->GetCurrentContext());
        const char* filename_string = *filename;
        int linenum = message->GetLineNumber(context).FromJust();
        fprintf(stderr, "%s:%i: %s\n", filename_string, linenum, exception_string);
        // Print line of source code.
        v8::String::Utf8Value sourceline(
                message->GetSourceLine(context).ToLocalChecked());
        const char* sourceline_string = *sourceline;
        fprintf(stderr, "%s\n", sourceline_string);
        // Print wavy underline (GetUnderline is deprecated).
        int start = message->GetStartColumn(context).FromJust();
        for (int i = 0; i < start; i++) {
            fprintf(stderr, " ");
        }
        int end = message->GetEndColumn(context).FromJust();
        for (int i = start; i < end; i++) {
            fprintf(stderr, "^");
        }
        fprintf(stderr, "\n");
        v8::Local<v8::Value> stack_trace_string;
        if (try_catch->StackTrace(context).ToLocal(&stack_trace_string) &&
            stack_trace_string->IsString() &&
            v8::Local<v8::String>::Cast(stack_trace_string)->Length() > 0) {
            v8::String::Utf8Value stack_trace(stack_trace_string);
            const char* stack_trace_string = *stack_trace;
            fprintf(stderr, "%s\n", stack_trace_string);
        }
    }
}

bool global_name_conflicts(const std::string & name) {
    if (std::find(reserved_global_names.begin(), reserved_global_names.end(), name) !=
        reserved_global_names.end()) {
        std::cerr << fmt::format("{} is a reserved js global name", name) << std::endl;
        return true;
    }
}

std::vector<std::string> reserved_global_names = {"Boolean", "Null", "Undefined", "Number", "String",
    "Object", "Symbol", "Date", "Array", "Set", "WeakSet",
    "Map", "WeakMap", "JSON"};





}
