
#if defined V8HELPERS
#include "v8toolkit/v8helpers.h"

#elif defined V8TOOLKIT
#include "v8toolkit/v8toolkit.h"

#elif defined V8_CLASS_WRAPPER
#include "v8toolkit/v8_class_wrapper_impl.h"

#elif defined JAVASCRIPT
#include "v8toolkit/javascript.h"

#endif

#ifdef STD_FUNCTIONS
#include "v8toolkit/javascript.h"
template class std::function<void()>;
template class std::function<int(int)>;
template class std::function<char(char)>;
template class std::function<float(float)>;
template class std::function<double(double)>;
#endif



#ifdef CREATE_CONTEXT
#include "v8toolkit/javascript.h"
auto create_context() {
    auto isolate = v8toolkit::Platform::create_isolate();
    auto context = isolate->create_context();
    return context;
}
#endif


#ifdef WRAP_EMPTY_CLASS
#include "v8toolkit/javascript.h"
#include "v8toolkit/wrapped_class_base.h"
class DataMemberClass : public v8toolkit::WrappedClassBase {

};

void wrap_DataMemberClass(v8toolkit::IsolatePtr isolate) {
    v8toolkit::V8ClassWrapper<DataMemberClass>::get_instance(*isolate);
};
#endif

#ifdef WRAP_DATA_MEMBER_CLASS
#include "v8toolkit/javascript.h"
using namespace v8toolkit;

struct DataMemberClass : public v8toolkit::WrappedClassBase {
    int a;
};

void wrap_DataMemberClass(v8toolkit::IsolatePtr isolate) {
    V8ClassWrapper<DataMemberClass> & wrapper = V8ClassWrapper<DataMemberClass>::get_instance(*isolate);
    wrapper.add_member<int, DataMemberClass, &DataMemberClass::a>("a");

};
#endif


#ifdef WRAP_5_INT_DATA_MEMBERS_CLASS
#include "v8toolkit/javascript.h"
using namespace v8toolkit;

struct DataMemberClass : public v8toolkit::WrappedClassBase {
    int a;
};

void wrap_DataMemberClass(v8toolkit::IsolatePtr isolate) {
    V8ClassWrapper<DataMemberClass> & wrapper = V8ClassWrapper<DataMemberClass>::get_instance(*isolate);
    wrapper.add_member<&DataMemberClass::a>("a");
    wrapper.add_member<&DataMemberClass::a>("b");
    wrapper.add_member<&DataMemberClass::a>("c");
    wrapper.add_member<&DataMemberClass::a>("d");
    wrapper.add_member<&DataMemberClass::a>("e");

};
#endif


#ifdef WRAP_5_DIFFERENT_DATA_MEMBERS_CLASS
#include "v8toolkit/javascript.h"
using namespace v8toolkit;

struct DataMemberClass : public v8toolkit::WrappedClassBase {
    int a;
    char b;
    float c;
    double d;
    unsigned e;
};

void wrap_DataMemberClass(v8toolkit::IsolatePtr isolate) {
    V8ClassWrapper<DataMemberClass> & wrapper = V8ClassWrapper<DataMemberClass>::get_instance(*isolate);
    wrapper.add_member<&DataMemberClass::a>("a");
    wrapper.add_member<&DataMemberClass::b>("b");
    wrapper.add_member<&DataMemberClass::c>("c");
    wrapper.add_member<&DataMemberClass::d>("d");
    wrapper.add_member<&DataMemberClass::e>("e");
};
#endif

#ifdef WRAP_5_DIFFERENT_DATA_MEMBERS_2_CLASSES
#include "v8toolkit/javascript.h"
using namespace v8toolkit;

struct DataMemberClass : public v8toolkit::WrappedClassBase {
    int a;
    char b;
    float c;
    double d;
    unsigned e;
};

struct DataMemberClass2 : public v8toolkit::WrappedClassBase {
    int a;
    char b;
    float c;
    double d;
    unsigned e;
};


void wrap_DataMemberClass(v8toolkit::IsolatePtr isolate) {
    {
        V8ClassWrapper<DataMemberClass> & wrapper = V8ClassWrapper<DataMemberClass>::get_instance(*isolate);
        wrapper.add_member<&DataMemberClass::a>("a");
        wrapper.add_member<&DataMemberClass::b>("b");
        wrapper.add_member<&DataMemberClass::c>("c");
        wrapper.add_member<&DataMemberClass::d>("d");
        wrapper.add_member<&DataMemberClass::e>("e");
    }
    {
        V8ClassWrapper<DataMemberClass2> & wrapper = V8ClassWrapper<DataMemberClass2>::get_instance(*isolate);
        wrapper.add_member<&DataMemberClass2::a>("a");
        wrapper.add_member<&DataMemberClass2::b>("b");
        wrapper.add_member<&DataMemberClass2::c>("c");
        wrapper.add_member<&DataMemberClass2::d>("d");
        wrapper.add_member<&DataMemberClass2::e>("e");
    }


};
#endif


#ifdef WRAP_VOID_VOID_MEMBER_FUNCTION_CLASS
#include "v8toolkit/javascript.h"
using namespace v8toolkit;

struct MemberFunctionClass : public v8toolkit::WrappedClassBase {
    void a();
};

void wrap_DataMemberClass(v8toolkit::IsolatePtr isolate) {
    V8ClassWrapper<MemberFunctionClass> & wrapper = V8ClassWrapper<MemberFunctionClass>::get_instance(*isolate);
    wrapper.add_method("a", &MemberFunctionClass::a);

};
#endif


#ifdef WRAP_MEMBER_FUNCTION_5_TIMES_CLASS
#include "v8toolkit/javascript.h"
using namespace v8toolkit;

struct MemberFunctionClass : public v8toolkit::WrappedClassBase {
    void a();
};

void wrap_DataMemberClass(v8toolkit::IsolatePtr isolate) {
    V8ClassWrapper<MemberFunctionClass> & wrapper = V8ClassWrapper<MemberFunctionClass>::get_instance(*isolate);
    wrapper.add_method("a", &MemberFunctionClass::a);
    wrapper.add_method("b", &MemberFunctionClass::a);
    wrapper.add_method("c", &MemberFunctionClass::a);
    wrapper.add_method("d", &MemberFunctionClass::a);
    wrapper.add_method("e", &MemberFunctionClass::a);

};
#endif


#ifdef WRAP_INT_VOID_MEMBER_FUNCTION_CLASS
#include "v8toolkit/javascript.h"
using namespace v8toolkit;

struct MemberFunctionClass : public v8toolkit::WrappedClassBase {
    int a();
};

void wrap_DataMemberClass(v8toolkit::IsolatePtr isolate) {
    V8ClassWrapper<MemberFunctionClass> & wrapper = V8ClassWrapper<MemberFunctionClass>::get_instance(*isolate);
    wrapper.add_method("a", &MemberFunctionClass::a);

};
#endif

#ifdef WRAP_5_DIFFERENT_MEMBER_FUNCTION_CLASS
#include "v8toolkit/javascript.h"
using namespace v8toolkit;

struct MemberFunctionClass : public v8toolkit::WrappedClassBase {
    int a(int);
    char b(char);
    float c(float);
    double d(double);
    unsigned e(unsigned);
};

void wrap_DataMemberClass(v8toolkit::IsolatePtr isolate) {
    V8ClassWrapper<MemberFunctionClass> & wrapper = V8ClassWrapper<MemberFunctionClass>::get_instance(*isolate);
    wrapper.add_method("a", &MemberFunctionClass::a);
    wrapper.add_method("b", &MemberFunctionClass::b);
    wrapper.add_method("c", &MemberFunctionClass::c);
    wrapper.add_method("d", &MemberFunctionClass::d);
    wrapper.add_method("e", &MemberFunctionClass::e);

};
#endif

#ifdef WRAP_5_DIFFERENT_MEMBER_FUNCTION_2_CLASSES
#include "v8toolkit/javascript.h"
using namespace v8toolkit;

struct MemberFunctionClass : public v8toolkit::WrappedClassBase {
    int a(int);
    char b(char);
    float c(float);
    double d(double);
    unsigned e(unsigned);
};

struct MemberFunctionClass2 : public v8toolkit::WrappedClassBase {
    int a(int);
    char b(char);
    float c(float);
    double d(double);
    unsigned e(unsigned);
};


void wrap_DataMemberClass(v8toolkit::IsolatePtr isolate) {
    V8ClassWrapper<MemberFunctionClass> & wrapper = V8ClassWrapper<MemberFunctionClass>::get_instance(*isolate);
    wrapper.add_method("a", &MemberFunctionClass::a);
    wrapper.add_method("b", &MemberFunctionClass::b);
    wrapper.add_method("c", &MemberFunctionClass::c);
    wrapper.add_method("d", &MemberFunctionClass::d);
    wrapper.add_method("e", &MemberFunctionClass::e);

    V8ClassWrapper<MemberFunctionClass2> & wrapper2 = V8ClassWrapper<MemberFunctionClass2>::get_instance(*isolate);
    wrapper2.add_method("a", &MemberFunctionClass2::a);
    wrapper2.add_method("b", &MemberFunctionClass2::b);
    wrapper2.add_method("c", &MemberFunctionClass2::c);
    wrapper2.add_method("d", &MemberFunctionClass2::d);
    wrapper2.add_method("e", &MemberFunctionClass2::e);

};
#endif

#ifdef WRAP_5_DIFFERENT_MEMBER_FUNCTION_10_CLASSES_OLD
#include "v8toolkit/javascript.h"
using namespace v8toolkit;

#define MAKE_CLASS(suffix) \
struct MemberFunctionClass##suffix : public v8toolkit::WrappedClassBase { \
    int a(int); \
    char b(char); \
    float c(float); \
    double d(double); \
    unsigned e(unsigned); \
};

MAKE_CLASS(1);
MAKE_CLASS(2);
MAKE_CLASS(3);
MAKE_CLASS(4);
MAKE_CLASS(5);
MAKE_CLASS(6);
MAKE_CLASS(7);
MAKE_CLASS(8);
MAKE_CLASS(9);
MAKE_CLASS(0);

#define MAKE_WRAPPER(suffix) \
{ \
    V8ClassWrapper<MemberFunctionClass##suffix> & wrapper = V8ClassWrapper<MemberFunctionClass##suffix>::get_instance(*isolate); \
    wrapper.add_method("a", &MemberFunctionClass##suffix::a); \
    wrapper.add_method("b", &MemberFunctionClass##suffix::b); \
    wrapper.add_method("c", &MemberFunctionClass##suffix::c); \
    wrapper.add_method("d", &MemberFunctionClass##suffix::d); \
    wrapper.add_method("e", &MemberFunctionClass##suffix::e); \
}

void wrap_DataMemberClass(v8toolkit::IsolatePtr isolate) {
    MAKE_WRAPPER(1);
    MAKE_WRAPPER(2);
    MAKE_WRAPPER(3);
    MAKE_WRAPPER(4);
    MAKE_WRAPPER(5);
    MAKE_WRAPPER(6);
    MAKE_WRAPPER(7);
    MAKE_WRAPPER(8);
    MAKE_WRAPPER(9);
    MAKE_WRAPPER(0);

};
#endif





#ifdef WRAP_5_DIFFERENT_MEMBER_FUNCTION_10_CLASSES_NEW
#include "v8toolkit/javascript.h"
using namespace v8toolkit;

#define MAKE_CLASS(suffix) \
struct MemberFunctionClass##suffix : public v8toolkit::WrappedClassBase { \
    int a(int); \
    char b(char); \
    float c(float); \
    double d(double); \
    unsigned e(unsigned); \
};

MAKE_CLASS(1);
MAKE_CLASS(2);
MAKE_CLASS(3);
MAKE_CLASS(4);
MAKE_CLASS(5);
MAKE_CLASS(6);
MAKE_CLASS(7);
MAKE_CLASS(8);
MAKE_CLASS(9);
MAKE_CLASS(0);

#define MAKE_WRAPPER(suffix) \
{ \
    V8ClassWrapper<MemberFunctionClass##suffix> & wrapper = V8ClassWrapper<MemberFunctionClass##suffix>::get_instance(*isolate); \
    wrapper.add_method<decltype(&MemberFunctionClass##suffix::a), &MemberFunctionClass##suffix::a>("a"); \
    wrapper.add_method<decltype(&MemberFunctionClass##suffix::b), &MemberFunctionClass##suffix::b>("b"); \
    wrapper.add_method<decltype(&MemberFunctionClass##suffix::c), &MemberFunctionClass##suffix::c>("c"); \
    wrapper.add_method<decltype(&MemberFunctionClass##suffix::d), &MemberFunctionClass##suffix::d>("d"); \
    wrapper.add_method<decltype(&MemberFunctionClass##suffix::e), &MemberFunctionClass##suffix::e>("e"); \
}

void wrap_DataMemberClass(v8toolkit::IsolatePtr isolate) {
    MAKE_WRAPPER(1);
    MAKE_WRAPPER(2);
    MAKE_WRAPPER(3);
    MAKE_WRAPPER(4);
    MAKE_WRAPPER(5);
    MAKE_WRAPPER(6);
    MAKE_WRAPPER(7);
    MAKE_WRAPPER(8);
    MAKE_WRAPPER(9);
    MAKE_WRAPPER(0);

};
#endif






#ifdef WRAP_10_DIFFERENT_MEMBER_FUNCTION_CLASS
#include "v8toolkit/javascript.h"
using namespace v8toolkit;

struct MemberFunctionClass : public v8toolkit::WrappedClassBase {
    int a(int);
    char b(char);
    float c(float);
    double d(double);
    unsigned e(unsigned);

    const int a2(int *);
    const char b2(char *);
    const float c2(float *);
    const double d2(double *);
    const unsigned e2(unsigned *);

};

void wrap_DataMemberClass(v8toolkit::IsolatePtr isolate) {
    V8ClassWrapper<MemberFunctionClass> & wrapper = V8ClassWrapper<MemberFunctionClass>::get_instance(*isolate);
    wrapper.add_method("a", &MemberFunctionClass::a);
    wrapper.add_method("b", &MemberFunctionClass::b);
    wrapper.add_method("c", &MemberFunctionClass::c);
    wrapper.add_method("d", &MemberFunctionClass::d);
    wrapper.add_method("e", &MemberFunctionClass::e);

    wrapper.add_method("a2", &MemberFunctionClass::a2);
    wrapper.add_method("b2", &MemberFunctionClass::b2);
    wrapper.add_method("c2", &MemberFunctionClass::c2);
    wrapper.add_method("d2", &MemberFunctionClass::d2);
    wrapper.add_method("e2", &MemberFunctionClass::e2);


};
#endif


#ifdef WRAP_COMPLEX_MEMBER_FUNCTION_CLASS
#include "v8toolkit/javascript.h"
using namespace v8toolkit;

struct MemberFunctionClass : public v8toolkit::WrappedClassBase {
    int a(int, char, float, double, unsigned, int, char, float, double, unsigned, int, char, float, double, unsigned, int, char, float, double, unsigned,
    int, char, float, double, unsigned, int, char, float, double, unsigned, int, char, float, double, unsigned, int, char, float, double, unsigned,
    int, char, float, double, unsigned, int, char, float, double, unsigned, int, char, float, double, unsigned, int, char, float, double, unsigned);
};

void wrap_DataMemberClass(v8toolkit::IsolatePtr isolate) {
    V8ClassWrapper<MemberFunctionClass> & wrapper = V8ClassWrapper<MemberFunctionClass>::get_instance(*isolate);
    wrapper.add_method("a", &MemberFunctionClass::a);

};
#endif