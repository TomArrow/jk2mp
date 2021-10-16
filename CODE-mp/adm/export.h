
#ifndef ADM_EXPORT_H
#define ADM_EXPORT_H

#ifdef ADM_STATIC_DEFINE
#  define ADM_EXPORT
#  define ADM_NO_EXPORT
#else
#  ifndef ADM_EXPORT
#    ifdef adm_EXPORTS
        /* We are building this library */
#      define ADM_EXPORT 
#    else
        /* We are using this library */
#      define ADM_EXPORT 
#    endif
#  endif

#  ifndef ADM_NO_EXPORT
#    define ADM_NO_EXPORT 
#  endif
#endif

#ifndef ADM_DEPRECATED
#  define ADM_DEPRECATED __declspec(deprecated)
#endif

#ifndef ADM_DEPRECATED_EXPORT
#  define ADM_DEPRECATED_EXPORT ADM_EXPORT ADM_DEPRECATED
#endif

#ifndef ADM_DEPRECATED_NO_EXPORT
#  define ADM_DEPRECATED_NO_EXPORT ADM_NO_EXPORT ADM_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef ADM_NO_DEPRECATED
#    define ADM_NO_DEPRECATED
#  endif
#endif

#endif /* ADM_EXPORT_H */
