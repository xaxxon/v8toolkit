#include <vector>
#include <iostream>
#include <utility>
#include <string>
//
//
// #include "include/libplatform/libplatform.h"
// #include "include/v8.h"
// #include "casts.hpp"

#include <map>

#include "./v8toolkit.h"

namespace v8toolkit {

void set_global_object_alias(v8::Isolate * isolate, const v8::Local<v8::Context> context, std::string alias_name)
{
    auto global_object = context->Global();
    (void)global_object->Set(context, v8::String::NewFromUtf8(isolate, alias_name.c_str()), global_object);
    
}

void process_v8_flags(int & argc, char ** argv)
{
    v8::V8::SetFlagsFromCommandLine(&argc, argv, true);
}


void expose_gc()
{
    static const char * EXPOSE_GC = "--expose-gc";
    v8::V8::SetFlagsFromString(EXPOSE_GC, strlen(EXPOSE_GC));   
}


void add_variable(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template, const char * name, const v8::Local<v8::Value> value) 
{
    object_template->Set(isolate, name, value);
}

void add_variable(const v8::Local<v8::Context> context, const v8::Local<v8::Object> & object, const char * name, const v8::Local<v8::Value> value) 
{
    auto isolate = context->GetIsolate();
    (void)object->Set(context, v8::String::NewFromUtf8(isolate, name), value);
}

void add_function(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template, const char * name, void(*function)(const v8::FunctionCallbackInfo<v8::Value>&)) {
    object_template->Set(isolate, name, make_function_template(isolate, function));
}


#ifdef USE_BOOST


// takes a format string and some javascript objects and does a printf-style print using boost::format
// fills missing parameters with empty strings and prints any extra parameters with spaces between them
void _printf_helper(const v8::FunctionCallbackInfo<v8::Value>& args, bool append_newline) {
    auto values = get_all_values(args);
    
    if (args.Length() > 0) {
        auto string = *v8::String::Utf8Value(values[0]);
        auto format = boost::format(string);

        int i;
        for (i = 1; format.remaining_args() > 0; i++) {
            if (i < values.size()) {
                format % *v8::String::Utf8Value(values[i]);
            } else {
                format % "";
            }
        }
        std::cout << format;
        while (i < values.size()) {
            std::cout << " " << *v8::String::Utf8Value(values[i]);
            i++;
        }
    }
    if (append_newline) {
        std::cout << std::endl;
    }
}

#endif // USE_BOOST

// Returns the values in a FunctionCallbackInfo object breaking out first-level arrays into their
//   contained values (but not subsequent arrays for no particular reason)
std::vector<v8::Local<v8::Value>> get_all_values(const v8::FunctionCallbackInfo<v8::Value>& args, int depth) {
    std::vector<v8::Local<v8::Value>> values;
    
    auto isolate = args.GetIsolate();
    auto context = isolate->GetCurrentContext();
    
    for (int i = 0; i < args.Length(); i++) {
        if (args[i]->IsArray()) {
            auto array = v8::Object::Cast(*args[i]);
            int i = 0;
            while(array->Has(context, i).FromMaybe(false)) {
                values.push_back(array->Get(context, i).ToLocalChecked());
                i++;
            }
        } else {
            values.push_back(args[i]);
        }
    }
    return values;
}



// prints out arguments with a space between them
void _print_helper(const v8::FunctionCallbackInfo<v8::Value>& args, bool append_newline) {

    auto values = get_all_values(args);
    int i = 0;
    for (auto value : values) {
        if (i > 0) {
            std::cout << " ";
        }
        std::cout << *v8::String::Utf8Value(value);
        i++;    
    }
    if (append_newline) {
        std::cout << std::endl;
    }

}


void printobj(const v8::FunctionCallbackInfo<v8::Value>& args) {
    auto isolate = args.GetIsolate();
    auto context = isolate->GetCurrentContext();
    for (int i = 0; i < args.Length(); i++) {
        auto object = args[i]->ToObject(context).ToLocalChecked();
        if(object->InternalFieldCount() > 0) {
            v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(object->GetInternalField(0));
            printf(">>> Object %p: %s\n", wrap->Value(), *v8::String::Utf8Value(args[i]));
        } else {
            printf(">>> Object does not appear to be a wrapped c++ class (no internal fields): %s\n", *v8::String::Utf8Value(args[i]));
        }
        
        printf("Object has the following own properties\n");
        for_each_own_property(context, object, [](v8::Local<v8::Value> name, v8::Local<v8::Value> value){
            printf(">>> %s: %s\n", *v8::String::Utf8Value(name), *v8::String::Utf8Value(value));
        });
        printf("End of object's own properties\n");
    }
}

void add_print(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> object_template ) {
#ifdef USE_BOOST
    add_function(isolate, object_template, "printf",    [](const v8::FunctionCallbackInfo<v8::Value>& args){_printf_helper(args, false);});
    add_function(isolate, object_template, "printfln",  [](const v8::FunctionCallbackInfo<v8::Value>& args){_printf_helper(args, true);});
#endif
    add_function(isolate, object_template, "print",    [](const v8::FunctionCallbackInfo<v8::Value>& args){_print_helper(args, false);});
    add_function(isolate, object_template, "println",  [](const v8::FunctionCallbackInfo<v8::Value>& args){_print_helper(args, true);});

    add_function(isolate, object_template, "printobj", [](const v8::FunctionCallbackInfo<v8::Value>& args){printobj(args);});
}




// helper for testing code, not a part of the library
// read the contents of the file and return it as a std::string
std::string get_file_contents(const char *filename)
{
  std::ifstream in(filename, std::ios::in | std::ios::binary);
  if (in) {
    std::string contents;
    in.seekg(0, std::ios::end);
    contents.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(&contents[0], contents.size());
    in.close();
    return(contents);
  }
  throw(errno);
}



void add_require(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template, const std::vector<std::string> & paths) {
    // once a module has been successfully required (in an isolate), its return value will be cached here
    //   and subsequent requires of the same module won't re-run the module, just return the
    //   cached value
    static std::map<v8::Isolate *, std::map<std::string, v8::Global<v8::Value>&>> require_results;
    
    static bool require_added = false;
    if (require_added) {
        printf("Require already added, not doing anything\n");
    }
    require_added = true;
    
    
    // if there's no entry for cached results for this isolate, make one now
    if (require_results.find(isolate) == require_results.end()) {
        require_results.insert(make_pair(isolate, std::map<std::string, v8::Global<v8::Value>&>()));
    }
    
    
    (void)add_function(isolate, object_template, "require", [paths](const v8::FunctionCallbackInfo<v8::Value> & info, std::string filename)->v8::Local<v8::Value>{        
        auto isolate = info.GetIsolate();
        auto context = isolate->GetCurrentContext();
        
        printf("require got callback info object with length set to: %d\n", info.Length());
        if (filename.find_first_of("..") == std::string::npos) {
            printf("require() attempted to use a path with more than one . in a row (disallowed as simple algorithm to stop tricky paths)");
            return v8::Object::New(isolate);
        }
        
        for (auto path : paths) {
            try {
                auto complete_filename = path + filename;
                
                // if the file doesn't exist, move on to the next path
                std::ifstream in(complete_filename);
                if (!in) {
                    printf("Module not found at %s\n", complete_filename.c_str());
                    continue;
                }
                in.close();
                
                
                // get the map of cached results for this isolate (guaranteed to exist because it was created before this lambda)
                auto & isolate_require_results = require_results[isolate];
                
                auto cached_require_results = isolate_require_results.find(complete_filename);
                if (cached_require_results != isolate_require_results.end()) {
                    printf("Found cached results, using cache instead of re-running module\n");
                    return cached_require_results->second.Get(isolate);
                } else {
                    printf("Didn't find cached version %p %s\n", isolate, complete_filename.c_str());
                }

                auto contents = get_file_contents(complete_filename.c_str());
                
                    
                // create a new context for it (this may be the wrong thing to do)
                auto module_global_template = v8::ObjectTemplate::New(isolate);     
                add_print(isolate, module_global_template);

                // Create module context
                auto module_context = v8::Context::New(isolate, nullptr, module_global_template);

                // Get module global object
                auto module_global_object = module_context->Global();

                // set up the module and exports stuff
                auto module_object = v8::Object::New(isolate);
                auto exports_object = v8::Object::New(isolate);
                add_variable(module_context, module_object, "exports", exports_object);
                add_variable(module_context, module_global_object, "module", module_object);
                add_variable(module_context, module_global_object, "exports", exports_object);
                (void)module_global_object->Set(module_context, v8::String::NewFromUtf8(isolate, "global"), module_global_object);

                v8::Local<v8::String> source =
                    v8::String::NewFromUtf8(isolate, contents.c_str(),
                                            v8::NewStringType::kNormal).ToLocalChecked();


                // Compile the source code.
                v8::Local<v8::Script> script = v8::Script::Compile(module_context, source).ToLocalChecked();

                printf("About to start running script\n");
                (void)script->Run(context);
                
                auto result = module_object->Get(module_context, v8::String::NewFromUtf8(isolate, "exports")).ToLocalChecked();

                // cache the result for subsequent requires of the same module in the same isolate
                auto new_global = new v8::Global<v8::Value>(isolate, result);
                isolate_require_results.insert(std::pair<std::string,
                                               v8::Global<v8::Value>&>(complete_filename, *new_global));

                return result;
            }catch(...) {}
            // if any failures, try the next path if it exists
        }
        return v8::Object::New(isolate);
    });
}




}  // namespace v8toolkit




