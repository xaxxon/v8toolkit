
#if defined V8HELPERS
#include "v8helpers.h"

#elif defined V8TOOLKIT
#include "v8toolkit.h"

#elif defined V8_CLASS_WRAPPER
#include "v8_class_wrapper_impl.h"

#elif defined JAVASCRIPT
#include "javascript.h"

#endif

#ifdef STD_FUNCTIONS
#include "javascript.h"
template class std::function<void()>;
template class std::function<int(int)>;
template class std::function<char(char)>;
template class std::function<float(float)>;
template class std::function<double(double)>;
#endif



#ifdef CREATE_CONTEXT
#include "javascript.h"
auto create_context() {
    auto isolate = v8toolkit::Platform::create_isolate();
    auto context = isolate->create_context();
    return context;
}
#endif


#ifdef WRAP_EMPTY_CLASS
#include "javascript.h"
class DataMemberClass {
};

void wrap_DataMemberClass(v8toolkit::IsolatePtr isolate) {
    v8toolkit::V8ClassWrapper<DataMemberClass>::get_instance(*isolate);
};
#endif

#ifdef WRAP_DATA_MEMBER_CLASS
#include "javascript.h"
using namespace v8toolkit;

struct DataMemberClass {
    int a;
};

void wrap_DataMemberClass(v8toolkit::IsolatePtr isolate) {
    V8ClassWrapper<DataMemberClass> & wrapper = V8ClassWrapper<DataMemberClass>::get_instance(*isolate);
    wrapper.add_member<int, DataMemberClass, &DataMemberClass::a>("a");

};
#endif


#ifdef WRAP_5_INT_DATA_MEMBERS_CLASS
#include "javascript.h"
using namespace v8toolkit;

struct DataMemberClass {
    int a;
};

void wrap_DataMemberClass(v8toolkit::IsolatePtr isolate) {
    V8ClassWrapper<DataMemberClass> & wrapper = V8ClassWrapper<DataMemberClass>::get_instance(*isolate);
    wrapper.add_member<int, DataMemberClass, &DataMemberClass::a>("a");
    wrapper.add_member<int, DataMemberClass, &DataMemberClass::a>("b");
    wrapper.add_member<int, DataMemberClass, &DataMemberClass::a>("c");
    wrapper.add_member<int, DataMemberClass, &DataMemberClass::a>("d");
    wrapper.add_member<int, DataMemberClass, &DataMemberClass::a>("e");

};
#endif


#ifdef WRAP_5_DIFFERENT_DATA_MEMBERS_CLASS
#include "javascript.h"
using namespace v8toolkit;

struct DataMemberClass {
    int a;
    char b;
    float c;
    double d;
    unsigned e;
};

void wrap_DataMemberClass(v8toolkit::IsolatePtr isolate) {
    V8ClassWrapper<DataMemberClass> & wrapper = V8ClassWrapper<DataMemberClass>::get_instance(*isolate);
    wrapper.add_member<int, DataMemberClass, &DataMemberClass::a>("a");
    wrapper.add_member<char, DataMemberClass, &DataMemberClass::b>("b");
    wrapper.add_member<float, DataMemberClass, &DataMemberClass::c>("c");
    wrapper.add_member<double, DataMemberClass, &DataMemberClass::d>("d");
    wrapper.add_member<unsigned, DataMemberClass, &DataMemberClass::e>("e");
};
#endif

#ifdef WRAP_5_DIFFERENT_DATA_MEMBERS_2_CLASSES
#include "javascript.h"
using namespace v8toolkit;

struct DataMemberClass {
    int a;
    char b;
    float c;
    double d;
    unsigned e;
};

struct DataMemberClass2 {
    int a;
    char b;
    float c;
    double d;
    unsigned e;
};


void wrap_DataMemberClass(v8toolkit::IsolatePtr isolate) {
    {
        V8ClassWrapper<DataMemberClass> & wrapper = V8ClassWrapper<DataMemberClass>::get_instance(*isolate);
        wrapper.add_member<int, DataMemberClass, &DataMemberClass::a>("a");
        wrapper.add_member<char, DataMemberClass, &DataMemberClass::b>("b");
        wrapper.add_member<float, DataMemberClass, &DataMemberClass::c>("c");
        wrapper.add_member<double, DataMemberClass, &DataMemberClass::d>("d");
        wrapper.add_member<unsigned, DataMemberClass, &DataMemberClass::e>("e");
    }
    {
        V8ClassWrapper<DataMemberClass2> & wrapper = V8ClassWrapper<DataMemberClass2>::get_instance(*isolate);
        wrapper.add_member<int, DataMemberClass2, &DataMemberClass2::a>("a");
        wrapper.add_member<char, DataMemberClass2, &DataMemberClass2::b>("b");
        wrapper.add_member<float, DataMemberClass2, &DataMemberClass2::c>("c");
        wrapper.add_member<double, DataMemberClass2, &DataMemberClass2::d>("d");
        wrapper.add_member<unsigned, DataMemberClass2, &DataMemberClass2::e>("e");
    }


};
#endif


#ifdef WRAP_VOID_VOID_MEMBER_FUNCTION_CLASS
#include "javascript.h"
using namespace v8toolkit;

struct MemberFunctionClass {
    void a();
};

void wrap_DataMemberClass(v8toolkit::IsolatePtr isolate) {
    V8ClassWrapper<MemberFunctionClass> & wrapper = V8ClassWrapper<MemberFunctionClass>::get_instance(*isolate);
    wrapper.add_method("a", &MemberFunctionClass::a);

};
#endif


#ifdef WRAP_MEMBER_FUNCTION_5_TIMES_CLASS
#include "javascript.h"
using namespace v8toolkit;

struct MemberFunctionClass {
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
#include "javascript.h"
using namespace v8toolkit;

struct MemberFunctionClass {
    int a();
};

void wrap_DataMemberClass(v8toolkit::IsolatePtr isolate) {
    V8ClassWrapper<MemberFunctionClass> & wrapper = V8ClassWrapper<MemberFunctionClass>::get_instance(*isolate);
    wrapper.add_method("a", &MemberFunctionClass::a);

};
#endif

#ifdef WRAP_5_DIFFERENT_MEMBER_FUNCTION_CLASS
#include "javascript.h"
using namespace v8toolkit;

struct MemberFunctionClass {
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
#include "javascript.h"
using namespace v8toolkit;

struct MemberFunctionClass {
    int a(int);
    char b(char);
    float c(float);
    double d(double);
    unsigned e(unsigned);
};

struct MemberFunctionClass2 {
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


#ifdef WRAP_10_DIFFERENT_MEMBER_FUNCTION_CLASS
#include "javascript.h"
using namespace v8toolkit;

struct MemberFunctionClass {
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
#include "javascript.h"
using namespace v8toolkit;

struct MemberFunctionClass {
    int a(int, char, float, double, unsigned, int, char, float, double, unsigned, int, char, float, double, unsigned, int, char, float, double, unsigned,
    int, char, float, double, unsigned, int, char, float, double, unsigned, int, char, float, double, unsigned, int, char, float, double, unsigned,
    int, char, float, double, unsigned, int, char, float, double, unsigned, int, char, float, double, unsigned, int, char, float, double, unsigned);
};

void wrap_DataMemberClass(v8toolkit::IsolatePtr isolate) {
    V8ClassWrapper<MemberFunctionClass> & wrapper = V8ClassWrapper<MemberFunctionClass>::get_instance(*isolate);
    wrapper.add_method("a", &MemberFunctionClass::a);

};
#endif