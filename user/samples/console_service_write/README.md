# Console RPC Write Sample

The sample exercises the named-RPC console flow directly:

- `ringos_console_query_devices()` asks the system which console RPC endpoints are available
- `ringos_rpc_open(endpoint_name, ...)` opens the selected named RPC endpoint and returns its channel handle
- `ringos_console_write()` sends the write request to the architecture-specific console driver through that handle
- the driver forwards the bytes to its hardware backend
