#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Compute CRC-32 (IEEE 802.3, polynomial 0xEDB88320) over a buffer.
 *
 * Bitwise implementation (no lookup table). Standard reflected CRC-32
 * with initial value 0xFFFFFFFF and final XOR 0xFFFFFFFF.
 *
 * @param data Pointer to input bytes (may be NULL if len == 0).
 * @param len  Number of bytes.
 * @return     CRC-32 checksum.
 */
uint32_t crc32_compute(const void *data, size_t len);

#endif /* CRC32_H */
