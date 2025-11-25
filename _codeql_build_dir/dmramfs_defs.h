#ifndef DMOD_MOD_DEFS_H_dmramfs
#define DMOD_MOD_DEFS_H_dmramfs

#include "dmod_defs.h"

#ifndef dmod_dmramfs_version
#  define dmod_dmramfs_version "0.1"
#endif

#ifdef DMOD_dmramfs
#  define dmod_dmramfs_api_to_mal(MODULE,NAME)                            \
            DMOD_API_TO_MAL(dmramfs, MODULE, NAME)
#  define dmod_dmramfs_api_to_mal_ex(NAME_IN, MODULE_MAL, NAME_MAL)       \
            DMOD_API_TO_MAL_EX(dmramfs, MODULE_IN, NAME_IN, MODULE_MAL, NAME_MAL)
#  define dmod_dmramfs_api(VERSION, RET, NAME, PARAMS)                    \
            DMOD_INPUT_API(dmramfs, DMOD_MAKE_VERSION(VERSION,0.1), RET, NAME, PARAMS)
#  define dmod_dmramfs_global_api(VERSION, RET, NAME, PARAMS)             \
            DMOD_GLOBAL_INPUT_API(dmramfs, DMOD_MAKE_VERSION(VERSION,0.1), RET, NAME, PARAMS)
#  define dmod_dmramfs_mal(VERSION, RET, NAME, PARAMS)                    \
            DMOD_MAL_OUTPUT_API(dmramfs , DMOD_MAKE_VERSION(VERSION,0.1), RET, NAME, PARAMS)
#  define dmod_dmramfs_global_mal(VERSION, RET, NAME, PARAMS)             \
            DMOD_GLOBAL_MAL_OUTPUT_API(dmramfs, DMOD_MAKE_VERSION(VERSION,0.1), RET, NAME, PARAMS)
#  define dmod_dmramfs_api_declaration(VERSION, RET, NAME, PARAMS)        \
            DMOD_INPUT_API_DECLARATION(dmramfs, DMOD_MAKE_VERSION(VERSION,0.1), RET, NAME, PARAMS)
#  define dmod_dmramfs_dif(VERSION, RET, NAME, PARAMS)                    \
            DMOD_DIF_FUNCTION_TYPE_DECLARATION(dmramfs, VERSION, RET, NAME, PARAMS);\
            DMOD_DIF_SIGNATURE_REGISTRATION(dmramfs, NAME, DMOD_MAKE_VERSION(VERSION,0.1))
#  define dmod_dmramfs_dif_api_declaration(VERSION, IMPL_MODULE, RET, NAME, PARAMS)  \
            DMOD_DIF_API_DECLARATION(dmramfs, IMPL_MODULE, DMOD_MAKE_VERSION(VERSION,0.1), RET, NAME, PARAMS)
#   ifndef DMOD_MODULE_NAME
#       define DMOD_MODULE_NAME        "dmramfs"
#   endif
#   ifndef DMOD_MODULE_VERSION
#       define DMOD_MODULE_VERSION     "0.1"
#   endif
#   define DMOD_AUTHOR_NAME        "Patryk;Kubiak"
#   define DMOD_STACK_SIZE         1024
#   define DMOD_PRIORITY           1
#   define DMOD_MODULE_TYPE        Dmod_ModuleType_Library
#   define DMOD_MANUAL_LOAD        OFF
#else
#  ifdef DMOD_MAL_dmramfs
#  define dmod_dmramfs_mal(VERSION, RET, NAME, PARAMS)            \
                DMOD_MAL_INPUT_API(dmramfs, DMOD_MAKE_VERSION(VERSION,0.1), RET, NAME, PARAMS)
#  define dmod_dmramfs_global_mal(VERSION, RET, NAME, PARAMS)     \
                DMOD_GLOBAL_MAL_INPUT_API(dmramfs, DMOD_MAKE_VERSION(VERSION,0.1), RET, NAME, PARAMS)
#else 
#  define dmod_dmramfs_mal(VERSION, RET, NAME, PARAMS)            \
                DMOD_MAL_OUTPUT_API(dmramfs , DMOD_MAKE_VERSION(VERSION,0.1), RET, NAME, PARAMS)
#  define dmod_dmramfs_global_mal(VERSION, RET, NAME, PARAMS)     \
                DMOD_GLOBAL_MAL_OUTPUT_API(dmramfs, DMOD_MAKE_VERSION(VERSION,0.1), RET, NAME, PARAMS)
#endif
#  define dmod_dmramfs_api(VERSION, RET, NAME, PARAMS)            \
                DMOD_OUTPUT_API(dmramfs, DMOD_MAKE_VERSION(VERSION,0.1), RET, NAME, PARAMS)
#  define dmod_dmramfs_global_api(VERSION, RET, NAME, PARAMS)     \
                DMOD_GLOBAL_OUTPUT_API(dmramfs, DMOD_MAKE_VERSION(VERSION,0.1), RET, NAME, PARAMS)
#  define dmod_dmramfs_dif(VERSION, RET, NAME, PARAMS)            \
                DMOD_DIF_FUNCTION_TYPE_DECLARATION(dmramfs, VERSION, RET, NAME, PARAMS);\
                DMOD_DIF_SIGNATURE_REGISTRATION(dmramfs, NAME, DMOD_MAKE_VERSION(VERSION,0.1))
#  define dmod_dmramfs_dif_api_declaration(VERSION, IMPL_MODULE, RET, NAME, PARAMS)  \
                RET DMOD_MAKE_DIF_API_FUNCTION_NAME(dmramfs, IMPL_MODULE, NAME) PARAMS; \
                _DMOD_DIF_API_REGISTRATION(dmramfs, IMPL_MODULE, DMOD_MAKE_VERSION(VERSION,0.1), NAME) \
                RET DMOD_MAKE_DIF_API_FUNCTION_NAME(dmramfs, IMPL_MODULE, NAME) PARAMS
#endif

#endif // DMOD_MOD_DEFS_H_dmramfs
