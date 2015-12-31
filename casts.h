#pragma once


// Specialized types that know how to convert from a v8::Value to a primitive type
template<typename T>
struct CastToNative {};



// casts from a primitive to a v8::Value
template<typename T>
struct CastToJS {};
