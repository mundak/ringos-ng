#include <ringos/console.h>
#include <ringos/rpc.h>
#include <ringos/status.h>
#include <stddef.h>

int main(void)
{
  static const char sample_message[] = "hello from console service sample";
  ringos_console_device devices[4] = { 0 };
  ringos_console_get_info_response console_info = { 0 };
  ringos_handle channel_handle = RINGOS_HANDLE_INVALID;
  size_t device_count = 0;
  size_t bytes_written = 0;

  const int32_t query_status = ringos_console_query_devices(devices, 4, &device_count);

  if ((query_status != RINGOS_STATUS_OK && query_status != RINGOS_STATUS_BUFFER_TOO_SMALL) || device_count == 0)
  {
    return 1;
  }

  if (ringos_rpc_open(devices[0].endpoint_name, &channel_handle) != RINGOS_STATUS_OK)
  {
    return 1;
  }

  if (ringos_console_get_info(channel_handle, &console_info) != RINGOS_STATUS_OK)
  {
    return 1;
  }

  if ((console_info.capability_flags & RINGOS_CONSOLE_CAPABILITY_WRITE) == 0)
  {
    return 1;
  }

  if (
    ringos_console_write(channel_handle, sample_message, sizeof(sample_message) - 1, &bytes_written) != RINGOS_STATUS_OK
    || bytes_written != sizeof(sample_message) - 1)
  {
    return 1;
  }

  if (ringos_console_write(channel_handle, "\n", 1, &bytes_written) != RINGOS_STATUS_OK || bytes_written != 1)
  {
    return 1;
  }

  return 0;
}
