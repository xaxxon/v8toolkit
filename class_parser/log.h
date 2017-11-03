#pragma once



#include <xl/log.h>


namespace v8toolkit::class_parser {

class LogSubjects {
    inline static std::string subject_names[] = {
        "comments", "class", "methods", "data members", "enums", "constructors", "destructors", "bidirectional output",
        "bindings output", "javascript stub output", "jsdoc", "should be wrapped"
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

        LOG_LAST_SUBJECT

    };

    static std::string const & get_subject_name(Subjects subject) {
        return subject_names[static_cast<std::underlying_type_t<Subjects>>(subject)];
    }
};

using LogLevelsT = xl::log::DefaultLevels;
using LogT = xl::Log<LogLevelsT, LogSubjects>;


#if 0
auto printer = [](LogT::LogMessage const & message){std::cerr << fmt::format("{}", message.string) << std::endl;};
inline LogT log(printer);
#else
inline LogT log;
#endif

} // end namespace v8toolkit::class_parser