#pragma once



#include <xl/log.h>


namespace v8toolkit::class_parser {

struct LogSubjects {
    inline static std::string subject_names[] = {
        "comments", "class", "methods", "data members", "enums", "constructors", "destructors", "bidirectional output",
        "bindings output", "javascript stub output", "jsdoc", "should be wrapped", "class parser"
    };

public:
    enum Subjects {
        Comments, // for log messages about parsing comments
        Class,
        Methods,
        DataMembers,
        Enums,
        Constructors,
        Destructors,
        BidirectionalOutput,
        BindingsOutput,
        JavaScriptStubOutput,
        JSDoc,
        ShouldBeWrapped,
        ClassParser,

        LOG_LAST_SUBJECT

    };
};

using LogLevelsT = xl::log::DefaultLevels;
using LogT = xl::log::Log<LogLevelsT, LogSubjects>;


#if 0
auto printer = [](LogT::LogMessage const & message){std::cerr << fmt::format("{}", message.string) << std::endl;};
inline LogT log(printer);
#else
extern LogT log;
#endif

} // end namespace v8toolkit::class_parser