    
    // Class Parser Config File
    //   in xl-json format which allows comments, multi-line strings, and trailing commas
    
    
    // Format definition:
    
    {
        // settings associated with specific output modules
        "output_modules": {
            "BindingsOutputModule": {
                // default is 0 (unlimited)
                "max_declarations_per_file": 100 
            }
        },
        // The first bulk rename which matches will be used, so ordering within each
        //   subsection's array is important
        "bulk_renames": {
            "static_functions": [
                {
                    "regex": "regex_to_match(.*)",
                    "replace": "just_the_end$1"
                }
            ],
            "instance_members": [
                {
                    "regex": "regex_to_match(.*)",
                    "replace": "just_the_end$1"
                }
            ]
        },
        // override settings for how to handle functions and data members of wrapped types
        // Types which you control the definition for should be controlled with inline
        //    attributes.  This is mostly for third-party types with definitions which cannot
        //    be changed
        "classes": {
            "namespace::ClassName": {
                "members": {
                    "char * function_one() const &&": {
                        "skip": true,
                        "name": "different_name_for_function_two"
                    },
                    "void namespace::ClassName::static_function_name(int * &)": {
                        "skip": false,
                        "name": "different_name_for_this_static_function"
                    },
                    "int namespace::ClassName::data_member": {
                        "skip": true,
                        "name": "different_name_for_this_data_member"
                    },
                }
            }
        }
    }