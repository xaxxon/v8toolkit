#include <unistd.h>

#include "javascript.h"
using namespace v8toolkit;

#define THREAD_COUNT 3
#define SLEEP_TIME 1


void print_future(Context & context, std::future<std::pair<std::shared_ptr<Script>, v8::Global<v8::Value>>> & future)
{
    static bool first_time = true;
    if(first_time) {
        printf("Waiting on future, the first time this should take about %d seconds (not %d*%d=%d seconds since each sleep runs in parallel)\n", SLEEP_TIME, THREAD_COUNT, SLEEP_TIME, SLEEP_TIME * THREAD_COUNT);
        first_time = false;
    }
    
    // locker can't lock until after the future has completed (since it has the lock), so these two calls will both block
    //   until about the same time
    future.wait();
    
    // this can block if the isolate is currently locked in another async
    //    context::operator() must acquire the isolate lock that the second
    //    async on isoalte3 may still be using
    context([&](auto isolate){
        v8::Global<v8::Value> global_value = future.get().second;
        v8::Local<v8::Value> local_value = global_value.Get(context.get_isolate());
        v8::String::Utf8Value utf8value(local_value);
        printf("run async result: '%s'\n", *utf8value); 
    });
    
}


int main(int argc, char ** argv)
{
        
    Platform::init(argc, argv, argv[0]);
    auto isolate = Platform::create_isolate();
    isolate->add_function("sleep", [](int i){sleep(i);});
    isolate->add_print();
    auto c1 = isolate->create_context();
    
    auto isolate2 = Platform::create_isolate();
    isolate2->add_function("sleep", [](int i){sleep(i);});
    
    isolate2->add_print();
    auto c2 = isolate2->create_context();
    //
    auto isolate3 = Platform::create_isolate();
    isolate3->add_function("sleep", [](int i){sleep(i);});
    
    isolate3->add_print();
    auto c3 = isolate3->create_context();
    
    // some code that takes a while to run
    const char * code = "sleep(1); 3;";
        
    auto f1 = c1->run_async(code);
    auto f2 = c2->run_async(code);
    
    // these two will run sequentially
    auto f3 = c3->run_async(code); 
    auto f4 = c3->run_async(code);
     
    
    
    print_future(*c1, f1);
    print_future(*c2, f2);
    // expect a delay here as print_future and the second async on c3
    //   both want the isolate3 lock.
    print_future(*c3, f3);
    print_future(*c3, f4);
    
    printf("Done with futures, starting off detached thread now with 1 second sleep\n");
    printf("C++ thinks I can run %d threads concurrently\n", std::thread::hardware_concurrency());
    auto thread = c3->run_thread("sleep(1);println('This is running in the background and needs to be joined');");
    
    // wait for the thread to complete
    thread.join();

    printf("Spawning detached thread - need to make sure to wait for it to finish before exint main\n");
    c3->run_detached("println('This is running in the background in a detached thread');");
    // no way to know when the thread finishes, so just make sure to wait long enough
    // if main exits before the thread, the isolate and context used by the thread will be lost and the thread will crash
    sleep(1);
    printf("Done waiting for detached thread to complete\n");
    


    printf("Exiting main\n");
}