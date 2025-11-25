#define DMOD_ENABLE_REGISTRATION    ON
#define ENABLE_DIF_REGISTRATIONS    ON
#include "dmod.h"
#include "dmramfs.h"
#include "dmfsi.h"
#include "dmlist.h"

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
static int              compare_file_name   (const void* a, const void* b);
static int              compare_dir_name    (const void* a, const void* b);
static file_t*          find_file           (dir_t* dir, dmfsi_path_t* path);
static dir_t*           find_dir            (dir_t* dir, dmfsi_path_t* path);
static file_t*          create_file         (dir_t* dir, dmfsi_path_t* path);
static file_handle_t*   create_file_handle  (file_t* file, int mode, int attribute);


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
    return ctx;
}

/**
 * @brief Deinitialize the file system
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _deinit, (dmfsi_context_t ctx) )
{
    if (ctx)
    {
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
    // TODO: Implement file close
    return DMFSI_ERR_GENERAL;
}

/**
 * @brief Read from a file
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _fread, (dmfsi_context_t ctx, void* fp, void* buffer, size_t size, size_t* read) )
{
    // TODO: Implement file read
    if (read) *read = 0;
    return DMFSI_ERR_GENERAL;
}

/**
 * @brief Write to a file
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _fwrite, (dmfsi_context_t ctx, void* fp, const void* buffer, size_t size, size_t* written) )
{
    // TODO: Implement file write
    if (written) *written = 0;
    return DMFSI_ERR_GENERAL;
}

/**
 * @brief Seek to a position in a file
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, long, _lseek, (dmfsi_context_t ctx, void* fp, long offset, int whence) )
{
    // TODO: Implement file seek
    return -1;
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
    // TODO: Implement getc
    return -1;
}

/**
 * @brief Put a character to file
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _putc, (dmfsi_context_t ctx, void* fp, int c) )
{
    // TODO: Implement putc
    return -1;
}

/**
 * @brief Get current file position
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, long, _tell, (dmfsi_context_t ctx, void* fp) )
{
    // TODO: Implement tell
    return -1;
}

/**
 * @brief Check if at end of file
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _eof, (dmfsi_context_t ctx, void* fp) )
{
    // TODO: Implement EOF check
    return -1;
}

/**
 * @brief Get file size
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, long, _size, (dmfsi_context_t ctx, void* fp) )
{
    // TODO: Implement size
    return -1;
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
    // TODO: Implement error reporting
    return DMFSI_ERR_GENERAL;
}

/**
 * @brief Open a directory
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _opendir, (dmfsi_context_t ctx, void** dp, const char* path) )
{
    // TODO: Implement directory open
    return DMFSI_ERR_GENERAL;
}

/**
 * @brief Close a directory
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _closedir, (dmfsi_context_t ctx, void* dp) )
{
    // TODO: Implement directory close
    return DMFSI_ERR_GENERAL;
}

/**
 * @brief Read directory entry
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _readdir, (dmfsi_context_t ctx, void* dp, dmfsi_dir_entry_t* entry) )
{
    // TODO: Implement directory read
    return DMFSI_ERR_NOT_FOUND;
}

/**
 * @brief Get file/directory statistics
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _stat, (dmfsi_context_t ctx, const char* path, dmfsi_stat_t* stat) )
{
    // TODO: Implement stat
    return DMFSI_ERR_GENERAL;
}

/**
 * @brief Delete a file
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _unlink, (dmfsi_context_t ctx, const char* path) )
{
    // TODO: Implement file deletion
    return DMFSI_ERR_GENERAL;
}

/**
 * @brief Rename a file or directory
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _rename, (dmfsi_context_t ctx, const char* oldpath, const char* newpath) )
{
    // TODO: Implement rename
    return DMFSI_ERR_GENERAL;
}

/**
 * @brief Change file mode/permissions
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _chmod, (dmfsi_context_t ctx, const char* path, int mode) )
{
    // TODO: Implement chmod
    return DMFSI_ERR_GENERAL;
}

/**
 * @brief Change file access and modification times
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _utime, (dmfsi_context_t ctx, const char* path, uint32_t atime, uint32_t mtime) )
{
    // TODO: Implement utime
    return DMFSI_ERR_GENERAL;
}

/**
 * @brief Create a directory
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _mkdir, (dmfsi_context_t ctx, const char* path, int mode) )
{
    // TODO: Implement directory creation
    return DMFSI_ERR_GENERAL;
}

/**
 * @brief Check if directory exists
 */
dmod_dmfsi_dif_api_declaration( 1.0, dmramfs, int, _direxists, (dmfsi_context_t ctx, const char* path) )
{
    // TODO: Implement directory existence check
    return 0;
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
 * @brief Find a file by its path
 */
static file_t* find_file(dir_t* dir, dmfsi_path_t* path)
{
    if(path->filename != NULL)
    {
        file_t* file = dmlist_find(dir->files, path->filename, compare_file_name);
        if(file == NULL)
        {
            dir_t* subdir = dmlist_find(dir->dirs, path->filename, compare_dir_name);
            if(subdir != NULL)
            {
                DMOD_LOG_ERROR("dmramfs: Path '%s' is a directory, not a file\n", path->filename);
            }
            else 
            {
                DMOD_LOG_ERROR("dmramfs: File '%s' not found in path\n", path->filename);
            }
        }
    }
    else if(path->directory != NULL)
    {
        dir_t* subdir = dmlist_find(dir->dirs, path->directory, compare_dir_name);
        if(subdir != NULL && path->next != NULL)
        {
            return find_file(subdir, path->next);
        }
        else 
        {
            DMOD_LOG_ERROR("dmramfs: Directory '%s' not found in path\n", path->directory);
        }
    }
    return NULL;
}

/**
 * @brief Find a directory by its path
 */
static dir_t* find_dir(dir_t* dir, dmfsi_path_t* path)
{
    if(path->directory != NULL)
    {
        dir_t* subdir = dmlist_find(dir->dirs, path->directory, compare_dir_name);
        if(subdir != NULL)
        {
            if(path->next != NULL)
            {
                return find_dir(subdir, path->next);
            }
            else 
            {
                return subdir;
            }
        }
        else 
        {
            DMOD_LOG_ERROR("dmramfs: Directory '%s' not found in path\n", path->directory);
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
        if(dmlist_insert(dir->files, 0, file) != 0)
        {
            DMOD_LOG_ERROR("dmramfs: Failed to insert new file '%s' into directory\n", path->filename);
            Dmod_Free(file->file_name);
            Dmod_Free(file);
            return NULL;
        }
        return file;
    }
    else if(path->directory != NULL)
    {
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

    if(!dmlist_push_back(file->handles, handle))
    {
        DMOD_LOG_ERROR("dmramfs: Failed to add handle to file's handle list\n");
        Dmod_Free(handle);
        return NULL;
    }

    return handle;
}