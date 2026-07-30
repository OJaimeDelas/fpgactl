#ifndef FPGA_TYPES_H
#define FPGA_TYPES_H

/*
 * Basic types and status codes for board-agnostic code.
 *
 * The typedefs are identical to the ones in Xilinx xil_types.h so that
 * board-side implementation files may include both headers.
 */

#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t  s32;
typedef int64_t  s64;
typedef uint64_t u64;

/* Status codes returned by every layer (0 = success, matching XST_SUCCESS) */
#define FPGA_OK     0
#define FPGA_ERROR  1

#endif /* FPGA_TYPES_H */
