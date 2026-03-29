#include <stdio.h>

#include <ringos/debug.h>

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#define RINGOS_STDIO_LOG_BUFFER_CAPACITY ((size_t) 512)

namespace
{
  enum class format_length : uint8_t
  {
    default_value,
    long_value,
    long_long_value,
    size_value
  };

  void append_character(char** cursor, size_t* remaining, int* total_written, char value)
  {
    if (*remaining > 1)
    {
      **cursor = value;
      ++(*cursor);
    }

    if (*remaining > 0)
    {
      --(*remaining);
    }

    ++(*total_written);
  }

  void append_string(char** cursor, size_t* remaining, int* total_written, const char* value)
  {
    const char* current = value != nullptr ? value : "(null)";

    while (*current != '\0')
    {
      append_character(cursor, remaining, total_written, *current);
      ++current;
    }
  }

  void append_unsigned_value(
    char** cursor, size_t* remaining, int* total_written, uint64_t value, uint32_t base, bool uppercase)
  {
    char digits[32];
    size_t digit_count = 0;
    const char* alphabet = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

    if (value == 0)
    {
      append_character(cursor, remaining, total_written, '0');
      return;
    }

    while (value != 0)
    {
      digits[digit_count] = alphabet[value % base];
      value /= base;
      ++digit_count;
    }

    while (digit_count > 0)
    {
      --digit_count;
      append_character(cursor, remaining, total_written, digits[digit_count]);
    }
  }

  void append_signed_value(char** cursor, size_t* remaining, int* total_written, int64_t value)
  {
    uint64_t magnitude = static_cast<uint64_t>(value);

    if (value < 0)
    {
      append_character(cursor, remaining, total_written, '-');
      magnitude = static_cast<uint64_t>(-(value + 1)) + 1;
    }

    append_unsigned_value(cursor, remaining, total_written, magnitude, 10, false);
  }

  format_length parse_format_length(const char** format_cursor)
  {
    if (**format_cursor == 'l')
    {
      ++(*format_cursor);

      if (**format_cursor == 'l')
      {
        ++(*format_cursor);
        return format_length::long_long_value;
      }

      return format_length::long_value;
    }

    if (**format_cursor == 'z')
    {
      ++(*format_cursor);
      return format_length::size_value;
    }

    return format_length::default_value;
  }

  uint64_t read_unsigned_argument(va_list arguments, format_length length)
  {
    switch (length)
    {
    case format_length::long_value:
      return va_arg(arguments, unsigned long);
    case format_length::long_long_value:
      return va_arg(arguments, unsigned long long);
    case format_length::size_value:
      return va_arg(arguments, size_t);
    case format_length::default_value:
    default:
      return va_arg(arguments, unsigned int);
    }
  }

  int64_t read_signed_argument(va_list arguments, format_length length)
  {
    switch (length)
    {
    case format_length::long_value:
      return va_arg(arguments, long);
    case format_length::long_long_value:
      return va_arg(arguments, long long);
    case format_length::size_value:
      return static_cast<int64_t>(va_arg(arguments, ptrdiff_t));
    case format_length::default_value:
    default:
      return va_arg(arguments, int);
    }
  }
}

int printf(const char* format, ...)
{
  va_list arguments;
  va_start(arguments, format);
  const int result = vprintf(format, arguments);
  va_end(arguments);
  return result;
}

int puts(const char* string)
{
  if (string == nullptr)
  {
    errno = EINVAL;
    return -1;
  }

  return ringos_debug_log(string);
}

int snprintf(char* buffer, size_t buffer_size, const char* format, ...)
{
  va_list arguments;
  va_start(arguments, format);
  const int result = vsnprintf(buffer, buffer_size, format, arguments);
  va_end(arguments);
  return result;
}

int vprintf(const char* format, va_list arguments)
{
  char buffer[RINGOS_STDIO_LOG_BUFFER_CAPACITY];
  va_list argument_copy;
  va_copy(argument_copy, arguments);
  const int result = vsnprintf(buffer, sizeof(buffer), format, argument_copy);
  va_end(argument_copy);

  if (result < 0)
  {
    return result;
  }

  (void) ringos_debug_log(buffer);
  return result;
}

int vsnprintf(char* buffer, size_t buffer_size, const char* format, va_list arguments)
{
  if (format == nullptr || (buffer == nullptr && buffer_size != 0))
  {
    errno = EINVAL;
    return -1;
  }

  char* cursor = buffer;
  size_t remaining = buffer_size;
  int total_written = 0;
  const char* format_cursor = format;

  while (*format_cursor != '\0')
  {
    if (*format_cursor != '%')
    {
      append_character(&cursor, &remaining, &total_written, *format_cursor);
      ++format_cursor;
      continue;
    }

    ++format_cursor;

    if (*format_cursor == '%')
    {
      append_character(&cursor, &remaining, &total_written, '%');
      ++format_cursor;
      continue;
    }

    const format_length length = parse_format_length(&format_cursor);
    const char specifier = *format_cursor;

    switch (specifier)
    {
    case 'c':
      append_character(&cursor, &remaining, &total_written, static_cast<char>(va_arg(arguments, int)));
      break;
    case 'd':
    case 'i':
      append_signed_value(&cursor, &remaining, &total_written, read_signed_argument(arguments, length));
      break;
    case 'p':
      append_string(&cursor, &remaining, &total_written, "0x");
      append_unsigned_value(
        &cursor,
        &remaining,
        &total_written,
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(va_arg(arguments, void*))),
        16,
        false);
      break;
    case 's':
      append_string(&cursor, &remaining, &total_written, va_arg(arguments, const char*));
      break;
    case 'u':
      append_unsigned_value(&cursor, &remaining, &total_written, read_unsigned_argument(arguments, length), 10, false);
      break;
    case 'x':
      append_unsigned_value(&cursor, &remaining, &total_written, read_unsigned_argument(arguments, length), 16, false);
      break;
    case 'X':
      append_unsigned_value(&cursor, &remaining, &total_written, read_unsigned_argument(arguments, length), 16, true);
      break;
    case '\0':
      --format_cursor;
      append_character(&cursor, &remaining, &total_written, '%');
      break;
    default:
      append_character(&cursor, &remaining, &total_written, '%');
      append_character(&cursor, &remaining, &total_written, specifier);
      break;
    }

    if (*format_cursor != '\0')
    {
      ++format_cursor;
    }
  }

  if (buffer_size != 0)
  {
    if (remaining > 0)
    {
      *cursor = '\0';
    }
    else
    {
      buffer[buffer_size - 1] = '\0';
    }
  }

  return total_written;
}

