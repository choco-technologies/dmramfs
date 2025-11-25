#define DMOD_ENABLE_REGISTRATION    ON
#define ENABLE_DIF_REGISTRATIONS    ON
#include "dmod.h"
#include "dmramfs.h"
#include "dmfsi.h"
#include "dmlist.h"
#include <string.h>

/** 
 * @brief Magic number for RAMFS context validation
 */
#define DMRAMFS_CONTEXT_MAGIC 0x52414D46  // 'RAMF'

/** 
 * @brief File structure
 */
typedef struct 
{
    char* file_name;
    void* data;
    size_t size;
    dmlist_context_t* handles;
} file_t;

/** 
 * @brief File handle structure
 */
typedef struct 
{
    file_t* file;
    int mode;
    int attribute;
    size_t position;    // Current read/write position
} file_handle_t;

/** 
 * @brief Directory structure
 */
typedef struct dir
{
    char* dir_name;
    dmlist_context_t* files;
    dmlist_context_t* dirs;
} dir_t;

/**
 * @brief Directory handle structure for reading directory entries
 */
typedef struct
{
    dir_t* dir;
    size_t file_index;  // Current index in files list
    size_t dir_index;   // Current index in dirs list
} dir_handle_t;

/**
 * @brief File system context structure
 */
struct dmfsi_context
{
    uint32_t          magic;
    dir_t*            root_dir;
};


// ============================================================================
//                      Local Prototypes
// ============================================================================
static int              compare_file_name       (const void* a, const void* b);
static int              compare_dir_name        (const void* a, const void* b);
static int              compare_handle_ptr      (const void* a, const void* b);
static int              compare_file_ptr        (const void* a, const void* b);
static file_t*          find_file               (dir_t* dir, dmfsi_path_t* path);
static dir_t*           find_dir                (dir_t* dir, dmfsi_path_t* path);
static file_t*          create_file             (dir_t* dir, dmfsi_path_t* path);
static file_handle_t*   create_file_handle      (file_t* file, int mode, int attribute);
static dir_t*           create_dir              (dir_t* parent, dmfsi_path_t* path);
static dir_t*           create_root_dir         (void);
static void             free_file               (file_t* file);
static void             free_dir                (dir_t* dir);


// ============================================================================
//                      Module Interface Implementation
// ============================================================================
/**
 * @brief Module initialization (optional)
 */
void dmod_preinit(void)
{
    // Nothing to do
}

/**
 * @brief Module initialization
 */
int dmod_init(const Dmod_Config_t *Config)
{
    // Nothing to do
    return 0;
}

/**
 * @brief Module deinitialization
 */
int dmod_deinit(void)
{
    // Nothing to do
    return 0;
}

// ============================================================================
//                      DMFSI Interface Implementation
// ============================================================================

/**
 * @brief Initialize the file system
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, dmfsi_context_t, _init, (const char* config) )
{
    dmfsi_context_t ctx = Dmod_Malloc(sizeof(struct dmfsi_context));
    if (ctx == NULL)
    {
        DMOD_LOG_ERROR("dmramfs: Failed to allocate memory for context\n");
        return NULL;
    }
    ctx->magic = DMRAMFS_CONTEXT_MAGIC;
    ctx->root_dir = create_root_dir();
    if (ctx->root_dir == NULL)
    {
        DMOD_LOG_ERROR("dmramfs: Failed to create root directory\n");
        Dmod_Free(ctx);
        return NULL;
    }
    return ctx;
}

/**
 * @brief Deinitialize the file system
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _deinit, (dmfsi_context_t ctx) )
{
    if (ctx)
    {
        if (ctx->root_dir)
        {
            free_dir(ctx->root_dir);
        }
        Dmod_Free(ctx);
    }
    return DMFSI_OK;
}

/**
 * @brief Validate the file system context
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _context_is_valid, (dmfsi_context_t ctx) )
{
    return (ctx && ctx->magic == DMRAMFS_CONTEXT_MAGIC) ? 1 : 0;
}

/**
 * @brief Open a file
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _fopen, (dmfsi_context_t ctx, void** fp, const char* path, int mode, int attr) )
{
    if(dmfsi_dmramfs_context_is_valid(ctx) == 0)
    {
        DMOD_LOG_ERROR("dmramfs: Invalid context in fopen\n");
        return DMFSI_ERR_INVALID;
    }

    dmfsi_path_t* p = dmfsi_path_create(path);
    if (p == NULL)
    {
        DMOD_LOG_ERROR("dmramfs: Invalid path in fopen: '%s'\n", path);
        return DMFSI_ERR_INVALID;
    }
    file_t* file = find_file(ctx->root_dir, p);
    
    if (file == NULL)
    {
        bool can_create = (mode & DMFSI_O_CREAT) != 0 || (mode & DMFSI_O_WRONLY) != 0;
        file = can_create ? create_file(ctx->root_dir, p) : NULL;
        if(file == NULL)
        {
            DMOD_LOG_ERROR("dmramfs: File not found and cannot be created: '%s'\n", path);
            dmfsi_path_free(p);
            return DMFSI_ERR_NOT_FOUND;
        }
    }
    dmfsi_path_free(p);

    file_handle_t* handle = create_file_handle(file, mode, attr);
    if (handle == NULL)
    {
        DMOD_LOG_ERROR("dmramfs: Failed to allocate memory for file handle\n");
        return DMFSI_ERR_GENERAL;
    }

    *fp = handle;
    return DMFSI_OK;
}

/**
 * @brief Close a file
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _fclose, (dmfsi_context_t ctx, void* fp) )
{
    if(dmfsi_dmramfs_context_is_valid(ctx) == 0)
    {
        DMOD_LOG_ERROR("dmramfs: Invalid context in fclose\n");
        return DMFSI_ERR_INVALID;
    }
    
    if (fp == NULL)
    {
        DMOD_LOG_ERROR("dmramfs: NULL file pointer in fclose\n");
        return DMFSI_ERR_INVALID;
    }
    
    file_handle_t* handle = (file_handle_t*)fp;
    file_t* file = handle->file;
    
    // Remove handle from file's handle list
    if (file && file->handles)
    {
        dmlist_remove(file->handles, handle, compare_handle_ptr);
    }
    
    Dmod_Free(handle);
    return DMFSI_OK;
}

/**
 * @brief Read from a file
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _fread, (dmfsi_context_t ctx, void* fp, void* buffer, size_t size, size_t* read) )
{
    if(dmfsi_dmramfs_context_is_valid(ctx) == 0)
    {
        DMOD_LOG_ERROR("dmramfs: Invalid context in fread\n");
        return DMFSI_ERR_INVALID;
    }
    
    if (fp == NULL || buffer == NULL)
    {
        if (read) *read = 0;
        return DMFSI_ERR_INVALID;
    }
    
    file_handle_t* handle = (file_handle_t*)fp;
    file_t* file = handle->file;
    
    if (file == NULL || file->data == NULL)
    {
        if (read) *read = 0;
        return DMFSI_OK;  // Empty file, nothing to read
    }
    
    // Calculate how much we can read
    size_t available = (handle->position < file->size) ? (file->size - handle->position) : 0;
    size_t to_read = (size < available) ? size : available;
    
    if (to_read > 0)
    {
        memcpy(buffer, (char*)file->data + handle->position, to_read);
        handle->position += to_read;
    }
    
    if (read) *read = to_read;
    return DMFSI_OK;
}

/**
 * @brief Write to a file
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _fwrite, (dmfsi_context_t ctx, void* fp, const void* buffer, size_t size, size_t* written) )
{
    if(dmfsi_dmramfs_context_is_valid(ctx) == 0)
    {
        DMOD_LOG_ERROR("dmramfs: Invalid context in fwrite\n");
        return DMFSI_ERR_INVALID;
    }
    
    if (fp == NULL || buffer == NULL)
    {
        if (written) *written = 0;
        return DMFSI_ERR_INVALID;
    }
    
    file_handle_t* handle = (file_handle_t*)fp;
    file_t* file = handle->file;
    
    if (file == NULL)
    {
        if (written) *written = 0;
        return DMFSI_ERR_INVALID;
    }
    
    // Calculate new size needed
    size_t end_position = handle->position + size;
    
    // Resize the file data buffer if needed
    if (end_position > file->size)
    {
        void* new_data = Dmod_Malloc(end_position);
        if (new_data == NULL)
        {
            DMOD_LOG_ERROR("dmramfs: Failed to allocate memory for file data\n");
            if (written) *written = 0;
            return DMFSI_ERR_GENERAL;
        }
        
        // Copy existing data if any
        if (file->data && file->size > 0)
        {
            memcpy(new_data, file->data, file->size);
        }
        
        // Zero-fill gap between old size and current position
        if (handle->position > file->size)
        {
            memset((char*)new_data + file->size, 0, handle->position - file->size);
        }
        
        // Free old data
        if (file->data)
        {
            Dmod_Free(file->data);
        }
        
        file->data = new_data;
        file->size = end_position;
    }
    
    // Write the data
    memcpy((char*)file->data + handle->position, buffer, size);
    handle->position += size;
    
    if (written) *written = size;
    return DMFSI_OK;
}

/**
 * @brief Seek to a position in a file
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, long, _lseek, (dmfsi_context_t ctx, void* fp, long offset, int whence) )
{
    if(dmfsi_dmramfs_context_is_valid(ctx) == 0)
    {
        DMOD_LOG_ERROR("dmramfs: Invalid context in lseek\n");
        return -1;
    }
    
    if (fp == NULL)
    {
        return -1;
    }
    
    file_handle_t* handle = (file_handle_t*)fp;
    file_t* file = handle->file;
    long new_position;
    
    switch (whence)
    {
        case DMFSI_SEEK_SET:
            new_position = offset;
            break;
        case DMFSI_SEEK_CUR:
            new_position = (long)handle->position + offset;
            break;
        case DMFSI_SEEK_END:
            new_position = (file ? (long)file->size : 0) + offset;
            break;
        default:
            return -1;
    }
    
    if (new_position < 0)
    {
        return -1;
    }
    
    handle->position = (size_t)new_position;
    return new_position;
}

/**
 * @brief Perform I/O control operation
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _ioctl, (dmfsi_context_t ctx, void* fp, int request, void* arg) )
{
    // TODO: Implement ioctl
    return DMFSI_ERR_GENERAL;
}

/**
 * @brief Synchronize file data to storage
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _sync, (dmfsi_context_t ctx, void* fp) )
{
    // TODO: Implement sync (no-op for RAM FS)
    return DMFSI_OK;
}

/**
 * @brief Get a character from file
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _getc, (dmfsi_context_t ctx, void* fp) )
{
    if(dmfsi_dmramfs_context_is_valid(ctx) == 0)
    {
        return -1;
    }
    
    if (fp == NULL)
    {
        return -1;
    }
    
    file_handle_t* handle = (file_handle_t*)fp;
    file_t* file = handle->file;
    
    if (file == NULL || file->data == NULL || handle->position >= file->size)
    {
        return -1;  // EOF
    }
    
    unsigned char c = ((unsigned char*)file->data)[handle->position];
    handle->position++;
    return (int)c;
}

/**
 * @brief Put a character to file
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _putc, (dmfsi_context_t ctx, void* fp, int c) )
{
    if(dmfsi_dmramfs_context_is_valid(ctx) == 0)
    {
        return -1;
    }
    
    if (fp == NULL)
    {
        return -1;
    }
    
    unsigned char ch = (unsigned char)c;
    size_t written = 0;
    
    int ret = dmfsi_dmramfs_fwrite(ctx, fp, &ch, 1, &written);
    if (ret != DMFSI_OK || written != 1)
    {
        return -1;
    }
    
    return c;
}

/**
 * @brief Get current file position
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, long, _tell, (dmfsi_context_t ctx, void* fp) )
{
    if(dmfsi_dmramfs_context_is_valid(ctx) == 0)
    {
        return -1;
    }
    
    if (fp == NULL)
    {
        return -1;
    }
    
    file_handle_t* handle = (file_handle_t*)fp;
    return (long)handle->position;
}

/**
 * @brief Check if at end of file
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _eof, (dmfsi_context_t ctx, void* fp) )
{
    if(dmfsi_dmramfs_context_is_valid(ctx) == 0)
    {
        return -1;
    }
    
    if (fp == NULL)
    {
        return -1;
    }
    
    file_handle_t* handle = (file_handle_t*)fp;
    file_t* file = handle->file;
    
    if (file == NULL)
    {
        return 1;  // Empty file is at EOF
    }
    
    return (handle->position >= file->size) ? 1 : 0;
}

/**
 * @brief Get file size
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, long, _size, (dmfsi_context_t ctx, void* fp) )
{
    if(dmfsi_dmramfs_context_is_valid(ctx) == 0)
    {
        return -1;
    }
    
    if (fp == NULL)
    {
        return -1;
    }
    
    file_handle_t* handle = (file_handle_t*)fp;
    file_t* file = handle->file;
    
    if (file == NULL)
    {
        return 0;
    }
    
    return (long)file->size;
}

/**
 * @brief Flush file buffers
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _fflush, (dmfsi_context_t ctx, void* fp) )
{
    // TODO: Implement flush (no-op for RAM FS)
    return DMFSI_OK;
}

/**
 * @brief Get last error code
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _error, (dmfsi_context_t ctx, void* fp) )
{
    // RAM filesystem doesn't track error state per handle
    return DMFSI_OK;
}

/**
 * @brief Open a directory
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _opendir, (dmfsi_context_t ctx, void** dp, const char* path) )
{
    if(dmfsi_dmramfs_context_is_valid(ctx) == 0)
    {
        DMOD_LOG_ERROR("dmramfs: Invalid context in opendir\n");
        return DMFSI_ERR_INVALID;
    }
    
    if (dp == NULL)
    {
        return DMFSI_ERR_INVALID;
    }
    
    dir_t* dir = NULL;
    
    // Handle root directory case
    if (path == NULL || strcmp(path, "/") == 0 || strcmp(path, "") == 0)
    {
        dir = ctx->root_dir;
    }
    else
    {
        // Remove leading slash if present
        const char* search_path = path;
        if (search_path[0] == '/')
        {
            search_path++;
        }
        
        // Remove trailing slash if present
        size_t len = strlen(search_path);
        char* clean_path = dmfsi_strndup(search_path, len);
        if (clean_path == NULL)
        {
            return DMFSI_ERR_GENERAL;
        }
        
        if (len > 0 && clean_path[len - 1] == '/')
        {
            clean_path[len - 1] = '\0';
        }
        
        if (strlen(clean_path) == 0)
        {
            dir = ctx->root_dir;
        }
        else
        {
            dmfsi_path_t* p = dmfsi_path_create(clean_path);
            if (p != NULL)
            {
                dir = find_dir(ctx->root_dir, p);
                dmfsi_path_free(p);
            }
        }
        Dmod_Free(clean_path);
    }
    
    if (dir == NULL)
    {
        return DMFSI_ERR_NOT_FOUND;
    }
    
    dir_handle_t* handle = Dmod_Malloc(sizeof(dir_handle_t));
    if (handle == NULL)
    {
        return DMFSI_ERR_GENERAL;
    }
    
    handle->dir = dir;
    handle->file_index = 0;
    handle->dir_index = 0;
    
    *dp = handle;
    return DMFSI_OK;
}

/**
 * @brief Close a directory
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _closedir, (dmfsi_context_t ctx, void* dp) )
{
    if(dmfsi_dmramfs_context_is_valid(ctx) == 0)
    {
        return DMFSI_ERR_INVALID;
    }
    
    if (dp == NULL)
    {
        return DMFSI_ERR_INVALID;
    }
    
    dir_handle_t* handle = (dir_handle_t*)dp;
    Dmod_Free(handle);
    return DMFSI_OK;
}

/**
 * @brief Read directory entry
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _readdir, (dmfsi_context_t ctx, void* dp, dmfsi_dir_entry_t* entry) )
{
    if(dmfsi_dmramfs_context_is_valid(ctx) == 0)
    {
        return DMFSI_ERR_INVALID;
    }
    
    if (dp == NULL || entry == NULL)
    {
        return DMFSI_ERR_INVALID;
    }
    
    dir_handle_t* handle = (dir_handle_t*)dp;
    dir_t* dir = handle->dir;
    
    if (dir == NULL)
    {
        return DMFSI_ERR_NOT_FOUND;
    }
    
    // First iterate through files
    size_t file_count = dmlist_size(dir->files);
    if (handle->file_index < file_count)
    {
        file_t* file = (file_t*)dmlist_get(dir->files, handle->file_index);
        if (file != NULL)
        {
            strncpy(entry->name, file->file_name, sizeof(entry->name) - 1);
            entry->name[sizeof(entry->name) - 1] = '\0';
            entry->size = (uint32_t)file->size;
            entry->attr = 0;  // Regular file
            entry->time = 0;
            handle->file_index++;
            return DMFSI_OK;
        }
    }
    
    // Then iterate through subdirectories
    size_t dir_count = dmlist_size(dir->dirs);
    if (handle->dir_index < dir_count)
    {
        dir_t* subdir = (dir_t*)dmlist_get(dir->dirs, handle->dir_index);
        if (subdir != NULL)
        {
            strncpy(entry->name, subdir->dir_name, sizeof(entry->name) - 1);
            entry->name[sizeof(entry->name) - 1] = '\0';
            entry->size = 0;
            entry->attr = 0x10;  // Directory attribute
            entry->time = 0;
            handle->dir_index++;
            return DMFSI_OK;
        }
    }
    
    // No more entries
    return DMFSI_ERR_NOT_FOUND;
}

/**
 * @brief Get file/directory statistics
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _stat, (dmfsi_context_t ctx, const char* path, dmfsi_stat_t* stat) )
{
    if(dmfsi_dmramfs_context_is_valid(ctx) == 0)
    {
        DMOD_LOG_ERROR("dmramfs: Invalid context in stat\n");
        return DMFSI_ERR_INVALID;
    }
    
    if (path == NULL || stat == NULL)
    {
        return DMFSI_ERR_INVALID;
    }
    
    // Remove leading slash if present
    const char* search_path = path;
    if (search_path[0] == '/')
    {
        search_path++;
    }
    
    if (strlen(search_path) == 0)
    {
        // Root directory stat
        stat->size = 0;
        stat->attr = 0x10;  // Directory
        stat->ctime = 0;
        stat->mtime = 0;
        stat->atime = 0;
        return DMFSI_OK;
    }
    
    dmfsi_path_t* p = dmfsi_path_create(search_path);
    if (p == NULL)
    {
        return DMFSI_ERR_INVALID;
    }
    
    // Try to find as file first
    file_t* file = find_file(ctx->root_dir, p);
    if (file != NULL)
    {
        stat->size = (uint32_t)file->size;
        stat->attr = 0;  // Regular file
        stat->ctime = 0;
        stat->mtime = 0;
        stat->atime = 0;
        dmfsi_path_free(p);
        return DMFSI_OK;
    }
    
    // Try to find as directory
    dir_t* dir = find_dir(ctx->root_dir, p);
    if (dir != NULL)
    {
        stat->size = 0;
        stat->attr = 0x10;  // Directory
        stat->ctime = 0;
        stat->mtime = 0;
        stat->atime = 0;
        dmfsi_path_free(p);
        return DMFSI_OK;
    }
    
    dmfsi_path_free(p);
    return DMFSI_ERR_NOT_FOUND;
}

/**
 * @brief Delete a file
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _unlink, (dmfsi_context_t ctx, const char* path) )
{
    if(dmfsi_dmramfs_context_is_valid(ctx) == 0)
    {
        DMOD_LOG_ERROR("dmramfs: Invalid context in unlink\n");
        return DMFSI_ERR_INVALID;
    }
    
    if (path == NULL)
    {
        return DMFSI_ERR_INVALID;
    }
    
    // Remove leading slash if present
    const char* search_path = path;
    if (search_path[0] == '/')
    {
        search_path++;
    }
    
    dmfsi_path_t* p = dmfsi_path_create(search_path);
    if (p == NULL)
    {
        return DMFSI_ERR_INVALID;
    }
    
    // Find the parent directory and file
    dir_t* parent_dir = ctx->root_dir;
    dmfsi_path_t* current = p;
    
    // Navigate to parent directory
    while (current->directory != NULL && current->next != NULL)
    {
        dir_t* subdir = dmlist_find(parent_dir->dirs, current->directory, compare_dir_name);
        if (subdir == NULL)
        {
            dmfsi_path_free(p);
            return DMFSI_ERR_NOT_FOUND;
        }
        parent_dir = subdir;
        current = current->next;
    }
    
    // Find and remove the file
    const char* filename = (current->filename != NULL) ? current->filename : current->directory;
    if (filename == NULL)
    {
        dmfsi_path_free(p);
        return DMFSI_ERR_NOT_FOUND;
    }
    
    file_t* file = dmlist_find(parent_dir->files, filename, compare_file_name);
    if (file == NULL)
    {
        dmfsi_path_free(p);
        return DMFSI_ERR_NOT_FOUND;
    }
    
    // Check if file has open handles
    if (file->handles && dmlist_size(file->handles) > 0)
    {
        dmfsi_path_free(p);
        return DMFSI_ERR_INVALID;  // File is in use
    }
    
    // Remove from list and free
    dmlist_remove(parent_dir->files, file, compare_file_ptr);
    free_file(file);
    
    dmfsi_path_free(p);
    return DMFSI_OK;
}

/**
 * @brief Rename a file or directory
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _rename, (dmfsi_context_t ctx, const char* oldpath, const char* newpath) )
{
    if(dmfsi_dmramfs_context_is_valid(ctx) == 0)
    {
        DMOD_LOG_ERROR("dmramfs: Invalid context in rename\n");
        return DMFSI_ERR_INVALID;
    }
    
    if (oldpath == NULL || newpath == NULL)
    {
        return DMFSI_ERR_INVALID;
    }
    
    // Remove leading slashes
    const char* old_search = (oldpath[0] == '/') ? oldpath + 1 : oldpath;
    const char* new_search = (newpath[0] == '/') ? newpath + 1 : newpath;
    
    dmfsi_path_t* old_p = dmfsi_path_create(old_search);
    if (old_p == NULL)
    {
        return DMFSI_ERR_INVALID;
    }
    
    // Find the file
    file_t* file = find_file(ctx->root_dir, old_p);
    if (file == NULL)
    {
        dmfsi_path_free(old_p);
        return DMFSI_ERR_NOT_FOUND;
    }
    
    // Extract just the filename from the new path
    dmfsi_path_t* new_p = dmfsi_path_create(new_search);
    if (new_p == NULL)
    {
        dmfsi_path_free(old_p);
        return DMFSI_ERR_INVALID;
    }
    
    // Find the new filename (traverse to the end)
    dmfsi_path_t* current = new_p;
    while (current->next != NULL)
    {
        current = current->next;
    }
    
    const char* new_name = (current->filename != NULL) ? current->filename : current->directory;
    if (new_name == NULL)
    {
        dmfsi_path_free(old_p);
        dmfsi_path_free(new_p);
        return DMFSI_ERR_INVALID;
    }
    
    // Update the filename
    char* old_name = file->file_name;
    file->file_name = dmfsi_strndup(new_name, strlen(new_name));
    if (file->file_name == NULL)
    {
        file->file_name = old_name;  // Restore on failure
        dmfsi_path_free(old_p);
        dmfsi_path_free(new_p);
        return DMFSI_ERR_GENERAL;
    }
    
    Dmod_Free(old_name);
    dmfsi_path_free(old_p);
    dmfsi_path_free(new_p);
    return DMFSI_OK;
}

/**
 * @brief Change file mode/permissions
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _chmod, (dmfsi_context_t ctx, const char* path, int mode) )
{
    // RAM filesystem doesn't support permissions
    // But we should verify the file exists
    if(dmfsi_dmramfs_context_is_valid(ctx) == 0)
    {
        return DMFSI_ERR_INVALID;
    }
    
    if (path == NULL)
    {
        return DMFSI_ERR_INVALID;
    }
    
    // Remove leading slash if present
    const char* search_path = path;
    if (search_path[0] == '/')
    {
        search_path++;
    }
    
    if (strlen(search_path) == 0)
    {
        return DMFSI_OK;  // Root directory
    }
    
    dmfsi_path_t* p = dmfsi_path_create(search_path);
    if (p == NULL)
    {
        return DMFSI_ERR_INVALID;
    }
    
    // Check if file or directory exists
    file_t* file = find_file(ctx->root_dir, p);
    dir_t* dir = (file == NULL) ? find_dir(ctx->root_dir, p) : NULL;
    
    dmfsi_path_free(p);
    
    if (file == NULL && dir == NULL)
    {
        return DMFSI_ERR_NOT_FOUND;
    }
    
    return DMFSI_OK;
}

/**
 * @brief Change file access and modification times
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _utime, (dmfsi_context_t ctx, const char* path, uint32_t atime, uint32_t mtime) )
{
    // RAM filesystem doesn't track timestamps
    // But we should verify the file exists
    if(dmfsi_dmramfs_context_is_valid(ctx) == 0)
    {
        return DMFSI_ERR_INVALID;
    }
    
    if (path == NULL)
    {
        return DMFSI_ERR_INVALID;
    }
    
    // Remove leading slash if present
    const char* search_path = path;
    if (search_path[0] == '/')
    {
        search_path++;
    }
    
    if (strlen(search_path) == 0)
    {
        return DMFSI_OK;  // Root directory
    }
    
    dmfsi_path_t* p = dmfsi_path_create(search_path);
    if (p == NULL)
    {
        return DMFSI_ERR_INVALID;
    }
    
    // Check if file or directory exists
    file_t* file = find_file(ctx->root_dir, p);
    dir_t* dir = (file == NULL) ? find_dir(ctx->root_dir, p) : NULL;
    
    dmfsi_path_free(p);
    
    if (file == NULL && dir == NULL)
    {
        return DMFSI_ERR_NOT_FOUND;
    }
    
    return DMFSI_OK;
}

/**
 * @brief Create a directory
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _mkdir, (dmfsi_context_t ctx, const char* path, int mode) )
{
    if(dmfsi_dmramfs_context_is_valid(ctx) == 0)
    {
        DMOD_LOG_ERROR("dmramfs: Invalid context in mkdir\n");
        return DMFSI_ERR_INVALID;
    }
    
    if (path == NULL)
    {
        return DMFSI_ERR_INVALID;
    }
    
    // Remove leading slash if present
    const char* search_path = path;
    if (search_path[0] == '/')
    {
        search_path++;
    }
    
    if (strlen(search_path) == 0)
    {
        return DMFSI_ERR_INVALID;  // Can't create root
    }
    
    dmfsi_path_t* p = dmfsi_path_create(search_path);
    if (p == NULL)
    {
        return DMFSI_ERR_INVALID;
    }
    
    // Check if already exists
    dir_t* existing = find_dir(ctx->root_dir, p);
    if (existing != NULL)
    {
        dmfsi_path_free(p);
        return DMFSI_OK;  // Already exists
    }
    
    // Create the directory
    dir_t* new_dir = create_dir(ctx->root_dir, p);
    dmfsi_path_free(p);
    
    if (new_dir == NULL)
    {
        return DMFSI_ERR_GENERAL;
    }
    
    return DMFSI_OK;
}

/**
 * @brief Check if directory exists
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _direxists, (dmfsi_context_t ctx, const char* path) )
{
    if(dmfsi_dmramfs_context_is_valid(ctx) == 0)
    {
        return 0;
    }
    
    if (path == NULL)
    {
        return 0;
    }
    
    // Handle root directory
    if (strcmp(path, "/") == 0 || strcmp(path, "") == 0)
    {
        return 1;
    }
    
    // Remove leading slash if present
    const char* search_path = path;
    if (search_path[0] == '/')
    {
        search_path++;
    }
    
    // Remove trailing slash if present
    size_t len = strlen(search_path);
    char* clean_path = dmfsi_strndup(search_path, len);
    if (clean_path == NULL)
    {
        return 0;
    }
    
    if (len > 0 && clean_path[len - 1] == '/')
    {
        clean_path[len - 1] = '\0';
    }
    
    if (strlen(clean_path) == 0)
    {
        Dmod_Free(clean_path);
        return 1;  // Root directory
    }
    
    dmfsi_path_t* p = dmfsi_path_create(clean_path);
    Dmod_Free(clean_path);
    
    if (p == NULL)
    {
        return 0;
    }
    
    dir_t* dir = find_dir(ctx->root_dir, p);
    dmfsi_path_free(p);
    
    return (dir != NULL) ? 1 : 0;
}

// ============================================================================
//                      Local Functions
// ============================================================================

/**
 * @brief Compare file path with a given path
 */
static int compare_file_name(const void* file, const void* file_name)
{
    const file_t* file_a = (const file_t*)file;
    const char* path_b = (const char*)file_name;
    return strcmp(file_a->file_name, path_b);
}

/**
 * @brief Compare directory name with a given name
 */
static int compare_dir_name(const void* a, const void* b)
{
    const dir_t* dir_a = (const dir_t*)a;
    const char* name_b = (const char*)b;
    // Assuming dir_t has a 'dir_name' member for comparison
    return strcmp(dir_a->dir_name, name_b);
}

/**
 * @brief Compare file handle pointers
 */
static int compare_handle_ptr(const void* a, const void* b)
{
    return (a == b) ? 0 : 1;
}

/**
 * @brief Compare file pointers
 */
static int compare_file_ptr(const void* a, const void* b)
{
    return (a == b) ? 0 : 1;
}

/**
 * @brief Find a file by its path
 */
static file_t* find_file(dir_t* dir, dmfsi_path_t* path)
{
    if (dir == NULL || path == NULL)
    {
        return NULL;
    }

    if(path->filename != NULL)
    {
        file_t* file = dmlist_find(dir->files, path->filename, compare_file_name);
        return file;  // Return the found file (or NULL if not found)
    }
    else if(path->directory != NULL)
    {
        // Handle empty directory (root path like /file.txt)
        if (strlen(path->directory) == 0 && path->next != NULL)
        {
            return find_file(dir, path->next);
        }
        
        dir_t* subdir = dmlist_find(dir->dirs, path->directory, compare_dir_name);
        if(subdir != NULL && path->next != NULL)
        {
            return find_file(subdir, path->next);
        }
    }
    return NULL;
}

/**
 * @brief Find a directory by its path
 */
static dir_t* find_dir(dir_t* dir, dmfsi_path_t* path)
{
    if (dir == NULL || path == NULL)
    {
        return NULL;
    }

    // Handle case where path ends with a filename (treat as dir name for direxists)
    const char* name = (path->directory != NULL) ? path->directory : path->filename;
    if (name == NULL)
    {
        return NULL;
    }
    
    // Handle empty directory (root path)
    if (strlen(name) == 0 && path->next != NULL)
    {
        return find_dir(dir, path->next);
    }

    dir_t* subdir = dmlist_find(dir->dirs, name, compare_dir_name);
    if (subdir != NULL)
    {
        if (path->next != NULL)
        {
            return find_dir(subdir, path->next);
        }
        else
        {
            return subdir;
        }
    }
    
    return NULL;
}

/**
 * @brief Create a file at the specified path
 * 
 * @param dir   The starting directory
 * @param path  The path to create the file at
 * 
 * @return file_t*  Pointer to the created file, or NULL on failure
 */
static file_t* create_file(dir_t* dir, dmfsi_path_t* path)
{
    if(path->filename != NULL)
    {
        file_t* file = Dmod_Malloc(sizeof(file_t));
        if(file == NULL)
        {
            DMOD_LOG_ERROR("dmramfs: Failed to allocate memory for new file '%s'\n", path->filename);
            return NULL;
        }
        file->file_name = dmfsi_strndup(path->filename, strlen(path->filename));
        file->data = NULL;
        file->size = 0;
        file->handles = dmlist_create(DMOD_MODULE_NAME);
        if(!dmlist_insert(dir->files, 0, file))
        {
            DMOD_LOG_ERROR("dmramfs: Failed to insert new file '%s' into directory\n", path->filename);
            if (file->handles) dmlist_destroy(file->handles);
            if (file->file_name) Dmod_Free(file->file_name);
            Dmod_Free(file);
            return NULL;
        }
        return file;
    }
    else if(path->directory != NULL)
    {
        // Handle empty directory (root path like /file.txt)
        if (strlen(path->directory) == 0 && path->next != NULL)
        {
            return create_file(dir, path->next);
        }
        
        dir_t* subdir = dmlist_find(dir->dirs, path->directory, compare_dir_name);
        if(subdir == NULL)
        {
            DMOD_LOG_ERROR("dmramfs: Directory '%s' not found in path for file creation\n", path->directory);
            return NULL;
        }
        if(path->next != NULL)
        {
            return create_file(subdir, path->next);
        }
    }
    return NULL;
}

/**
 * @brief Create a file handle for the specified file
 * 
 * @param file      The file to create a handle for
 * @param mode      The mode to open the file with
 * @param attribute The attributes for the file handle
 * 
 * @return file_handle_t*  Pointer to the created file handle, or NULL on failure
 */
static file_handle_t* create_file_handle(file_t* file, int mode, int attribute)
{
    file_handle_t* handle = Dmod_Malloc(sizeof(file_handle_t));
    if (handle == NULL)
    {
        DMOD_LOG_ERROR("dmramfs: Failed to allocate memory for file handle\n");
        return NULL;
    }

    handle->file = file;
    handle->mode = mode;
    handle->attribute = attribute;
    handle->position = 0;

    // Handle truncate mode
    if ((mode & DMFSI_O_TRUNC) && file != NULL)
    {
        if (file->data)
        {
            Dmod_Free(file->data);
            file->data = NULL;
        }
        file->size = 0;
    }

    // Handle append mode - start at end of file
    if ((mode & DMFSI_O_APPEND) && file != NULL)
    {
        handle->position = file->size;
    }

    if(!dmlist_push_back(file->handles, handle))
    {
        DMOD_LOG_ERROR("dmramfs: Failed to add handle to file's handle list\n");
        Dmod_Free(handle);
        return NULL;
    }

    return handle;
}

/**
 * @brief Create the root directory
 * 
 * @return dir_t*  Pointer to the created root directory, or NULL on failure
 */
static dir_t* create_root_dir(void)
{
    dir_t* root = Dmod_Malloc(sizeof(dir_t));
    if (root == NULL)
    {
        DMOD_LOG_ERROR("dmramfs: Failed to allocate memory for root directory\n");
        return NULL;
    }

    root->dir_name = dmfsi_strndup("/", 1);
    root->files = dmlist_create(DMOD_MODULE_NAME);
    root->dirs = dmlist_create(DMOD_MODULE_NAME);

    if (root->dir_name == NULL || root->files == NULL || root->dirs == NULL)
    {
        DMOD_LOG_ERROR("dmramfs: Failed to initialize root directory\n");
        if (root->dir_name) Dmod_Free(root->dir_name);
        if (root->files) dmlist_destroy(root->files);
        if (root->dirs) dmlist_destroy(root->dirs);
        Dmod_Free(root);
        return NULL;
    }

    return root;
}

/**
 * @brief Create a directory at the specified path
 * 
 * @param parent  The parent directory
 * @param path    The path to create the directory at
 * 
 * @return dir_t*  Pointer to the created directory, or NULL on failure
 */
static dir_t* create_dir(dir_t* parent, dmfsi_path_t* path)
{
    if (path == NULL || parent == NULL)
    {
        return NULL;
    }

    // If this is a filename entry, treat it as a directory name (for mkdir with no trailing slash)
    const char* name = (path->filename != NULL) ? path->filename : path->directory;
    if (name == NULL)
    {
        return NULL;
    }
    
    // Handle empty directory (root path)
    if (strlen(name) == 0 && path->next != NULL)
    {
        return create_dir(parent, path->next);
    }

    // Check if it's the final component
    if (path->filename != NULL || path->next == NULL)
    {
        // Check if directory already exists
        dir_t* existing = dmlist_find(parent->dirs, name, compare_dir_name);
        if (existing != NULL)
        {
            return existing;
        }

        // Create new directory
        dir_t* new_dir = Dmod_Malloc(sizeof(dir_t));
        if (new_dir == NULL)
        {
            DMOD_LOG_ERROR("dmramfs: Failed to allocate memory for directory '%s'\n", name);
            return NULL;
        }

        new_dir->dir_name = dmfsi_strndup(name, strlen(name));
        new_dir->files = dmlist_create(DMOD_MODULE_NAME);
        new_dir->dirs = dmlist_create(DMOD_MODULE_NAME);

        if (new_dir->dir_name == NULL || new_dir->files == NULL || new_dir->dirs == NULL)
        {
            DMOD_LOG_ERROR("dmramfs: Failed to initialize directory '%s'\n", name);
            if (new_dir->dir_name) Dmod_Free(new_dir->dir_name);
            if (new_dir->files) dmlist_destroy(new_dir->files);
            if (new_dir->dirs) dmlist_destroy(new_dir->dirs);
            Dmod_Free(new_dir);
            return NULL;
        }

        if (!dmlist_insert(parent->dirs, 0, new_dir))
        {
            DMOD_LOG_ERROR("dmramfs: Failed to insert directory '%s' into parent\n", name);
            if (new_dir->dir_name) Dmod_Free(new_dir->dir_name);
            if (new_dir->files) dmlist_destroy(new_dir->files);
            if (new_dir->dirs) dmlist_destroy(new_dir->dirs);
            Dmod_Free(new_dir);
            return NULL;
        }

        return new_dir;
    }
    else
    {
        // Navigate to or create intermediate directory
        dir_t* subdir = dmlist_find(parent->dirs, name, compare_dir_name);
        if (subdir == NULL)
        {
            // Create intermediate directory
            subdir = Dmod_Malloc(sizeof(dir_t));
            if (subdir == NULL)
            {
                return NULL;
            }

            subdir->dir_name = dmfsi_strndup(name, strlen(name));
            subdir->files = dmlist_create(DMOD_MODULE_NAME);
            subdir->dirs = dmlist_create(DMOD_MODULE_NAME);

            if (!dmlist_insert(parent->dirs, 0, subdir))
            {
                if (subdir->dir_name) Dmod_Free(subdir->dir_name);
                if (subdir->files) dmlist_destroy(subdir->files);
                if (subdir->dirs) dmlist_destroy(subdir->dirs);
                Dmod_Free(subdir);
                return NULL;
            }
        }

        return create_dir(subdir, path->next);
    }
}

/**
 * @brief Free a file and all its resources
 * 
 * @param file  The file to free
 */
static void free_file(file_t* file)
{
    if (file == NULL)
    {
        return;
    }

    if (file->file_name)
    {
        Dmod_Free(file->file_name);
    }

    if (file->data)
    {
        Dmod_Free(file->data);
    }

    if (file->handles)
    {
        // Free all handles
        while (dmlist_size(file->handles) > 0)
        {
            file_handle_t* handle = (file_handle_t*)dmlist_front(file->handles);
            dmlist_pop_front(file->handles);
            if (handle) Dmod_Free(handle);
        }
        dmlist_destroy(file->handles);
    }

    Dmod_Free(file);
}

/**
 * @brief Free a directory and all its contents recursively
 * 
 * @param dir  The directory to free
 */
static void free_dir(dir_t* dir)
{
    if (dir == NULL)
    {
        return;
    }

    // Free all files
    if (dir->files)
    {
        while (dmlist_size(dir->files) > 0)
        {
            file_t* file = (file_t*)dmlist_front(dir->files);
            dmlist_pop_front(dir->files);
            free_file(file);
        }
        dmlist_destroy(dir->files);
    }

    // Free all subdirectories recursively
    if (dir->dirs)
    {
        while (dmlist_size(dir->dirs) > 0)
        {
            dir_t* subdir = (dir_t*)dmlist_front(dir->dirs);
            dmlist_pop_front(dir->dirs);
            free_dir(subdir);
        }
        dmlist_destroy(dir->dirs);
    }

    if (dir->dir_name)
    {
        Dmod_Free(dir->dir_name);
    }

    Dmod_Free(dir);
}