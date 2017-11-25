## V8 Toolkit Class Parser





### Config File 

For classes which cannot be annotated (such as third party code), a config file may be specified
to the class parser plugin to change the way the bindings are generated.  Attributes in this
file will always override annotations in code.  

The layout and supported attributes are shown below:

    {
        "classes": {
            "namespace::ClassName1": {
                // Name to wrap class as
                "name": "different_name_for_ClassName1",
                "member_functions": {
                    "member_function_signature_1": {
                        // whether to skip wrapping this function
                        "skip": true, 
                        
                        // name to wrap function as
                        "name": "different_name_for_member_function_name_1" 
                    },
                    "member_function_signature_2": {...}
                },
                "static_functions": {...}, // same as member_functions
                "data_members": {...} // same as member_functions
            }, 
            "namespace::ClassName2": {...}
        }
    }
    
The exact string representation is important and must match the internal presentation in class
parser.   It may be useful to view the generated log file to see the internally used names
in order to make sure to match them in your config file. 

