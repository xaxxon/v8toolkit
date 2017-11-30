
#pragma once

#include <string>
#include <xl/log.h>

namespace v8toolkit {

struct LoggingSubjects {
    inline static std::string subject_names[] = {"Object Management",
                                                 "Runtime Exception",
                                                 "Compilation Exception",
                                                 "wrapped function call",
                                                 "wrapped data member access",
                                                 "debug websocket"

    };

    enum class Subjects {
        V8_OBJECT_MANAGEMENT, // when core V8 objects are created or town down
        RUNTIME_EXCEPTION,
        COMPILATION_EXCEPTION,
        WRAPPED_FUNCTION_CALL,
        WRAPPED_DATA_MEMBER_ACCESS,
        DebugWebSocket,

        LOG_LAST_SUBJECT
    };
};


using LogT = xl::log::Log<xl::log::DefaultLevels, LoggingSubjects>;
inline LogT log;

} // end namespace v8toolkit