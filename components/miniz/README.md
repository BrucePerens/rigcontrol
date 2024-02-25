# Miniz made even mini-er.

This is taken from the Miniz github at https://github.com/richgel999/miniz
`tinfl_decompress_mem_to_callback()` function, as that's all this program needs.
The `tinfl_decompress()` function, which does the actual work of decompression,
is in the ESP32 on-chip ROM, as it's used for their FLASH functions.
