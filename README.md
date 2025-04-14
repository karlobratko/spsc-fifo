# SPSC FIFO

A single-header, lock-free FIFO queue designed specifically for single-producer/single-consumer scenarios, such as thread pipeline patterns.

## Features

- **Lock-free operation**: No mutexes or critical sections
- **Memory efficient**: Minimizes padding and allocations
- **Cache friendly**: Designed to avoid false sharing
- **Customizable**: Configure alignment, memory allocation, assertions
- **Thread safety debugging**: Optional thread binding validation (Unix-like systems)
- **Zero dependencies**: Standalone implementation with no external requirements

## Usage

1. Include the header in your project
2. Define `SPSC_FIFO_IMPLEMENTATION` in exactly one source file

## Configuration Options

Define these before including the header to customize behavior:

- `SPSC_FIFO_STATIC`: Make all functions static
- `SPSC_FIFO_NDEBUG`: Disable thread safety debugging
- `SPSC_FIFO_ASSERT(expr)`: Override assert implementation
- `SPSC_FIFO_ALLOC(sz)`: Override memory allocation
- `SPSC_FIFO_FREE(p)`: Override memory deallocation
- `SPSC_FIFO_CACHE_LINE_SIZE`: Set cache line size (default: 64 bytes)
- `SPSC_FIFO_DEFAULT_BUF_ALIGNMENT`: Set default buffer alignment

## API

### Allocation and Management

- `spsc_fifo_alloc`: Allocate FIFO with default alignment
- `spsc_fifo_aligned_alloc`: Allocate FIFO with custom buffer alignment
- `spsc_fifo_free`: Free FIFO resources
- `spsc_fifo_reset`: Reset FIFO to empty state

### Producer Functions

- `spsc_fifo_write`: Write data (partial writes allowed)
- `spsc_fifo_write_n`: Write exact amount or nothing
- `spsc_fifo_write_avail`: Get available write space
- `spsc_fifo_is_full`: Check if FIFO is full

### Consumer Functions

- `spsc_fifo_read`: Read data (partial reads allowed)
- `spsc_fifo_read_n`: Read exact amount or nothing
- `spsc_fifo_peek`: View data without removing
- `spsc_fifo_peek_n`: View exact amount or nothing
- `spsc_fifo_skip`: Skip data (partial skips allowed)
- `spsc_fifo_skip_n`: Skip exact amount or nothing
- `spsc_fifo_read_avail`: Get available data
- `spsc_fifo_is_empty`: Check if FIFO is empty

## License

MIT License. See the [LICENSE](./LICENSE) file for details.