

message(looking for v8 libs in ${V8_LIB_DIR})

set(V8_LIB_NAMES icui18n icuuc v8 v8_libbase v8_libplatform)

FOREACH(LIB_NAME ${V8_LIB_NAMES})
    FIND_LIBRARY(FOUND_LIB_${LIB_NAME} ${LIB_NAME} PATHS ${V8_LIB_DIR})
    add_library(v8::${LIB_NAME} UNKNOWN IMPORTED)
    set_target_properties(v8::${LIB_NAME} PROPERTIES
            IMPORTED_LOCATION "${FOUND_LIB_${LIB_NAME}}"
            INTERFACE_COMPILE_OPTIONS ""
            INTERFACE_INCLUDE_DIRECTORIES ${V8_INCLUDE_DIR})

    LIST(APPEND V8_LIBS v8::${LIB_NAME})
    MESSAGE("Lib: v8::${LIB_NAME}")
    MESSAGE("Found Lib: ${FOUND_LIB_${LIB_NAME}}")
ENDFOREACH(LIB_NAME)
