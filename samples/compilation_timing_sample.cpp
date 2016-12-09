
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