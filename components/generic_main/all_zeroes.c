#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
typedef uint32_t word;

bool
gm_all_zeroes(const void * address, size_t size)
{
  // Compare any leading unaligned bytes, until the address is word-aligned.
  if (size > sizeof(word)) {
    switch ((uintptr_t)address & (sizeof(word) - 1)) {
    case 0:
      // The address is already aligned.
      break;
    case 1:
      // There are three unaligned bytes before a word boundary.
      // Compare the leading byte, and then fall through.
      size--;
      if (*(const uint8_t *)address++)
        return false;
      /* FALLTHROUGH */
    case 2:
      // There is a 16-bit-aligned 16-bit datum left before a word boundary.
      // Comapre it, and then go on to the 32-bit-word-aligned compare routine.
      size -= 2;
      if (*(const uint16_t *)address++)
        return false;
      break;
    case 3:
      // There was only one unalinged byte before a word boundary.
      // Compare just one byte.
      size--;
      if (*(const uint8_t *)address++)
        return false;
    }
  }
  // Address is now word-aligned.
  // Compare words (32-bits at a time) if possible.
  if (size >= sizeof(word)) {
    const word * waddress = address;
    while (size != 0) {
      size -= sizeof(word);
      if (*waddress++)
        return false;
    }
    address = waddress;
  }
  // Compare any remaining bytes.
  // word-aligned.
  while (size != 0) {
    size--;
    if (*(const uint8_t *)address++)
      return false;
  }
  return true;
}
