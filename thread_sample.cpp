#include "javascript.h"
using namespace v8toolkit;
using namespace v8;

#define THREAD_COUNT 3
#define SLEEP_TIME 1


void print_future(ContextHelper & context, std::future<Global<Value>> & future)
{
	static bool first_time = true;
	if(first_time) {
		printf("Waiting on future, the first time this should take about %d seconds (not %d*%d=%d seconds since each sleep runs in parallel)\n", SLEEP_TIME, THREAD_COUNT, SLEEP_TIME, SLEEP_TIME * THREAD_COUNT);
		first_time = false;
	}
	
	// locker can't lock until after the future has completed (since it has the lock), so these two calls will both block
	//   until about the same time
	future.wait();
	
	context([&](auto isolate){
		
		Global<Value> global_value = future.get();
		Local<Value> local_value = global_value.Get(context.get_isolate());
		v8::String::Utf8Value utf8value(local_value);
		printf("run async result: '%s'\n", *utf8value);	
	});
	
}


int main(int argc, char ** argv)
{
	PlatformHelper::init(argc, argv);
	auto isolate = PlatformHelper::create_isolate();
	isolate->add_function("sleep", [](int i){sleep(i);});
	isolate->add_print();
	auto c1 = isolate->create_context();
	
	auto isolate2 = PlatformHelper::create_isolate();
	isolate2->add_function("sleep", [](int i){sleep(i);});
	
	isolate2->add_print();
	auto c2 = isolate2->create_context();
	//
	auto isolate3 = PlatformHelper::create_isolate();
	isolate3->add_function("sleep", [](int i){sleep(i);});
	
	isolate3->add_print();
	auto c3 = isolate3->create_context();
	
	// some code that takes a while to run
	const char * code = "sleep(1); 3;";
		
	auto f1 = c1->run_async(code);
	auto f2 = c2->run_async(code);
	auto f3 = c3->run_async(code);
	
	
	print_future(*c1, f1);
	print_future(*c2, f2);
	print_future(*c3, f3);
}