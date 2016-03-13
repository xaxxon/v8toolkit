
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include <vector>
#include <iostream>
#include <utility>
#include <string>
#include <sstream>
#include <regex>
#include <mutex>
#include <set>
#include <map>
#include <vector>

#include <boost/format.hpp>

#include "./v8toolkit.h"


namespace v8toolkit {

void process_v8_flags(int & argc, char ** argv)
{
    v8::V8::SetFlagsFromCommandLine(&argc, argv, true);
}


void expose_gc()
{
    static const char * EXPOSE_GC = "--expose-gc";
    v8::V8::SetFlagsFromString(EXPOSE_GC, strlen(EXPOSE_GC));   
}


void add_variable(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template, const char * name, const v8::Local<v8::Data> template_to_attach) 
{
    object_template->Set(isolate, name, template_to_attach);
}


void add_variable(const v8::Local<v8::Context> context, const v8::Local<v8::Object> & object, const char * name, const v8::Local<v8::Value> value) 
{
    auto isolate = context->GetIsolate();
    (void)object->Set(context, v8::String::NewFromUtf8(isolate, name), value);
}


void add_function(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template, const char * name, void(*function)(const v8::FunctionCallbackInfo<v8::Value>&)) {
    object_template->Set(isolate, name, make_function_template(isolate, function));
}



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
std::string _printf_helper(const v8::FunctionCallbackInfo<v8::Value>& args, bool append_newline) {
    return _format_helper(args, append_newline);
}


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
std::string _print_helper(const v8::FunctionCallbackInfo<v8::Value>& args, bool append_newline) {
    std::stringstream sstream;
    auto values = get_all_values(args);
    int i = 0;
    for (auto value : values) {
        if (i > 0) {
            sstream << " ";
        }
        sstream << *v8::String::Utf8Value(value);
        i++;    
    }
    if (append_newline) {
        sstream << std::endl;
    }
    return sstream.str();
}


void add_print(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> object_template, std::function<void(const std::string &)> callback) {
    add_function(isolate, object_template, "printf",    [callback](const v8::FunctionCallbackInfo<v8::Value>& info){callback(_printf_helper(info, false));});
    add_function(isolate, object_template, "printfln",  [callback](const v8::FunctionCallbackInfo<v8::Value>& info){callback(_printf_helper(info, true));});
    add_function(isolate, object_template, "sprintf",  [](const v8::FunctionCallbackInfo<v8::Value>& info){return _format_helper(info, false);});
    
    add_function(isolate, object_template, "print",    [callback](const v8::FunctionCallbackInfo<v8::Value>& info){callback(_print_helper(info, false));});
    add_function(isolate, object_template, "println",  [callback](const v8::FunctionCallbackInfo<v8::Value>& info){callback(_print_helper(info, true));});

    add_function(isolate, object_template, "printobj", [callback](const v8::FunctionCallbackInfo<v8::Value>& info){
        auto isolate = info.GetIsolate();
        callback(stringify_value(isolate, info[0]) + "\n");
    });
    add_function(isolate, object_template, "printobjall", [callback](const v8::FunctionCallbackInfo<v8::Value>& info){
        auto isolate = info.GetIsolate();
        callback(stringify_value(isolate, info[0], true, true) + "\n");
    });
	
}

void add_assert(v8::Isolate * isolate,  v8::Local<v8::ObjectTemplate> object_template)
{
    add_function(isolate, object_template, "assert", [](const v8::FunctionCallbackInfo<v8::Value>& info) {
        auto isolate = info.GetIsolate();
        auto context = isolate->GetCurrentContext();
        if (V8_TOOLKIT_DEBUG) printf("Asserting: '%s'\n", *v8::String::Utf8Value(info[0]));

        v8::TryCatch tc(isolate);
        auto script_maybe = v8::Script::Compile(context, info[0]->ToString());
        assert(!tc.HasCaught());
        
        auto script = script_maybe.ToLocalChecked();
        auto result_maybe = script->Run(context);
        assert (!tc.HasCaught());
        
        auto result = result_maybe.ToLocalChecked();
        // print_v8_value_details(result);
        
        bool default_value = false;
        bool assert_result = result->BooleanValue(context).FromMaybe(default_value);
        assert(assert_result);
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


/**
* Takes in javascript source and attempts to compile it to a script.
* On error, it sets the output parameter `error` and returns `false`
*/
bool compile_source(v8::Local<v8::Context> & context, std::string source, v8::Local<v8::Script> & script, v8::Local<v8::Value> & error)
{
    auto isolate = context->GetIsolate();
    v8::TryCatch try_catch(isolate);
    auto source_maybe =
        v8::String::NewFromUtf8(isolate, source.c_str(),
                                v8::NewStringType::kNormal);
                                
    if (try_catch.HasCaught()) {
        error = try_catch.Exception();
        return false;
    }
    
    auto local_source = source_maybe.ToLocalChecked();
                                
    auto script_maybe = v8::Script::Compile(context, local_source);
    
    if (try_catch.HasCaught()) {
        // TODO: Is this the rignt thing to do?   Can this function be called from within a javascript context?  Maybe for assert()?
        error = try_catch.Exception();
        if (V8_TOOLKIT_DEBUG) printf("Failed to compile: %s\n", *v8::String::Utf8Value(try_catch.Exception()));
        return false;
    }
    
    script = script_maybe.ToLocalChecked();
    return true;
    
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
bool require(
    v8::Local<v8::Context> context,
    std::string filename,
    v8::Local<v8::Value> & result,
    const std::vector<std::string> & paths,
    bool track_file_modification_times, bool use_cache)
{
    auto isolate = context->GetIsolate();
    v8::Locker l(isolate);
    if (filename.find("..") != std::string::npos) {
        if (V8_TOOLKIT_DEBUG) printf("require() attempted to use a path with more than one . in a row '%s' (disallowed as simple algorithm to stop tricky paths)", filename.c_str());
        isolate->ThrowException(v8::String::NewFromUtf8(isolate, "Cannot specify a file containing .."));
        return false;
    }

    for (auto suffix : std::vector<std::string>{"", ".js", ".json", }) {
        for (auto path : paths) {
            try {
                auto complete_filename = path + "/" + filename + suffix;
        
                std::string file_contents;
                time_t file_modification_time = 0;
                if (!get_file_contents(complete_filename, file_contents, file_modification_time)) {
                    if (V8_TOOLKIT_DEBUG) printf("Module not found at %s\n", complete_filename.c_str());
                    continue;
                }
                
                // get the map of cached results for this isolate (guaranteed to exist because it was created before this lambda)
                // This is read-only and per-isolate and the only way to add is to be in the isolate
                if (use_cache) {
                    std::lock_guard<std::mutex> l(require_results_mutex);
                    auto & isolate_require_results = require_results[isolate];
        
                    auto cached_require_results = isolate_require_results.find(complete_filename);
                    if (cached_require_results != isolate_require_results.end()) {
                        if (V8_TOOLKIT_DEBUG) printf("Found cached results, using cache instead of re-running module\n");
                        
                        // if we don't care about file modifications or the file modification time is the same as before,
                        //   return the cached result
                        if (!track_file_modification_times || file_modification_time == cached_require_results->second.first) {
                            if (V8_TOOLKIT_DEBUG) printf("Returning cached results\n");
                            result = cached_require_results->second.second.Get(isolate);
                            return true;
                        } else {
                            if (V8_TOOLKIT_DEBUG) printf("Not returning cached results because modification time was no good\n");
                        }
                    } else {
                        if (V8_TOOLKIT_DEBUG) printf("Didn't find cached version for isolate %p %s\n", isolate, complete_filename.c_str());
                    }
                }

                // Compile the source code.
                v8::MaybeLocal<v8::Value> maybe_result;
                                            
                if (std::regex_search(filename, std::regex(".json$"))) {
                    v8::Local<v8::Script> script;
                    v8::Local<v8::Value> error;
                    v8::TryCatch try_catch(isolate);
                    // TODO: make sure requiring a json file is being tested
                    if (V8_TOOLKIT_DEBUG) printf("About to try to parse json: %s\n", file_contents.c_str());
                    maybe_result = v8::JSON::Parse(isolate, v8::String::NewFromUtf8(isolate,file_contents.c_str()));
                    if (try_catch.HasCaught()) {
                        try_catch.ReThrow();
                        if (V8_TOOLKIT_DEBUG) printf("Couldn't run json for %s, error: %s\n", complete_filename.c_str(), *v8::String::Utf8Value(try_catch.Exception()));
                        return false;
                    }
                } else {
                    v8::Local<v8::Script> script;
                    v8::Local<v8::Value> error;
                    if (!compile_source(context, file_contents, script, error)) {
                        isolate->ThrowException(error);
                        if (V8_TOOLKIT_DEBUG) printf("Couldn't compile .js for %s\n", complete_filename.c_str());
                        return false;
                    }
                        
                    v8::TryCatch try_catch(isolate);
                    
                    

                    // set up the module and exports stuff
                    auto module_object = v8::Object::New(isolate);
                    // TODO: module object should also have "children" array, "filename" string, "id" string, "loaded" boolean, "parent" module object that first loaded this one
                    auto exports_object = v8::Object::New(isolate);
                    add_variable(context, module_object, "exports", exports_object);
                    add_variable(context, context->Global(), "module", module_object);
                    add_variable(context, context->Global(), "exports", exports_object);
                    (void)context->Global()->Set(context, v8::String::NewFromUtf8(isolate, "global"), context->Global());

                    
                    // return value of run doesn't matter, only what is hooked up to export variable
                    (void)script->Run(context);

                    maybe_result = context->Global()->Get(context, v8::String::NewFromUtf8(isolate, "exports"));
                    context->Global()->Delete(context, v8::String::NewFromUtf8(isolate, "module"));
                    context->Global()->Delete(context, v8::String::NewFromUtf8(isolate, "exports"));
                    
                    if (try_catch.HasCaught()) {
                        try_catch.ReThrow();
                        if (V8_TOOLKIT_DEBUG) printf("Couldn't run .js for %s\n", complete_filename.c_str());
                        return false;
                    }
                    
                }

                result = maybe_result.ToLocalChecked();
                
                // cache the result for subsequent requires of the same module in the same isolate
                if (use_cache) {
                    std::lock_guard<std::mutex> l(require_results_mutex);
                    auto new_global = new v8::Global<v8::Value>(isolate, result);
                    auto & isolate_require_results = require_results[isolate];
                    isolate_require_results.insert(std::pair<std::string, cached_module_t>(complete_filename, cached_module_t(file_modification_time, *new_global)));
                }
                // printf("Require final result: %s\n", stringify_value(isolate, result).c_str());
                // printf("Require returning resulting object for module %s\n", complete_filename.c_str());
                return true;
            }catch(...) {}
            // if any failures, try the next path if it exists
        }
    }
    // printf("Couldn't find any matches for %s\n", filename.c_str());
    isolate->ThrowException(v8::String::NewFromUtf8(isolate, "No such module found in any search path"));
    return false;

}


void add_module_list(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template)
{
    add_function(isolate, object_template, "module_list", 
        [isolate]{return scoped_run(isolate,[isolate]{return mapper(require_results[isolate],[](auto module_info){
            // don't return the modification time
            return std::pair<std::string, v8::Global<v8::Value>&>(module_info.first, module_info.second.second);
        });});}
    );
}

void add_require(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template, const std::vector<std::string> & paths) {
    
    // if there's no entry for cached results for this isolate, make one now
    {
        std::lock_guard<std::mutex> guard(require_results_mutex);
        if (require_results.find(isolate) == require_results.end()) {
            require_results.insert(make_pair(isolate, cached_isolate_modules_t()));
        }
    }

    (void)add_function(isolate, object_template, "require", 
        [isolate, paths](std::string filename) {
            auto context = isolate->GetCurrentContext();
            v8::Local<v8::Value> result; 
            
            // if require returns false, it will throw a javascript exception
            //   so it doesn't matter if the result sent back is good
            if(require(context, filename, result, paths)) {
                if (V8_TOOLKIT_DEBUG) printf("Require returning to caller: '%s'\n", stringify_value(isolate, result).c_str());
            }
            return result;
        });
}


// to find the directory of the executable, you could use argv[0], but here are better results:
//  http://stackoverflow.com/questions/1023306/finding-current-executables-path-without-proc-self-exe/1024937#1024937
// including these as helpers in this library would probably be useful
void require_directory(v8::Local<v8::Context> context, std::string directory_name)
{   
    // This probably works on more than just APPLE
#ifdef __APPLE__
    auto full_directory_name = directory_name;
    DIR * dir = opendir(full_directory_name.c_str());
    if (dir == NULL) {
        if (V8_TOOLKIT_DEBUG) printf("Could not open directory %s\n", full_directory_name.c_str());
        return;
    }
    struct dirent * dp;
    
    auto require_path = std::vector<std::string>{directory_name};
    while ((dp = readdir(dir)) != NULL) {
        if (dp->d_type == DT_DIR) {
            // printf("Skipping %s because it's a directory\n", dp->d_name);
            continue;
        }
        if (V8_TOOLKIT_DEBUG) printf("reading directory, got %s\n", dp->d_name);
        v8::Local<v8::Value> result;
        require(context, dp->d_name, result, require_path);
    }
    if (V8_TOOLKIT_DEBUG) printf("Done reading directory\n");
    (void)closedir(dir);
    return;
    
#endif // __APPLE__
    
    assert(false);
    
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

std::set<std::string> make_set_from_object_keys(v8::Isolate * isolate, v8::Local<v8::Object> & object, bool own_properties_only = true) 
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


void dump_prototypes(v8::Isolate * isolate, v8::Local<v8::Object> object)
{
	printf("Looking at prototype chain\n");
	while (!object->IsNull()) {
	    printf("%s:\n", *v8::String::Utf8Value(object));
	    // print_v8_value_details(foo);
	    printf("%s\n", stringify_value(isolate, object).c_str());
	    object = v8::Local<v8::Object>::Cast(object->GetPrototype());
	}
	printf("Done looking at prototype chain\n");
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



/*
* "interesting" means things not a part of Object - or at least pretty close to that
*/
std::vector<std::string> get_interesting_properties(v8::Local<v8::Context> context, v8::Local<v8::Object> object)
{
    auto isolate = context->GetIsolate();
    auto names = object->GetPropertyNames(context).ToLocalChecked();
    return CastToNative<std::vector<std::string>>()(isolate, names);
}






}  // namespace v8toolkit




