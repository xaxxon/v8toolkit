
### Class Parser Config File

Format definition:

{
	"classes": {
        "namespace::ClassName": {
            "members": {
                "char * function_one() const &&": {
                    "skip": true,
                    "name": "different_name_for_function_two"
                }, {
                "void namespace::ClassName::static_function_name(int * &)": {
                    "skip": false,
                    "name": "different_name_for_this_static_function"
                },
                "int namespace::ClassName::data_member": {
                    "skip": true,
                    "name": "different_name_for_this_data_member"
                }
            }
        }
	}
}