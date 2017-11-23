
### Class Parser Config File

Format definition:

{
	"classes": {
		"member_functions": {
			"xl::AlignedStringBuffer_Dynamic<16>": {
				"member_functions": {
					"char * function_one() const &&": {
						"skip": true,
						"name": "different_name_for_function_two"
					}
				},
				static_functions: {
                    "char * static_function(int)": {
                        "skip": false,
                        "name": "different_name_static_function"
                    }
				},
				data_members: {
				    "SomeClass::data_member": {
				        "skip": true
				        "name": "different_name_data_member"
				    }
				}
			}
		}
	}
}