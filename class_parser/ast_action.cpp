

#include <unistd.h>

#include <fstream>




#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#include "clang/Frontend/FrontendPluginRegistry.h"
#pragma clang diagnostic pop


#include "ast_action.h"
#include "helper_functions.h"

#include "output_modules/bindings_output.h"



#include <xl/templates.h>
#include <xl/regexer.h>
#include <xl/json.h>

namespace v8toolkit::class_parser {

extern int print_logging;


LogT log = []{

    // configure xl::templates logging as well
    if (!xl::templates::log.is_status_file_enabled()) {
        xl::templates::log.add_callback(std::cout, "xl::templates: ");
        xl::templates::log.set_status(v8toolkit::class_parser::LogLevelsT::Levels::Info, false);
        xl::templates::log.set_status(v8toolkit::class_parser::LogLevelsT::Levels::Warn, false);
        xl::templates::log.set_status(v8toolkit::class_parser::LogLevelsT::Levels::Error, true);
        xl::templates::log.enable_status_file("class_parser_plugin_templates.log_status");
    }

    LogT log;
    // if status file was already enabled (by test harness, for example), then don't mess with it
    if (!v8toolkit::class_parser::log.is_status_file_enabled()) {
        // set defaults if file doesn't exist
        v8toolkit::class_parser::log.add_callback(std::cout, "v8toolkit::class_parser: ");
        log.set_status(v8toolkit::class_parser::LogLevelsT::Levels::Info, false);
        log.set_status(v8toolkit::class_parser::LogLevelsT::Levels::Warn, false);
        log.enable_status_file("class_parser_plugin.log_status");
        log.set_status(v8toolkit::class_parser::LogLevelsT::Levels::Error, true);

        
//        std::cerr << fmt::format("level status:") << std::endl;
//        for (auto [b, i] : xl::each_i(log.level_status)) {
//            std::cerr << fmt::format("{}: {}", i, (bool)b) << std::endl;
//        }
        
    }
    return log;
}();


bool PrintFunctionNamesAction::BeginInvocation(CompilerInstance & ci) {
    compiler_instance = &ci;
//    std::cerr << fmt::format("setting global compiler instance to {}", (void*)compiler_instance) << std::endl;

    log.info(LogSubjects::Subjects::ClassParser, "BeginInvocation");

    if (this->output_modules.empty()) {
        cerr << "NO OUTPUT MODULES SPECIFIED - *ABORTING* - did you mean to pass --use-default-output-modules" << endl;
        return false;
    } else {
//        std::cerr << fmt::format("{} output modules registered", this->output_modules.size()) << std::endl;
        return true;
    }
}

#ifndef TEST_MODE
static FrontendPluginRegistry::Add <PrintFunctionNamesAction>
    X("v8toolkit-generate-bindings", "generate v8toolkit bindings");
#endif

// This is called when all parsing is done
void PrintFunctionNamesAction::EndSourceFileAction() {
    log.info(LogSubjects::Subjects::ClassParser, "EndSourceFileAction");
}

void PrintFunctionNamesAction::add_output_module(unique_ptr<OutputModule> output_module) {
    v8toolkit::class_parser::log.info(LogSubjects::Subjects::ClassParser, "Adding output module {}", output_module->get_name());
    this->output_modules.push_back(std::move(output_module));
}

void PrintFunctionNamesAction::PrintHelp(llvm::raw_ostream & ros) {
    std::cerr << fmt::format("Printing help") << std::endl;
    ros << "Help for PrintFunctionNames plugin goes here\n";
}

std::unique_ptr<ASTConsumer> PrintFunctionNamesAction::CreateASTConsumer(CompilerInstance & CI,
                                               llvm::StringRef) {
    return llvm::make_unique<ClassHandlerASTConsumer>(CI, this->output_modules);
}

PrintFunctionNamesAction::PrintFunctionNamesAction()

{
    std::cerr << fmt::format("RESETTING ALL VALUES IN FOR NEW ACTION") << std::endl;
    WrappedClass::wrapped_classes.clear();
    WrappedClass::used_constructor_names.clear();
//    PrintFunctionNamesAction::config_data_initialized = false;
//    PrintFunctionNamesAction::config_data = xl::json::Json{};

}

PrintFunctionNamesAction::~PrintFunctionNamesAction()
{
}



bool PrintFunctionNamesAction::ParseArgs(const CompilerInstance & CI,
               const std::vector<std::string> & args) {
    std::cerr << fmt::format("Parsing args (updated assertion version)") << std::endl;
    bool first_argument = true;
    for (unsigned i = 0, e = args.size(); i < e; ++i) {
        llvm::errs() << "PrintFunctionNames arg = " << args[i] << "\n";

        static xl::Regex config_file_regex("^--config-file=(.*)$");
        std::smatch match_results;
        if (args[i] == "--use-default-output-modules") {
            std::cerr << fmt::format("Using default output modules") << std::endl;
            output_modules.push_back(std::make_unique<javascript_stub_output::JavascriptStubOutputModule>());
            output_modules.push_back(std::make_unique<bindings_output::BindingsOutputModule>());
            output_modules.push_back(std::make_unique<bidirectional_output::BidirectionalOutputModule>());
            output_modules.push_back(std::make_unique<javascript_subclass_template_output::JavascriptSubclassTemplateOutputModule>());
        } else if (auto matches = config_file_regex.match(args[i])) {
            if (!first_argument) {
                throw ClassParserException("Config file must be first parameter if it is specified at all");
            }
            auto filename = matches[1];
            log.info(LogT::Subjects::ClassParser, "Config file specified as: '{}'", filename);
            ifstream config_file(filename);
            if (!config_file) {
                constexpr size_t buffer_size = 1024;
                char buffer[buffer_size];
                throw ClassParserException("Couldn't open config file: {} from current directory {}", filename, getcwd(buffer, buffer_size));
            }
            std::string configuration((std::istreambuf_iterator<char>(config_file)),
                                      std::istreambuf_iterator<char>());

            xl::json::Json config_json(configuration);
            if (!config_json.is_valid()) {
                throw ClassParserException("Invalid JSON in config file: {}", filename);
            }
            this->config_data = std::move(config_json);
            this->config_data_initialized = true;

            v8toolkit::class_parser::log.info(LogT::Subjects::ConfigFile, "Loaded config file:\n{}\n", this->config_data.get_source());

        }
        first_argument = false;
    }
    if (args.size() && args[0] == "help") {
        PrintHelp(llvm::errs());
    }

    return true;
}



xl::json::Json PrintFunctionNamesAction::get_config_data() {
    if (!PrintFunctionNamesAction::config_data_initialized) {
        throw ClassParserException("json config data not initialized");
    }
    return PrintFunctionNamesAction::config_data;
}


}