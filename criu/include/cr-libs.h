#ifndef __CR_LIBS_H__
#define __CR_LIBS_H__

#ifdef CR_NOGLIBC
#error Shared libraries are not supported for PIEs
#endif

enum shared_libs {
	LIB_BSD = 0,
	SHARED_LIB_LAST,
};

/* Called on init to dlopen() all .so libraries */
extern void shared_libs_load(void);

/* Called on exit to call all libraries destructors */
extern void shared_libs_unload(void);

/*
 * Lookup a function in a shared library.
 * Returns NULL on failure.
 */
extern void *shared_libs_lookup(enum shared_libs id, const char *func);

#define shared_libs_lookup_once(id, func)			\
({								\
	static int searched;					\
	static void *ret;					\
	if (!searched) {					\
		ret = shared_libs_lookup(id, func);		\
		searched = 1;					\
	}							\
	ret;							\
})

/*
 * Get the version of loaded shared library.
 * Returns 0 on success.
 */
extern int shared_libs_get_version(enum shared_libs id, unsigned *major,
				   unsigned *minor, unsigned *patch);

#endif /* __CR_LIBS_H__ */
