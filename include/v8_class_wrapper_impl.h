
#pragma once



#include <algorithm>
#include <string>

#include "v8_class_wrapper.h"

namespace v8toolkit {


    template<class T>
	V8ClassWrapper<T, V8TOOLKIT_V8CLASSWRAPPER_USE_REAL_TEMPLATE_SFINAE>::V8ClassWrapper(v8::Isolate * isolate) : isolate(isolate) {
		this->isolate_to_wrapper_map.emplace(isolate, this);
	}

    template<class T> void
	V8ClassWrapper<T, V8TOOLKIT_V8CLASSWRAPPER_USE_REAL_TEMPLATE_SFINAE>::call_callbacks(v8::Local<v8::Object> object, const std::string & property_name, v8::Local<v8::Value> & value) {
		for (auto &callback : attribute_callbacks) {
			callback(isolate, object, property_name, value);
		}
	}


    template<class T> void
	V8ClassWrapper<T, V8TOOLKIT_V8CLASSWRAPPER_USE_REAL_TEMPLATE_SFINAE>::check_if_name_used(const std::string & name) {
      if (std::find(used_attribute_name_list.begin(),
		    used_attribute_name_list.end(),
		    name) != used_attribute_name_list.end()) {

  	  throw DuplicateNameException(fmt::format("Cannot add method/member named '{}' to class '{}', name already in use", name, class_name));
      }
      used_attribute_name_list.push_back(name);
    }

	template<class T> void
	V8ClassWrapper<T, V8TOOLKIT_V8CLASSWRAPPER_USE_REAL_TEMPLATE_SFINAE>::check_if_static_name_used(const std::string & name) {
		if (std::find(used_static_attribute_name_list.begin(),
					  used_static_attribute_name_list.end(),
					  name) != used_static_attribute_name_list.end()) {

			throw DuplicateNameException(fmt::format("Cannot add static method named '{}' to class '{}', name already in use", name, class_name));
		}
		used_static_attribute_name_list.push_back(name);
	}



	// takes a Data() parameter of a StdFunctionCallbackType lambda and calls it
    //   Useful because capturing lambdas don't have a traditional function pointer type
    template<class T> void
	V8ClassWrapper<T, V8TOOLKIT_V8CLASSWRAPPER_USE_REAL_TEMPLATE_SFINAE>::callback_helper(const v8::FunctionCallbackInfo<v8::Value>& args) {
	    StdFunctionCallbackType * callback_lambda = (StdFunctionCallbackType *)v8::External::Cast(*(args.Data()))->Value();		
	    (*callback_lambda)(args);
    }
    
    
    template<class T> void
	V8ClassWrapper<T, V8TOOLKIT_V8CLASSWRAPPER_USE_REAL_TEMPLATE_SFINAE>::register_callback(AttributeChangeCallback callback) {
	attribute_callbacks.push_back(callback);
    }
  
	
	
    /**
     * Creates a new v8::FunctionTemplate capable of creating wrapped T objects based on previously added methods and members.
     * TODO: This needs to track all FunctionTemplates ever created so it can try to use them in GetInstanceByPrototypeChain
     */
    template<class T>
	v8::Local<v8::FunctionTemplate>
	V8ClassWrapper<T, V8TOOLKIT_V8CLASSWRAPPER_USE_REAL_TEMPLATE_SFINAE>::make_wrapping_function_template(v8::FunctionCallback callback,
												     const v8::Local<v8::Value> & data) {
		assert(this->finalized == true);

//	fprintf(stderr, "Making new wrapping function template for type %s\n", demangle<T>().c_str());

		auto function_template = v8::FunctionTemplate::New(isolate, callback, data);
		init_instance_object_template(function_template->InstanceTemplate());
		init_prototype_object_template(function_template->PrototypeTemplate());
		init_static_methods(function_template);

		function_template->SetClassName(v8::String::NewFromUtf8(isolate, class_name.c_str()));



		// if there is a parent type set, set that as this object's prototype
		auto parent_function_template = global_parent_function_template.Get(isolate);
		if (!parent_function_template.IsEmpty()) {
	    fprintf(stderr, "FOUND PARENT TYPE of %s, USING ITS PROTOTYPE AS PARENT PROTOTYPE\n", demangle<T>().c_str());
			function_template->Inherit(parent_function_template);
		}

		for (auto callback : this->function_template_callbacks) {
			callback(function_template);
		}

//	fprintf(stderr, "Adding this_class_function_template for %s\n", demangle<T>().c_str());
		this_class_function_templates.emplace_back(v8::Global<v8::FunctionTemplate>(isolate, function_template));
		return function_template;
	}

    /**
     * Returns an existing constructor function template for the class/isolate OR creates one if none exist.
     *   This is to keep the number of constructor function templates as small as possible because looking up
     *   which one created an object takes linear time based on the number that exist
     */
    template<class T> v8::Local<v8::FunctionTemplate>
	V8ClassWrapper<T, V8TOOLKIT_V8CLASSWRAPPER_USE_REAL_TEMPLATE_SFINAE>::get_function_template()
	{
	    if (this_class_function_templates.empty()){
//		fprintf(stderr, "Making function template because there isn't one %s\n", demangle<T>().c_str());
		// this will store it for later use automatically
		return make_wrapping_function_template();
	    } else {
		// fprintf(stderr, "Not making function template because there is already one %s\n", demangle<T>().c_str());
		// return an arbitrary one, since they're all the same when used to call .NewInstance()
		return this_class_function_templates[0].Get(isolate);
	    }
	}


    /**
     * Returns the embedded native object inside a javascript object or throws if none is present or its type cannot
     *   be determined
     */
    template<class T>  T *
	V8ClassWrapper<T, V8TOOLKIT_V8CLASSWRAPPER_USE_REAL_TEMPLATE_SFINAE>::get_cpp_object(v8::Local<v8::Object> object) {

        if (object->InternalFieldCount() == 0) {
            throw CastException("Tried to get internal field from object with no internal fields");
        } else if (object->InternalFieldCount() > 1) {
            throw CastException(
                    "Tried to get internal field from object with more than one internal fields - this is not supported by v8toolkit");
        }

        auto wrap = v8::Local<v8::External>::Cast(object->GetInternalField(0));
        WrappedData<T> *wrapped_data = static_cast<WrappedData<T> *>(wrap->Value());
        if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "uncasted internal field: %p\n", wrapped_data->native_object);
        return this->cast(static_cast<AnyBase *>(wrapped_data->native_object));
    }



    /**
     * Check to see if an object can be converted to type T, else return nullptr
     */
    template<class T>   T *
	V8ClassWrapper<T, V8TOOLKIT_V8CLASSWRAPPER_USE_REAL_TEMPLATE_SFINAE>::cast(AnyBase * any_base)
	{
	    if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "In ClassWrapper::cast for type %s\n", demangle<T>().c_str());
	    if(type_checker != nullptr) {
		if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "Explicit compatible types set, using that\n");
		return type_checker->check(any_base);
	    } else if (dynamic_cast<AnyPtr<T>*>(any_base)) {
		assert(false); // should not use this code path anymore
		if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "No explicit compatible types, but successfully cast to self-type\n");
		return static_cast<AnyPtr<T>*>(any_base)->get();
	    }
	    // if it's already not const, it's ok to run it again
	    else if (dynamic_cast<AnyPtr<std::remove_const_t<T>>*>(any_base)) {
		return static_cast<AnyPtr<std::remove_const_t<T>>*>(any_base)->get();
	    }

	    throw CastException("Could not determine type of object in V8ClassWrapper::cast().  Define ANYBASE_DEBUG for more information");
	}


    template<class T>  void
	V8ClassWrapper<T, V8TOOLKIT_V8CLASSWRAPPER_USE_REAL_TEMPLATE_SFINAE>::init_instance_object_template(v8::Local<v8::ObjectTemplate> object_template) {
		object_template->SetInternalFieldCount(1);
//	fprintf(stderr, "Adding %d members\n", (int)this->member_adders.size());
		for (auto &adder : this->member_adders) {
			adder(object_template);
		}
		// if this is set, it allows the object returned from a 'new' call to be used as a function as well as a traditional object
		//   e.g. let my_object = new MyClass(); my_object();
		if (callable_adder.callback) {
			object_template->SetCallAsFunctionHandler(callback_helper,
													  v8::External::New(this->isolate, &callable_adder.callback));
		}
	}


    template<class T> void V8ClassWrapper<T, V8TOOLKIT_V8CLASSWRAPPER_USE_REAL_TEMPLATE_SFINAE>::
	init_prototype_object_template(v8::Local<v8::ObjectTemplate> object_template) {
//	    fprintf(stderr, "Adding %d methods\n", (int)this->method_adders.size());
		for (auto &adder : this->method_adders) {

			std::cerr << fmt::format("Class: {} adding method: {}", demangle<T>(), adder.method_name) << std::endl;

			// create a function template, set the lambda created above to be the handler
			auto function_template = v8::FunctionTemplate::New(this->isolate);
			function_template->SetCallHandler(callback_helper, v8::External::New(this->isolate, &adder.callback));

			// methods are put into the protype of the newly created javascript object
			object_template->Set(v8::String::NewFromUtf8(isolate, adder.method_name.c_str()), function_template);
		}
		for (auto &adder : this->fake_method_adders) {
			adder(object_template);
		}

		if (this->indexed_property_getter) {
			object_template->SetIndexedPropertyHandler(this->indexed_property_getter);
		}
	}

    template<class T>  void
	V8ClassWrapper<T, V8TOOLKIT_V8CLASSWRAPPER_USE_REAL_TEMPLATE_SFINAE>::init_static_methods(v8::Local<v8::FunctionTemplate> constructor_function_template) {
//	fprintf(stderr, "Adding %d static methods\n", (int)this->static_method_adders.size());
		for (auto &adder : this->static_method_adders) {
			adder(constructor_function_template);
		}
	}




    /**
     * Returns a "singleton-per-isolate" instance of the V8ClassWrapper for the wrapped class type.
     * For each isolate you need to add constructors/methods/members separately.
     */
//
//    template<class T>   	 V8ClassWrapper<T> &
//	V8ClassWrapper<T, V8TOOLKIT_V8CLASSWRAPPER_USE_REAL_TEMPLATE_SFINAE>::get_instance(v8::Isolate * isolate)
//	{
//	    if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "isolate to wrapper map %p size: %d\n", &isolate_to_wrapper_map, (int)isolate_to_wrapper_map.size());
//
//        if (isolate_to_wrapper_map.find(isolate) == isolate_to_wrapper_map.end()) {
//		    auto new_object = new V8ClassWrapper<T>(isolate);
//		    if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "Creating instance %p for isolate: %p\n", new_object, isolate);
//	    }
//	    if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "(after) isolate to wrapper map size: %d\n", (int)isolate_to_wrapper_map.size());
//
//	    auto object = isolate_to_wrapper_map[isolate];
//	    if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "Returning v8 wrapper: %p\n", object);
//	    return *object;
//	}
//

    /**
     * V8ClassWrapper objects shouldn't be deleted during the normal flow of your program unless the associated isolate
     *   is going away forever.   Things will break otherwise as no additional objects will be able to be created
     *   even though V8 will still present the ability to your javascript (I think)
     */
    template<class T>
	V8ClassWrapper<T, V8TOOLKIT_V8CLASSWRAPPER_USE_REAL_TEMPLATE_SFINAE>::~V8ClassWrapper()
	{
	    // this was happening when it wasn't supposed to, like when making temp copies.   need to disable copying or something
	    //   if this line is to be added back
	    // isolate_to_wrapper_map.erase(this->isolate);
	}


    template<class T>  V8ClassWrapper<T> &
	V8ClassWrapper<T, V8TOOLKIT_V8CLASSWRAPPER_USE_REAL_TEMPLATE_SFINAE>::set_class_name(const std::string & name){
	assert(!this->finalized);
	this->class_name = name;
	return *this;
    }


    
    /**
     * Function to force API user to declare that all members/methods have been added before any
     *   objects of the wrapped type can be created to make sure everything stays consistent
     * Must be called before adding any constructors or using wrap_existing_object()
     */
    template<class T>  V8ClassWrapper<T> &
	V8ClassWrapper<T, V8TOOLKIT_V8CLASSWRAPPER_USE_REAL_TEMPLATE_SFINAE>::finalize(bool wrap_as_most_derived_flag) {
        if (this->finalized) {
            throw V8Exception(this->isolate, "Called ::finalize on wrapper that was already finalized");
        }
        if (!std::is_const<T>::value) {
            V8ClassWrapper<std::add_const_t<T>>::get_instance(isolate).finalize();
        }
	    this->wrap_as_most_derived_flag = wrap_as_most_derived_flag;
        this->finalized = true;
        get_function_template(); // force creation of a function template that doesn't call v8_constructor
        return *this;
    }


    /**
     * returns whether finalize() has been called on this type for this isolate
     */
//    template<class T>  bool
//	V8ClassWrapper<T, V8TOOLKIT_V8CLASSWRAPPER_USE_REAL_TEMPLATE_SFINAE>::is_finalized()
//	{
//	    return this->finalized;
//	}


} // end namespace v8toolkit
