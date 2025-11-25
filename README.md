# dmramfs

DMOD RAM File System - an in-memory file system module for the DMOD framework.

## Overview

`dmramfs` is a RAM-based file system implementation that provides a fully featured, transient file system stored entirely in memory. It implements the DMOD File System Interface (DMFSI) and can be used with the DMOD Virtual File System (DMVFS).

## Features

- **In-Memory Storage**: All files and directories are stored in RAM for fast access
- **Full File Operations**: Support for read, write, seek, truncate, and append operations
- **Directory Support**: Create, list, and navigate directories
- **File Management**: Rename, delete, and get file statistics
- **DMFSI Compliant**: Implements the standard DMOD file system interface

## Building

```bash
mkdir build
cd build
cmake .. -DDMOD_MODE=DMOD_MODULE
cmake --build .
```

## Usage

The module can be loaded and mounted using DMVFS:

```c
#include "dmvfs.h"

// Initialize DMVFS
dmvfs_init(16, 32);

// Mount the RAM filesystem at /mnt
dmvfs_mount_fs("dmramfs", "/mnt", NULL);

// Use standard file operations
void* fp;
dmvfs_fopen(&fp, "/mnt/test.txt", DMFSI_O_CREAT | DMFSI_O_RDWR, 0, 0);
dmvfs_fwrite(fp, "Hello, World!", 13, NULL);
dmvfs_fclose(fp);

// Unmount when done
dmvfs_unmount_fs("/mnt");
dmvfs_deinit();
```

## API

The module implements the full DMFSI interface:

### File Operations
- `_fopen` - Open a file
- `_fclose` - Close a file
- `_fread` - Read from a file
- `_fwrite` - Write to a file
- `_lseek` - Seek to a position
- `_tell` - Get current position
- `_eof` - Check for end of file
- `_size` - Get file size
- `_getc` - Read a single character
- `_putc` - Write a single character
- `_sync` - Sync file (no-op for RAM FS)
- `_fflush` - Flush buffers (no-op for RAM FS)

### Directory Operations
- `_mkdir` - Create a directory
- `_opendir` - Open a directory for reading
- `_readdir` - Read directory entries
- `_closedir` - Close a directory
- `_direxists` - Check if directory exists

### File Management
- `_stat` - Get file/directory statistics
- `_unlink` - Delete a file
- `_rename` - Rename a file

## Testing

Tests are run using the `fs_tester` tool from the dmvfs repository:

```bash
# Build dmvfs with tests
cd /path/to/dmvfs
mkdir build && cd build
cmake .. -DDMOD_MODE=DMOD_SYSTEM -DDMVFS_BUILD_TESTS=ON
cmake --build .

# Run tests against dmramfs
./tests/fs_tester /path/to/dmramfs.dmf
```

## License

See [LICENSE](LICENSE) file for details.
