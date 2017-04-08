

## Build Instructions


in v8 directory:

`./tools/dev/v8gen.py <TARGET>`

To list all available targets:
`./tools/dev/v8gen.py list`


Then, to set additional options, edit `./out.gn/<TARGET>/args.gn`

Whether to build shared libraries or not
`is_component_build=true`

 build with debug or not (already set if you selected a debug target above
`is_debug=true`

Whether to require snapshots at runtime
`v8_use_snapshot=false`


Once configured as desired, build with:
`ninja -C out.gn/x64.release`

or only exactly what you want (can be a bit faster)

`ninja -C out.gn/x64.release libv8.so`

