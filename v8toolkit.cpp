#include <vector>
#include <iostream>
#include <utility>
#include <string>
#include <sstream>
#include <regex>
#include <mutex>
#include <set>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

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

std::string _format_helper(const v8::FunctionCallbackInfo<v8::Value>& args, bool append_newline)
{
    std::stringstream sstream;
    auto values = get_all_values(args);
    
    if (args.Length() > 0) {
        auto format = boost::format(*v8::String::Utf8Value(values[0]));

        unsigned int i;
        for (i = 1; format.remaining_args() > 0; i++) {
            if (i < values.size()) {
                format % *v8::String::Utf8Value(values[i]);
            } else {
                format % "";
            }
        }
        sstream << format;
        while (i < values.size()) {
            sstream << " " << *v8::String::Utf8Value(values[i]);
            i++;
        }
    }
    if (append_newline) {
        sstream << std::endl;
    }
    return sstream.str();
}



// takes a format string and some javascript objects and does a printf-style print using boost::format
// fills missing parameters with empty strings and prints any extra parameters with spaces between them
void _printf_helper(const v8::FunctionCallbackInfo<v8::Value>& args, bool append_newline) {
    std::cout << _format_helper(args, append_newline);
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



void printobj(v8::Local<v8::Context> context, v8::Local<v8::Object> object)
{
    if(object->InternalFieldCount() > 0) {
        v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(object->GetInternalField(0));
        printf(">>> Object %p: %s\n", wrap->Value(), *v8::String::Utf8Value(object));
    } else {
        printf(">>> Object does not appear to be a wrapped c++ class (no internal fields): %s\n", *v8::String::Utf8Value(object));
    }
    
    printf("Object has the following own properties\n");
    for_each_own_property(context, object, [](v8::Local<v8::Value> name, v8::Local<v8::Value> value){
        printf(">>> %s: %s\n", *v8::String::Utf8Value(name), *v8::String::Utf8Value(value));
    });
    printf("End of object's own properties\n");
    
}


void printobj_callback(const v8::FunctionCallbackInfo<v8::Value>& args) {
    auto isolate = args.GetIsolate();
    auto context = isolate->GetCurrentContext();
    for (int i = 0; i < args.Length(); i++) {
        auto object = args[i]->ToObject(context).ToLocalChecked();
        printobj(context, object);
    }
}

void add_print(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> object_template ) {
#ifdef USE_BOOST
    add_function(isolate, object_template, "printf",    [](const v8::FunctionCallbackInfo<v8::Value>& info){_printf_helper(info, false);});
    add_function(isolate, object_template, "printfln",  [](const v8::FunctionCallbackInfo<v8::Value>& info){_printf_helper(info, true);});
    add_function(isolate, object_template, "sprintf",  [](const v8::FunctionCallbackInfo<v8::Value>& info){return _format_helper(info, false);});
    
#endif
    add_function(isolate, object_template, "print",    [](const v8::FunctionCallbackInfo<v8::Value>& info){_print_helper(info, false);});
    add_function(isolate, object_template, "println",  [](const v8::FunctionCallbackInfo<v8::Value>& info){_print_helper(info, true);});

    add_function(isolate, object_template, "printobj", [](const v8::FunctionCallbackInfo<v8::Value>& info){
        auto isolate = info.GetIsolate();
        printf("%s", stringify_value(isolate, info[0]).c_str());
    });
}




bool get_file_contents(std::string filename, std::string & file_contents) 
{
    time_t ignored_time;
    return get_file_contents(filename, file_contents, ignored_time);
}


bool get_file_contents(std::string filename, std::string & file_contents, time_t & file_modification_time)
{
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1) {
        return false;
    }
    struct stat file_stat;
    if (fstat(fd, &file_stat) == -1) {
        return false;
    }

    file_modification_time = file_stat.st_mtime;
    auto file_size = file_stat.st_size;
    
    file_contents.resize(file_size);
    
    auto bytes_remaining = file_size;
    // TODO: Need to handle file not having as many bytes as expected with open returning 0 before bytes_remaining == 0
    while (bytes_remaining > 0) {
        auto bytes_read = read(fd, &file_contents[file_size - bytes_remaining], bytes_remaining);
        bytes_remaining -= bytes_read;
    }
    
    return true;
    
}

bool _get_modification_time_of_filename(std::string filename, time_t & modification_time)
{
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1) {
        return false;
    }
    struct stat file_stat;
    if (fstat(fd, &file_stat) == -1) {
        return false;
    }
    
    return file_stat.st_mtime;
}



// once a module has been successfully required (in an isolate), its return value will be cached here
//   and subsequent requires of the same module won't re-run the module, just return the
//   cached value
std::mutex require_results_mutex;
typedef std::pair<time_t, v8::Global<v8::Value>&> cached_module_t; // a single module result with modified time
typedef std::map<std::string, cached_module_t> cached_isolate_modules_t; // a named module result
static std::map<v8::Isolate *, cached_isolate_modules_t> require_results;


cached_isolate_modules_t get_loaded_modules(v8::Isolate * isolate)
{
    std::lock_guard<std::mutex> l(require_results_mutex);
    return require_results[isolate];
}



/** Attempts to load the specified external resource.  
* Attempts to load exact match filename in each path, then <filename>.js in each path, then <filename>.json in each path
*
* Paths are attempted in vector order[0, 1, 2...].  If the matching filename ends in .js (either because it was specified as such or because
*   it matched after the suffix was added) it will be executed as a traditional module with the exports object being returned.  If the matching
*   filename ends in .json, the last value in the file will be returned.
* The results of require'd files is cached and if the same module (based on the full matching value including path and any added suffix) is
*   required again, the cached value will be returned.  The module will not be re-run.
*
* The goal of this function is to be as close to node.js require as possible, so patches or descriptions of how it differs are appreciated.
*   Not that much time was spent trying to determine the exact behavior, so there are likely significant differences
*/
v8::Local<v8::Value> require(v8::Isolate * isolate, 
    v8::Local<v8::Context> & context, 
    std::string filename, 
    const std::vector<std::string> & paths,
    bool track_file_modification_times)
{

    v8::Locker l(isolate);
    if (filename.find("..") != std::string::npos) {
        printf("require() attempted to use a path with more than one . in a row '%s' (disallowed as simple algorithm to stop tricky paths)", filename.c_str());
        return v8::Object::New(isolate);
    }

    for (auto suffix : std::vector<std::string>{"", ".js", ".json", }) {
        for (auto path : paths) {
            try {
                auto complete_filename = path + "/" + filename + suffix;
        
                std::string file_contents;
                time_t file_modification_time = 0;
                if (!get_file_contents(complete_filename, file_contents, file_modification_time)) {
                    printf("Module not found at %s\n", complete_filename.c_str());
                    continue;
                }
                
                // get the map of cached results for this isolate (guaranteed to exist because it was created before this lambda)
                // This is read-only and per-isolate and the only way to add is to be in the isolate
                {
                    std::lock_guard<std::mutex> l(require_results_mutex);
                    auto & isolate_require_results = require_results[isolate];
        
                    auto cached_require_results = isolate_require_results.find(complete_filename);
                    if (cached_require_results != isolate_require_results.end()) {
                        printf("Found cached results, using cache instead of re-running module\n");
                        
                        // if we don't care about file modifications or the file modification time is the same as before,
                        //   return the cached result
                        if (!track_file_modification_times || file_modification_time == cached_require_results->second.first) {
                            return cached_require_results->second.second.Get(isolate);
                        }
                    } else {
                        printf("Didn't find cached version %p %s\n", isolate, complete_filename.c_str());
                    }
                }

    
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
                    v8::String::NewFromUtf8(isolate, file_contents.c_str(),
                                            v8::NewStringType::kNormal).ToLocalChecked();


                // Compile the source code.
                v8::Local<v8::Script> script = v8::Script::Compile(module_context, source).ToLocalChecked();
                v8::MaybeLocal<v8::Value> maybe_result;
                if (std::regex_match(filename, std::regex(".json$"))) {
                    printf("About to start evaluate .json file\n");
                    maybe_result = script->Run(context);
                
                } else {

                    printf("About to start running script\n");
                    (void)script->Run(context);
    
                    maybe_result = module_object->Get(module_context, v8::String::NewFromUtf8(isolate, "exports"));
                }
                
                // cache the result for subsequent requires of the same module in the same isolate
                auto result = maybe_result.ToLocalChecked();
                auto new_global = new v8::Global<v8::Value>(isolate, result);
                {
                    std::lock_guard<std::mutex> l(require_results_mutex);
                    auto & isolate_require_results = require_results[isolate];
                    isolate_require_results.insert(std::pair<std::string, cached_module_t>(complete_filename, cached_module_t(file_modification_time, *new_global)));
                }

                return result;
            }catch(...) {}
            // if any failures, try the next path if it exists
        }
    }
    return v8::Object::New(isolate);
}



void add_module_list(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template)
{
    std::lock_guard<std::mutex> l(require_results_mutex);
    add_function(isolate, object_template, "module_list", [isolate]{return scoped_run(isolate,[isolate]{return require_results[isolate];});});
}

void add_require(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template, const std::vector<std::string> & paths) {
    
    static bool require_added = false;
    if (require_added) {
        printf("Require already added, not doing anything\n");
    }
    require_added = true;
    
    
    // if there's no entry for cached results for this isolate, make one now
    {
        v8::Locker l(isolate);
        if (require_results.find(isolate) == require_results.end()) {
            require_results.insert(make_pair(isolate, cached_isolate_modules_t()));
        }
    }

    (void)add_function(isolate, object_template, "require", [paths](const v8::FunctionCallbackInfo<v8::Value> & info, std::string filename)->v8::Local<v8::Value>{
        auto isolate = info.GetIsolate();
        auto context = isolate->GetCurrentContext();
        
        return require(isolate, context, filename, paths);

    });
}


void require_directory(v8::Isolate * isolate, v8::Local<v8::Context> context, std::string directory_name)
{
    
// #include <boost/filesystem.hpp>
    //
    // boost::filesystem::path p = boost::filesystem::current_path();
    // boost::filesystem::directory_iterator it{p};
    // while (it != boost::filesystem::directory_iterator{})
    //   std::cout << *it++ << '\n';
    //

    // This probably works on more than just APPLE
#ifdef __APPLE__
    DIR * dir = opendir(".");
    if (dir == NULL)
            return;
    struct dirent * dp;
    
    auto require_path = std::vector<std::string>{directory_name};
    while ((dp = readdir(dir)) != NULL) {
        
        require(isolate, context, dp->d_name, require_path);
            // if (dp->d_namlen == len && strcmp(dp->d_name, name) == 0) {
            //         (void)closedir(dir);
            //         return (FOUND);
            // }
    }
    (void)closedir(dir);
    return;
    
#endif // __APPLE__
    
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

int get_array_length(v8::Isolate * isolate, v8::Local<v8::Array> array) {
    auto context = isolate->GetCurrentContext();
    return array->Get(context, v8::String::NewFromUtf8(isolate, "length")).ToLocalChecked()->Uint32Value(); 
}

std::set<std::string> make_set_from_object_keys(v8::Isolate * isolate, v8::Local<v8::Object> & object) 
{
    auto context = isolate->GetCurrentContext();
    auto properties = object->GetOwnPropertyNames(context).ToLocalChecked();
    auto array_length = get_array_length(isolate, properties);
    
    std::set<std::string> keys;
    
    for (int i = 0; i < array_length; i++) {
        keys.insert(*v8::String::Utf8Value(properties->Get(context, i).ToLocalChecked()));
    }

    return keys;
}



bool compare_contents(v8::Isolate * isolate, const v8::Local<v8::Value> & left, const v8::Local<v8::Value> & right)
{
    // printf("Comparing two things\n");
    auto context = isolate->GetCurrentContext();
    
    v8::Local<v8::Boolean> bool_left;
    v8::Local<v8::Boolean> bool_right;
    
    // if the left is a bool, return true if right is a bool and they match, otherwise false
    if (left->IsBoolean()) {
        // printf("Checking two booleans\n");
        return  right->IsBoolean() && left->ToBoolean(context).ToLocalChecked()->Value() == right->ToBoolean(context).ToLocalChecked()->Value();
    }

    if (left->IsNumber()) {
        // printf("Checking two numbers\n");
        // auto ln = left->ToNumber(context).ToLocalChecked()->Value();
        // auto rn = right->ToNumber(context).ToLocalChecked()->Value();
        // printf("Comparing %f and %f\n", ln, rn);
        return  right->IsNumber() && left->ToNumber(context).ToLocalChecked()->Value() == right->ToNumber(context).ToLocalChecked()->Value();
    }
    
    if (left->IsString()) {
        // printf("Checking two strings\n");
        if (!right->IsString()) {
            return false;
        }
        return !strcmp(*v8::String::Utf8Value(left), *v8::String::Utf8Value(right));            
    }

    if (left->IsArray()) {
        // printf("Checking two arrays\n");
        if (!right->IsArray()) {
            return false;
        }
        auto array_left = v8::Local<v8::Array>::Cast(left);
        auto array_right = v8::Local<v8::Array>::Cast(right);
        
        auto left_length = get_array_length(isolate, array_left);
        auto right_length = get_array_length(isolate, array_right);
        
        if (left_length != right_length) {
            // printf("Array lengths differ %d %d\n", (int)left_length, (int) right_length);
            return false;
        }
        
        for (int i = 0; i < left_length; i++) {
            auto left_value = array_left->Get(context, i);
            auto right_value = array_right->Get(context, i);
            if (!compare_contents(isolate, left_value.ToLocalChecked(), right_value.ToLocalChecked())) {
                return false;
            }
        }
        return true;
    }


    // check this last in case it's some other type of more specialized object we will test the specialization instead (like an array)
    // objects must have all the same keys and each key must have the same as determined by calling this function on each value
    auto object_left = v8::Local<v8::Object>::Cast(left);
    if(!object_left.IsEmpty()) {
        // printf("Checking two arrays\n");
        
        auto object_right = v8::Local<v8::Object>::Cast(right);
        
        // if they're not both objects, return false
        if (object_right.IsEmpty()) {
            // printf("right value not object\n");
            return false;
        }
        auto left_keys = make_set_from_object_keys(isolate, object_left);
        auto right_keys = make_set_from_object_keys(isolate, object_right);
        if (left_keys.size() != right_keys.size()) {
            // printf("key count mismatch: %d %d\n", (int)left_keys.size(), (int)right_keys.size());
            return false;
        }

        for(auto left_key : left_keys) {
            auto left_value = object_left->Get(context, v8::String::NewFromUtf8(isolate, left_key.c_str()));
            auto right_value = object_right->Get(context, v8::String::NewFromUtf8(isolate, left_key.c_str()));
            if (right_value.IsEmpty()) {
                // printf("right side doesn't have key: %s\n", left_key.c_str());
                return false;
            } else if (!compare_contents(isolate, left_value.ToLocalChecked(), right_value.ToLocalChecked())) {
                // printf("Recursive check of value in both objects returned false for key %s\n", left_key.c_str());
                return false;
            }
        }
        
        return true;
    }
    // printf("Returning false because left value is of unknown/unhandled type\n");
    
    return false;    
}









AnyBase::~AnyBase() {}







std::string stringify_value(v8::Isolate * isolate, const v8::Local<v8::Value> & value, std::string indentation)
{
    // printf("Comparing two things\n");
    auto context = isolate->GetCurrentContext();
    
    std::string output = indentation;
    
    // if the left is a bool, return true if right is a bool and they match, otherwise false
    if (value->IsBoolean() || value->IsNumber() || value->IsString()) {
        output += *v8::String::Utf8Value(value);
    } else if (value->IsArray()) {
        // printf("Stringifying array\n");
        auto array = v8::Local<v8::Array>::Cast(value);
        auto array_length = get_array_length(isolate, array);
        
        output += "[";
        auto first_element = true;
        for (int i = 0; i < array_length; i++) {
            if (!first_element) {
                output += ", ";
            }
            first_element = false;
            auto value = array->Get(context, i);
            output += stringify_value(isolate, value.ToLocalChecked(), indentation);
        }
        
        output += "]";
    } else {
        // check this last in case it's some other type of more specialized object we will test the specialization instead (like an array)
        // objects must have all the same keys and each key must have the same as determined by calling this function on each value
        auto object = v8::Local<v8::Object>::Cast(value);
        if(!object.IsEmpty()) {
            // printf("Stringifying object\n");
            output += "{";
            auto keys = make_set_from_object_keys(isolate, object);
            auto first_key = true;
            for(auto key : keys) {
                if (!first_key) {
                    output += ", ";
                }
                first_key = false;
                output += key;
                output += ": ";
                auto value = object->Get(context, v8::String::NewFromUtf8(isolate, key.c_str()));
                output += stringify_value(isolate, value.ToLocalChecked(), indentation);
            }
            output += "}";
        }
    }    
    return output;
}



}  // namespace v8toolkit




