
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
#include <map>
#include <fmt/format.h>
#include <boost/format.hpp>

#include "v8toolkit/v8_class_wrapper.h"


namespace v8toolkit {



// used in v8_class_wrapper.h to store callbacks for cleaning up wrapper objects when an isolate is destroyed
V8ClassWrapperInstanceRegistry wrapper_registery;

using namespace ::v8toolkit::literals;

void process_v8_flags(int & argc, char ** argv)
{
    v8::V8::SetFlagsFromCommandLine(&argc, argv, true);
}

    
void expose_gc()
{
    static const char * EXPOSE_GC = "--expose-gc";
    v8::V8::SetFlagsFromString(EXPOSE_GC, strlen(EXPOSE_GC));
}


InvalidCallException::InvalidCallException(const std::string & message)
{
    if (v8::Isolate::GetCurrent() != nullptr) {
        this->message = message + get_stack_trace_string(v8::StackTrace::CurrentStackTrace(v8::Isolate::GetCurrent(), 100));
    } else {
        this->message = message;
    }
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
    object_template->Set(isolate, name, make_function_template(isolate, function, name));
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
//   contained values (but not subsequent arrays)
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



void add_print(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> object_template, func::function<void(const std::string &)> callback) {
    add_function(isolate, object_template, "printf",    [callback](const v8::FunctionCallbackInfo<v8::Value>& info){callback(_printf_helper(info, false));});
    add_function(isolate, object_template, "printfln",  [callback](const v8::FunctionCallbackInfo<v8::Value>& info){callback(_printf_helper(info, true));});
    add_function(isolate, object_template, "sprintf",  [](const v8::FunctionCallbackInfo<v8::Value>& info){return _format_helper(info, false);});
    
    add_function(isolate, object_template, "print",    [callback](const v8::FunctionCallbackInfo<v8::Value>& info){callback(_print_helper(info, false));});
    add_function(isolate, object_template, "println",  [callback](const v8::FunctionCallbackInfo<v8::Value>& info){callback(_print_helper(info, true));});

    add_function(isolate, object_template, "printobj", [callback](const v8::FunctionCallbackInfo<v8::Value>& info){
        auto isolate = info.GetIsolate();
        for (int i = 0; i < info.Length(); i++) {
            callback(stringify_value(isolate, info[i]) + "\n");
        }
    });
    add_function(isolate, object_template, "printobjall", [callback](const v8::FunctionCallbackInfo<v8::Value>& info){
        auto isolate = info.GetIsolate();
        for (int i = 0; i < info.Length(); i++) {
            callback(stringify_value(isolate, info[i], true) + "\n");
        }
    });
	
}

void add_print(const v8::Local<v8::Context> context, func::function<void(const std::string &)> callback) {
    add_function(context, context->Global(), "printf",    [callback](const v8::FunctionCallbackInfo<v8::Value>& info){callback(_printf_helper(info, false));});
    add_function(context, context->Global(), "printfln",  [callback](const v8::FunctionCallbackInfo<v8::Value>& info){callback(_printf_helper(info, true));});
    add_function(context, context->Global(), "sprintf",  [](const v8::FunctionCallbackInfo<v8::Value>& info){return _format_helper(info, false);});

    add_function(context, context->Global(), "print",    [callback](const v8::FunctionCallbackInfo<v8::Value>& info){callback(_print_helper(info, false));});
    add_function(context, context->Global(), "println",  [callback](const v8::FunctionCallbackInfo<v8::Value>& info){callback(_print_helper(info, true));});

    add_function(context, context->Global(), "printobj", [callback](const v8::FunctionCallbackInfo<v8::Value>& info){
        auto isolate = info.GetIsolate();
        for (int i = 0; i < info.Length(); i++) {
            callback(stringify_value(isolate, info[i]) + "\n");
        }
    });
    add_function(context, context->Global(), "printobjall", [callback](const v8::FunctionCallbackInfo<v8::Value>& info){
        auto isolate = info.GetIsolate();
        for (int i = 0; i < info.Length(); i++) {
            callback(stringify_value(isolate, info[i], true) + "\n");
        }
    });
}


void add_assert(v8::Isolate * isolate,  v8::Local<v8::ObjectTemplate> object_template)
{
    add_function(isolate, object_template, "assert", [](const v8::FunctionCallbackInfo<v8::Value>& info) {
#ifndef NDEBUG
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

//        print_v8_value_details(result);
        assert(result->BooleanValue(context).FromMaybe(false));
#endif
    });
}



std::optional<std::string> get_file_contents(std::string filename) 
{
    time_t ignored_time = 0;
    return get_file_contents(filename, ignored_time);
}

#ifdef _MSC_VER
#include <windows.h>
#endif

std::optional<std::string> get_file_contents(std::string filename, time_t & file_modification_time)
{
    std::string file_contents;
    
#ifdef _MSC_VER
	auto file_handle = CreateFile(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file_handle == INVALID_HANDLE_VALUE) {
		return {};
	}

	FILETIME creationTime,
		lpLastAccessTime,
		lastWriteTime;
	bool err = GetFileTime(file_handle, &creationTime, &lpLastAccessTime, &lastWriteTime);

	auto file_size = GetFileSize(file_handle, nullptr);
	file_contents.resize(file_size);

	ReadFile(file_handle, &file_contents[0], file_size, nullptr, nullptr);


#else



    int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1) {
        return {};
    }
    struct stat file_stat;
    if (fstat(fd, &file_stat) == -1) {
        return {};
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
#endif
    
    return file_contents;
}

bool _get_modification_time_of_filename(std::string filename, time_t & modification_time)
{

#if defined _MSC_VER
	auto file_handle = CreateFile(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file_handle == INVALID_HANDLE_VALUE) {
		return false;
	}

	FILETIME creationTime,
		lpLastAccessTime,
		lastWriteTime;
	bool err = GetFileTime(file_handle, &creationTime, &lpLastAccessTime, &lastWriteTime);

	modification_time = *(time_t*)&lastWriteTime;

	return true;

#else
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1) {
        return false;
    }
    struct stat file_stat;
    if (fstat(fd, &file_stat) == -1) {
        return false;
    }
    
     modification_time = file_stat.st_mtime;
	 return true;
#endif
}



// once a module has been successfully required (in an isolate), its return value will be cached here
//   and subsequent requires of the same module won't re-run the module, just return the
//   cached value
std::mutex require_results_mutex;

typedef std::map<std::string, RequireResult> cached_isolate_modules_t; // a named module result
static std::map<v8::Isolate *, cached_isolate_modules_t> require_results;

void delete_require_cache_for_isolate(v8::Isolate * isolate) {
    std::lock_guard<std::mutex> l(require_results_mutex);
    require_results.erase(isolate);
}

cached_isolate_modules_t & get_loaded_modules(v8::Isolate * isolate)
{
    std::lock_guard<std::mutex> l(require_results_mutex);
    return require_results[isolate];
}


/**
* Takes in javascript source and attempts to compile it to a script.
* On error, it sets the output parameter `error` and returns `false`
*/
bool compile_source(v8::Local<v8::Context> & context, std::string source, v8::Local<v8::Script> & script, v8::Local<v8::Value> & error, v8::ScriptOrigin * script_origin = nullptr)
{
    auto isolate = context->GetIsolate();
    v8::TryCatch try_catch(isolate);
    auto local_source = v8::String::NewFromUtf8(isolate, source.c_str());

    auto script_maybe = v8::Script::Compile(context, local_source, script_origin);
    
    if (try_catch.HasCaught()) {

        ReportException(isolate, &try_catch);

        // TODO: Is this the rignt thing to do?   Can this function be called from within a javascript context?  Maybe for assert()?
        error = try_catch.Exception();
        printf("%s\n", stringify_value(isolate, try_catch.Exception()).c_str());
        if (V8_TOOLKIT_DEBUG) printf("Failed to compile: %s\n", *v8::String::Utf8Value(try_catch.Exception()));
        return false;
    }
    
    script = script_maybe.ToLocalChecked();
    return true;
}


v8::Local<v8::Value> run_script(v8::Local<v8::Context> context, v8::Local<v8::Script> script) {

    v8::Isolate * isolate = context->GetIsolate();
    
    // This catches any errors thrown during script compilation
    v8::TryCatch try_catch(isolate);

    auto maybe_result = script->Run(context);
    if (try_catch.HasCaught()) {
//        printf("Context::run threw exception - about to print details:\n");
        ReportException(isolate, &try_catch);
    } else {
//	printf("Context::run ran without throwing exception\n");
    }

    if(maybe_result.IsEmpty()) {

        v8::Local<v8::Value> e = try_catch.Exception();
        // print_v8_value_details(e);


        if(e->IsExternal()) {
            // no longer supported
        } else {
            throw V8Exception(isolate, v8::Global<v8::Value>(isolate, e));
        }
    }
    v8::Local<v8::Value> result = maybe_result.ToLocalChecked();
    return result;
}
    


/**
 * Compiles the given code within a containing function and returns the exports object
 * @param context
 * @param module_source
 * @param script_origin
 * @return
 */
v8::Local<v8::Value> execute_module(v8::Local<v8::Context> context,
                                    const std::string module_source,
                                    const v8::ScriptOrigin & script_origin,
                                    v8::Local<v8::Function> & compiled_function) {

    auto isolate = context->GetIsolate();


//    std::cerr << fmt::format("ScriptOrigin.ResourceName: {}, line offset: {}, column offset: {}, source: {}",
//                             *v8::String::Utf8Value(script_origin.ResourceName()), script_origin.ResourceLineOffset()->Value(),
//                             script_origin.ResourceColumnOffset()->Value(), module_source.c_str()) << std::endl;


    v8::ScriptCompiler::Source source(v8::String::NewFromUtf8(isolate, module_source.c_str()), script_origin);
    v8::Local<v8::String> parameter_names[] = {
        "module"_v8, "exports"_v8
    };

    v8::TryCatch try_catch(isolate);
    auto maybe_module_function =
        v8::ScriptCompiler::CompileFunctionInContext(context, &source, 2, &parameter_names[0], 0, nullptr);
    if (try_catch.HasCaught()) {
        ReportException(isolate, &try_catch);
        throw V8CompilationException(isolate, try_catch);
    }



    // NEED PROPER ERROR HANDLING HERE
    assert(!maybe_module_function.IsEmpty());
    compiled_function = maybe_module_function.ToLocalChecked();

//    std::cerr << fmt::format("module script id: {}", compiled_function->GetScriptOrigin().ScriptID()->Value()) << std::endl;
//    std::cerr << fmt::format("After CompileFunctionInContext: ScriptOrigin.ResourceName: {}, line offset: {}, column offset: {}, source: {}",
//                             *v8::String::Utf8Value(compiled_function->GetScriptOrigin().ResourceName()), compiled_function->GetScriptOrigin().ResourceLineOffset()->Value(),
//                             compiled_function->GetScriptOrigin().ResourceColumnOffset()->Value(), module_source.c_str()) << std::endl;


    v8::Local<v8::Value> module_params[2];
    module_params[0] = v8::Object::New(isolate);
    auto exports_object = v8::Object::New(isolate);
    module_params[1] = exports_object;
    add_variable(context, module_params[0]->ToObject(), "exports", exports_object);

    {
        v8::TryCatch module_execution_try_catch(isolate);
        (void) compiled_function->Call(context, context->Global(), 2, &module_params[0]);
        if (module_execution_try_catch.HasCaught()) {
            ReportException(isolate, &module_execution_try_catch);
            throw V8ExecutionException(isolate, module_execution_try_catch);
        }
    }

    return exports_object;
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
#define REQUIRE_DEBUG_PRINTS false
//#define REQUIRE_DEBUG_PRINTS true
bool require(
    v8::Local<v8::Context> context,
    std::string filename,
    v8::Local<v8::Value> & result,
    const std::vector<std::string> & paths,
    bool track_file_modification_times,
    bool use_cache,
    func::function<void(RequireResult const &)> callback,
    func::function<std::string(std::string const &)> resource_name_callback)
{

    auto isolate = context->GetIsolate();
    v8::Locker l(isolate);
    if (filename.find("..") != std::string::npos) {
        if (REQUIRE_DEBUG_PRINTS) printf("require() attempted to use a path with more than one . in a row '%s' (disallowed as simple algorithm to stop tricky paths)", filename.c_str());
        isolate->ThrowException("Cannot specify a file containing .."_v8);
        return false;
    }

    for (auto suffix : std::vector<std::string>{"", ".js", ".json", }) {
        for (auto path : paths) {
#ifdef _MSC_VER
            auto complete_filename = path + "\\" + filename + suffix;
#else
            auto complete_filename = path + "/" + filename + suffix;
#endif

            std::string resource_name = complete_filename;
            if (resource_name_callback) {
                resource_name = resource_name_callback(complete_filename);
            }

            time_t file_modification_time = 0;
            auto file_contents = get_file_contents(complete_filename, file_modification_time);

            if (!file_contents) {
                if (REQUIRE_DEBUG_PRINTS) printf("Module not found at %s\n", complete_filename.c_str());
                continue;
            }

            // get the map of cached results for this isolate (guaranteed to exist because it was created before this lambda)
            // This is read-only and per-isolate and the only way to add is to be in the isolate
            if (use_cache) {
                std::lock_guard<std::mutex> l(require_results_mutex);
                auto & isolate_require_results = require_results[isolate];

                auto cached_require_results = isolate_require_results.find(complete_filename);
                if (cached_require_results != isolate_require_results.end()) {
                    if (REQUIRE_DEBUG_PRINTS)
                        printf("Found cached results, using cache instead of re-running module\n");

                    // if we don't care about file modifications or the file modification time is the same as before,
                    //   return the cached result
                    if (!track_file_modification_times ||
                        file_modification_time == cached_require_results->second.time) {
                        if (REQUIRE_DEBUG_PRINTS) printf("Returning cached results\n");
                        result = cached_require_results->second.result.Get(isolate);
                        return true;
                    } else {
                        if (REQUIRE_DEBUG_PRINTS)
                            printf("Not returning cached results because modification time was no good\n");
                    }
                } else {
                    if (REQUIRE_DEBUG_PRINTS)
                        printf("Didn't find cached version for isolate %p %s\n", isolate, complete_filename.c_str());
                }
            }

            // CACHE WAS SEARCHED AND NO RESULT FOUND - DO THE WORK AND CACHE THE RESULT AFTERWARDS

            // Compile the source code.

            v8::Local<v8::Script> script;

            if (std::regex_search(filename, std::regex(".json$"))) {

                v8::ScriptOrigin script_origin(v8::String::NewFromUtf8(isolate,
                                                                       resource_name.c_str()),
                                               v8::Integer::New(isolate, 0), // line offset
                                               v8::Integer::New(isolate, 0)  // column offset
                );

                v8::Local<v8::Value> error;
                v8::TryCatch try_catch(isolate);
                // TODO: make sure requiring a json file is being tested
                if (REQUIRE_DEBUG_PRINTS) printf("About to try to parse json: %s\n", file_contents->c_str());
                auto maybe_result = v8::JSON::Parse(isolate, v8::String::NewFromUtf8(isolate, file_contents->c_str()));
                if (try_catch.HasCaught()) {
                    try_catch.ReThrow();
                    if (REQUIRE_DEBUG_PRINTS)
                        printf("Couldn't run json for %s, error: %s\n", complete_filename.c_str(),
                               *v8::String::Utf8Value(try_catch.Exception()));
                    return false;
                }
                result = maybe_result.ToLocalChecked();

                // cache the result for subsequent requires of the same module in the same isolate
                if (use_cache) {
                    std::lock_guard<std::mutex> l(require_results_mutex);
                    auto & isolate_require_results = require_results[isolate];
                    auto i = isolate_require_results.find(complete_filename);
                    if (i == isolate_require_results.end()) {
                        isolate_require_results.emplace(complete_filename,
                                                        RequireResult(isolate, context, v8::Local<v8::Function>(),
                                                                      result, time(nullptr)));
                    }
                }

            } else {

                v8::ScriptOrigin script_origin(v8::String::NewFromUtf8(isolate, resource_name.c_str()),
                                               0_v8, // line offset
                                               26_v8,  // column offset - cranked up because v8 subtracts a bunch off but if it's negative then chrome ignores it
                                               v8::Local<v8::Boolean>());

                v8::Local<v8::Value> error;
                v8::Local<v8::Function> module_function;

                result = execute_module(context, *file_contents, script_origin, module_function);

                std::lock_guard<std::mutex> l(require_results_mutex);
                auto & isolate_require_results = require_results[isolate];
                isolate_require_results.emplace(complete_filename,
                                                RequireResult(isolate, context, module_function, result,
                                                              file_modification_time));
                if (callback) {
                    callback(isolate_require_results.find(complete_filename)->second);
                }
            }



            // printf("Require final result: %s\n", stringify_value(isolate, result).c_str());
            // printf("Require returning resulting object for module %s\n", complete_filename.c_str());
            return true;

        }
    }
    if (REQUIRE_DEBUG_PRINTS) printf("Couldn't find any matches for %s\n", filename.c_str());
    isolate->ThrowException("No such module found in any search path"_v8);
    return false;
}


void add_module_list(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template)
{
    using ReturnType = std::map<std::string, v8::Global<v8::Value>&>;

    add_function(isolate, object_template, "module_list",

        [isolate]{return scoped_run(isolate, [isolate]()->ReturnType {
            ReturnType results;
            auto & isolate_results = require_results[isolate];
            for (auto & result_pair : isolate_results) {
                results.emplace(result_pair.first, result_pair.second.result);
            }
			return results;
		});
	});
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
        [isolate, paths](const std::string & filename) {
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

    
void dump_prototypes(v8::Isolate * isolate, v8::Local<v8::Object> object)
{
    fprintf(stderr, "Looking at prototype chain\n");
    int i = 0;
	while (!object->IsNull()) {
	    /* This code assumes things about what is in the internfieldcount that isn't safe to assume
	    if (object->InternalFieldCount() == 1) {
		auto wrap = v8::Local<v8::External>::Cast(object->GetInternalField(0));
		// the type of the class wrapper doesn't matter - it's just a pointer
		auto wrapped_data = static_cast<WrappedData<int> *>(wrap->Value());
		fprintf(stderr, "Prototype is wrapped object with debug string type name: %s\n", wrapped_data->native_object_type.c_str());
	    }
	    */
	    fprintf(stderr, "[level: %d] %s:\n", i++, *v8::String::Utf8Value(object));
	    // print_v8_value_details(foo);
	    fprintf(stderr, "%s\n", stringify_value(isolate, object).c_str());
	    object = v8::Local<v8::Object>::Cast(object->GetPrototype());
	}
	fprintf(stderr, "Done looking at prototype chain\n");
}


bool compare_contents(v8::Isolate * isolate, const v8::Local<v8::Value> & left, const v8::Local<v8::Value> & right)
{
     log.info(LogT::Subjects::V8TOOLKIT, "Comparing two things");
    auto context = isolate->GetCurrentContext();

    // if they're both undefined, then they are equal.  If only one, then they're not.
    if (left->IsUndefined() || right->IsUndefined()) {
        return left->IsUndefined() && right->IsUndefined();
    }
    
    v8::Local<v8::Boolean> bool_left;
    v8::Local<v8::Boolean> bool_right;
    
    // if the left is a bool, return true if right is a bool and they match, otherwise false
    if (left->IsBoolean()) {
        log.info(LogT::Subjects::V8TOOLKIT, "Checking two booleans");
        return  right->IsBoolean() && left->ToBoolean(context).ToLocalChecked()->Value() == right->ToBoolean(context).ToLocalChecked()->Value();
    }

    if (left->IsNumber()) {
        log.info(LogT::Subjects::V8TOOLKIT, "Checking two numbers");
        auto ln = left->ToNumber(context).ToLocalChecked()->Value();
        auto rn = right->ToNumber(context).ToLocalChecked()->Value();
        log.info(LogT::Subjects::V8TOOLKIT, "Comparing %f and %f", ln, rn);
        return  right->IsNumber() && left->ToNumber(context).ToLocalChecked()->Value() == right->ToNumber(context).ToLocalChecked()->Value();
    }
    
    if (left->IsString()) {
        log.info(LogT::Subjects::V8TOOLKIT, "Checking two strings");
        if (!right->IsString()) {
            return false;
        }
        return !strcmp(*v8::String::Utf8Value(left), *v8::String::Utf8Value(right));            
    }

    if (left->IsArray()) {
        log.info(LogT::Subjects::V8TOOLKIT, "Checking two arrays");
        if (!right->IsArray()) {
            return false;
        }
        auto array_left = v8::Local<v8::Array>::Cast(left);
        auto array_right = v8::Local<v8::Array>::Cast(right);
        
        auto left_length = get_array_length(isolate, array_left);
        auto right_length = get_array_length(isolate, array_right);
        
        if (left_length != right_length) {
            log.info(LogT::Subjects::V8TOOLKIT, "Array lengths differ %d %d", (int)left_length, (int) right_length);
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
        log.info(LogT::Subjects::V8TOOLKIT, "Checking two arrays\n");
        
        auto object_right = v8::Local<v8::Object>::Cast(right);
        
        // if they're not both objects, return false
        if (object_right.IsEmpty()) {
            log.info(LogT::Subjects::V8TOOLKIT, "right value not object\n");
            return false;
        }
        auto left_keys = get_object_keys(isolate, object_left);
        auto right_keys = get_object_keys(isolate, object_right);
        if (left_keys.size() != right_keys.size()) {
            log.info(LogT::Subjects::V8TOOLKIT, "key count mismatch: %d %d", (int)left_keys.size(), (int)right_keys.size());
            return false;
        }

        for(auto left_key : left_keys) {
            auto left_value = object_left->Get(context, v8::String::NewFromUtf8(isolate, left_key.c_str()));
            auto right_value = object_right->Get(context, v8::String::NewFromUtf8(isolate, left_key.c_str()));
            if (right_value.IsEmpty()) {
                log.info(LogT::Subjects::V8TOOLKIT, "right side doesn't have key: %s", left_key.c_str());
                return false;
            } else if (!compare_contents(isolate, left_value.ToLocalChecked(), right_value.ToLocalChecked())) {
                log.info(LogT::Subjects::V8TOOLKIT, "Recursive check of value in both objects returned false for key %s", left_key.c_str());
                return false;
            }
        }
        
        return true;
    }
    log.info(LogT::Subjects::V8TOOLKIT, "Returning false because left value is of unknown/unhandled type");
    
    return false;    
}





AnyBase::~AnyBase() {}


/*
* "interesting" means things not a part of Object - or at least pretty close to that
*/
std::vector<std::string> get_interesting_properties(v8::Local<v8::Context> context, v8::Local<v8::Object> object)
{
    auto isolate = context->GetIsolate();
    auto current_object = object;

    std::vector<std::string> results;


    // if the current object's prototype is null, then this is the Object object, and we don't want it's properties
     while (!current_object.IsEmpty() &&
         !current_object->IsNull() &&
         !current_object->IsUndefined() &&
         !current_object->GetPrototype()->IsNull()) {

         // for some reason, requesting all properties crashes on constructor function objects like `Object`
         auto maybe_own_property_names = current_object->GetOwnPropertyNames(context, v8::PropertyFilter::ONLY_CONFIGURABLE);
         if (maybe_own_property_names.IsEmpty()) {
             return results;
         }
         auto own_property_names = CastToNative<std::vector<std::string>>()(isolate, maybe_own_property_names.ToLocalChecked());
         results.insert(results.end(), own_property_names.begin(), own_property_names.end());
         current_object = current_object->GetPrototype().As<v8::Object>();
     }


    return results;
}



SetWeakCallbackData::SetWeakCallbackData(func::function<void(v8::WeakCallbackInfo<SetWeakCallbackData> const &)> callback,
                                         v8::Isolate * isolate,
                                         const v8::Local<v8::Object> & javascript_object, bool destructive) :
    callback(callback),
    destructive(destructive)
    {
//        std::cerr << fmt::format("Creating weak callback data with destructive: {}", this->destructive) << std::endl;
        this->global.Reset(isolate, javascript_object);
    }


SetWeakCallbackData * global_set_weak(v8::Isolate * isolate,
                            const v8::Local<v8::Object> & javascript_object,
                            func::function<void(v8::WeakCallbackInfo<SetWeakCallbackData> const &)> callback, bool destructive)
{
    // this memory deleted in the GC callback
    auto callback_data = new SetWeakCallbackData(callback, isolate, javascript_object, destructive);

    // set the callback on the javascript_object to be called when it's garbage collected
    callback_data->global.template SetWeak<SetWeakCallbackData>(callback_data,
                                                                [](const v8::WeakCallbackInfo<SetWeakCallbackData> & info) {
                                                                    SetWeakCallbackData * callback_data = info.GetParameter();
                                                                    callback_data->callback(info);
                                                                    callback_data->global.Reset();
                                                                    delete callback_data; // delete the memory allocated when global_set_weak is called
                                                                }, v8::WeakCallbackType::kParameter);

    return callback_data;
}




void foreach_filesystem_helper(const std::string & directory_name,
                               const volatile bool files,
                               const volatile bool directories,
                               std::function<void(const std::string &)> const & callback)
{
    // This probably works on more than just APPLE
#ifndef _MSC_VER

    auto full_directory_name = directory_name;
    DIR * dir = opendir(full_directory_name.c_str());
    if (dir == NULL) {
        printf("Could not open directory %s\n", full_directory_name.c_str());
        return;
    }
    struct dirent * dp;
    while ((dp = readdir(dir)) != NULL) {

        if ((dp->d_type == DT_DIR && directories && strcmp(dp->d_name, ".") && strcmp(dp->d_name, "..")) ||
            (dp->d_type == DT_REG && files)) {
            callback(dp->d_name);
        }
    }
    (void)closedir(dir);
    return;

#else

    WIN32_FIND_DATA file_metadata;
	HANDLE directory = FindFirstFile((directory_name + "\\*").c_str(), &file_metadata);

	if (directory ==  INVALID_HANDLE_VALUE) {
		printf("Could not open directory %s\n", directory_name.c_str());
		return;
	}
	do {
		if (!strcmp(file_metadata.cFileName, ".") || !strcmp(file_metadata.cFileName, "..")) {
			continue;
		}
		if (
			((file_metadata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && directories) ||
			((!(file_metadata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) && files)) {
			callback(file_metadata.cFileName);
		} else {
			printf("Skipping file %s because it's not of desired type file: %d directory: %d\n", file_metadata.cFileName, files, directories);
			continue;
		}


	} while(FindNextFile(directory, &file_metadata));

	FindClose(directory);
	return;


#endif

}

void foreach_file(const std::string & directory_name, std::function<void(const std::string &)> const & callback) {
    return foreach_filesystem_helper(directory_name, true, false, callback);
}

void foreach_directory(const std::string & directory_name, std::function<void(const std::string &)> const & callback) {
    return foreach_filesystem_helper(directory_name, false, true, callback);
}



}  // namespace v8toolkit




