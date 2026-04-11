# Console RPC Write Sample

This sample is kept as a standalone SDK example even though the kernel no longer exposes a console runtime.

The source still demonstrates the original named-RPC console flow:

- `ringos_console_query_devices()` asks the system which console RPC endpoints are available
- `ringos_rpc_open(endpoint_name, ...)` opens the selected named RPC endpoint and returns its channel handle
- `ringos_console_write()` sends the write request to the selected console endpoint

The local Windows batch wrapper now validates that the sample still compiles for the supported target architectures. It
does not boot a console-enabled kernel image, because that runtime path has been removed.
