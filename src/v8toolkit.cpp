
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
#include <fmt/format.h>
#include <boost/format.hpp>

#include "v8_class_wrapper.h"
#include "./v8toolkit.h"


namespace v8toolkit {

void process_v8_flags(int & argc, char ** argv)
{
    v8::V8::SetFlagsFromCommandLine(&argc, argv, true);
}


void expose_debug(const std::string & debug_name = "debug") {
    static const char * EXPOSE_DEBUG_AS = "--expose-debug-as=";
    std::string expose_debug_as(EXPOSE_DEBUG_AS);
    expose_debug_as += debug_name;
    v8::V8::SetFlagsFromString(expose_debug_as.c_str(), expose_debug_as.length());
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
//        print_v8_value_details(result);
        assert(assert_result);
    });
}



bool get_file_contents(std::string filename, std::string & file_contents) 
{
    time_t ignored_time;
    return get_file_contents(filename, file_contents, ignored_time);
}

#ifdef _MSC_VER
#include <windows.h>
#endif

bool get_file_contents(std::string filename, std::string & file_contents, time_t & file_modification_time)
{
#ifdef _MSC_VER
	auto file_handle = CreateFile(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file_handle == INVALID_HANDLE_VALUE) {
		return false;
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
#endif
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

struct RequireResult {
    time_t time;
    v8::Global<v8::Value> result;
    std::unique_ptr<v8::ScriptOrigin> script_origin;
    RequireResult(v8::Isolate * isolate, const time_t & time, v8::Local<v8::Value> result, std::unique_ptr<v8::ScriptOrigin> script_origin) :
            time(time), result(v8::Global<v8::Value>(isolate, result)), script_origin(std::move(script_origin))
    {}
    // IF CRASHING IN RequireResult DESTRUCTOR, MAKE SURE TO CALL delete_require_cache_for_isolate BEFORE DESTROYING ISOLATE
};

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
	printf("Context::run threw exception - about to print details:\n");
	ReportException(isolate, &try_catch);
    } else {
	printf("Context::run ran without throwing exception\n");
    }

    if(maybe_result.IsEmpty()) {

        v8::Local<v8::Value> e = try_catch.Exception();
        // print_v8_value_details(e);


	// This functionality causes the javascript to not be able to catch and understand the exception
        if(e->IsExternal()) {
            auto anybase = (AnyBase *)v8::External::Cast(*e)->Value();
            auto anyptr_exception_ptr = dynamic_cast<Any<std::exception_ptr> *>(anybase);
            assert(anyptr_exception_ptr); // cannot handle other types at this time TODO: throw some other type of exception if this happens UnknownExceptionException or something

            // TODO: Are we leaking a copy of this exception by not cleaning up the exception_ptr ref count?
            std::rethrow_exception(anyptr_exception_ptr->get());
        } else {
            printf("v8 internal exception thrown: %s\n", *v8::String::Utf8Value(e));
            throw V8Exception(isolate, v8::Global<v8::Value>(isolate, e));
        }
    }
    v8::Local<v8::Value> result = maybe_result.ToLocalChecked();
    return result;
}
    


v8::Local<v8::Value> execute_module(v8::Local<v8::Context> context,
                                    const std::string module_source,
                                    const v8::ScriptOrigin & script_origin) {

    auto isolate = context->GetIsolate();

    v8::ScriptCompiler::Source source(v8::String::NewFromUtf8(isolate, module_source.c_str()), script_origin);
    v8::Local<v8::String> parameter_names[] = {
        v8::String::NewFromUtf8(isolate, "module"),
        v8::String::NewFromUtf8(isolate, "exports")
    };
    auto maybe_module_function =
        v8::ScriptCompiler::CompileFunctionInContext(context, &source, 2, &parameter_names[0], 0, nullptr);

    // NEED PROPER ERROR HANDLING HERE
    assert(!maybe_module_function.IsEmpty());
    auto module_function = maybe_module_function.ToLocalChecked();

    v8::Local<v8::Object> receiver = v8::Object::New(isolate);
    v8::Local<v8::Value> module_params[2];
    module_params[0] = v8::Object::New(isolate);
    auto exports_object = v8::Object::New(isolate);
    module_params[1] = exports_object;
    add_variable(context, module_params[0]->ToObject(), "exports", exports_object);

    (void)module_function->Call(context, context->Global(), 2, &module_params[0]);

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
//#define REQUIRE_DEBUG_PRINTS false
#define REQUIRE_DEBUG_PRINTS true
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
        if (REQUIRE_DEBUG_PRINTS) printf("require() attempted to use a path with more than one . in a row '%s' (disallowed as simple algorithm to stop tricky paths)", filename.c_str());
        isolate->ThrowException(v8::String::NewFromUtf8(isolate, "Cannot specify a file containing .."));
        return false;
    }

    for (auto suffix : std::vector<std::string>{"", ".js", ".json", }) {
        for (auto path : paths) {
            try {
#ifdef _MSC_VER
		auto complete_filename = path + "\\" + filename + suffix;
#else
                auto complete_filename = path + "/" + filename + suffix;
#endif
        
                std::string file_contents;
                time_t file_modification_time = 0;
                if (!get_file_contents(complete_filename, file_contents, file_modification_time)) {
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
                        if (REQUIRE_DEBUG_PRINTS) printf("Found cached results, using cache instead of re-running module\n");
                        
                        // if we don't care about file modifications or the file modification time is the same as before,
                        //   return the cached result
                        if (!track_file_modification_times || file_modification_time == cached_require_results->second.time) {
                            if (REQUIRE_DEBUG_PRINTS) printf("Returning cached results\n");
                            result = cached_require_results->second.result.Get(isolate);
                            return true;
                        } else {
                            if (REQUIRE_DEBUG_PRINTS) printf("Not returning cached results because modification time was no good\n");
                        }
                    } else {
                        if (REQUIRE_DEBUG_PRINTS) printf("Didn't find cached version for isolate %p %s\n", isolate, complete_filename.c_str());
                    }
                }

                // Compile the source code.
                auto script_origin =
                        std::make_unique<v8::ScriptOrigin>(v8::String::NewFromUtf8(isolate,
                                                                                   complete_filename.c_str()),
                v8::Integer::New(isolate, 1));
                                            
                if (std::regex_search(filename, std::regex(".json$"))) {
                    v8::Local<v8::Script> script;
                    v8::Local<v8::Value> error;
                    v8::TryCatch try_catch(isolate);
                    // TODO: make sure requiring a json file is being tested
                    if (REQUIRE_DEBUG_PRINTS) printf("About to try to parse json: %s\n", file_contents.c_str());
                    auto maybe_result = v8::JSON::Parse(isolate, v8::String::NewFromUtf8(isolate,file_contents.c_str()));
                    if (try_catch.HasCaught()) {
                        try_catch.ReThrow();
                        if (REQUIRE_DEBUG_PRINTS) printf("Couldn't run json for %s, error: %s\n", complete_filename.c_str(), *v8::String::Utf8Value(try_catch.Exception()));
                        return false;
                    }
                } else {
                    v8::Local<v8::Script> script;
                    v8::Local<v8::Value> error;
                    result = execute_module(context, file_contents, *script_origin);
//                    if (!compile_source(context, file_contents, script, error, script_origin.get())) {
//                        isolate->ThrowException(error);
//                        if (REQUIRE_DEBUG_PRINTS) printf("Couldn't compile .js for %s\n", complete_filename.c_str());
//                        return false;
//                    }

                    // set up the module and exports stuff
                    // TODO: module object should also have "children" array, "filename" string, "id" string, "loaded" boolean, "parent" module object that first loaded this one
//                    (void)context->Global()->Set(context, v8::String::NewFromUtf8(isolate, "global"), context->Global());

                    // return value of run doesn't matter, only what is hooked up to export variable
//		    (void)run_script(context, script);
                    //(void)script->Run(context);
//                    maybe_result = context->Global()->Get(context, v8::String::NewFromUtf8(isolate, "exports"));
//                    context->Global()->Delete(context, v8::String::NewFromUtf8(isolate, "module"));
//                    context->Global()->Delete(context, v8::String::NewFromUtf8(isolate, "exports"));
		}


                
                // cache the result for subsequent requires of the same module in the same isolate
                if (use_cache) {
                    std::lock_guard<std::mutex> l(require_results_mutex);
                    auto & isolate_require_results = require_results[isolate];
                    isolate_require_results.emplace(complete_filename, RequireResult(isolate, file_modification_time,
                                                                                     result, std::move(script_origin)));
                }
                // printf("Require final result: %s\n", stringify_value(isolate, result).c_str());
                // printf("Require returning resulting object for module %s\n", complete_filename.c_str());
                return true;
            }catch(...) {}
            // if any failures, try the next path if it exists
        }
    }
    if (REQUIRE_DEBUG_PRINTS) printf("Couldn't find any matches for %s\n", filename.c_str());
    isolate->ThrowException(v8::String::NewFromUtf8(isolate, "No such module found in any search path"));
    return false;
}


void add_module_list(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template)
{
    using ReturnType = std::vector<std::pair<std::string, v8::Global<v8::Value>&>>;

    add_function(isolate, object_template, "module_list",

        [isolate]{return scoped_run(isolate, [isolate]()->ReturnType {
            ReturnType results;
            auto & isolate_results = require_results[isolate];
            for (auto & result_pair : isolate_results) {
                results.emplace_back(result_pair.first, result_pair.second.result);
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
	while (!object->IsNull()) {
	    /* This code assumes things about what is in the internfieldcount that isn't safe to assume
	    if (object->InternalFieldCount() == 1) {
		auto wrap = v8::Local<v8::External>::Cast(object->GetInternalField(0));
		// the type of the class wrapper doesn't matter - it's just a pointer
		auto wrapped_data = static_cast<WrappedData<int> *>(wrap->Value());
		fprintf(stderr, "Prototype is wrapped object with debug string type name: %s\n", wrapped_data->native_object_type.c_str());
	    }
	    */
	    fprintf(stderr, "%s:\n", *v8::String::Utf8Value(object));
	    // print_v8_value_details(foo);
	    fprintf(stderr, "%s\n", stringify_value(isolate, object).c_str());
	    object = v8::Local<v8::Object>::Cast(object->GetPrototype());
	}
	fprintf(stderr, "Done looking at prototype chain\n");
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




