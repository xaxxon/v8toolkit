#pragma once



#include <xl/log.h>


namespace v8toolkit::class_parser {

class LogSubjects {
    inline static std::string subject_names[] = {"default"};

public:
    enum Subjects {
        Comments, // for log messages about parsing comments
        Class,
        Methods,
        DataMembers,
        Enums,
        Constructors,
        Destructors
    };

    static std::string const & get_subject_name(Subjects subject) {
        return subject_names[static_cast<std::underlying_type_t<Subjects>>(subject)];
    }
};

using LogLevelsT = xl::log::DefaultLevels;
using LogT = xl::Log<LogLevelsT, LogSubjects>;

inline LogT log;

} // end namespace v8toolkit::class_parser