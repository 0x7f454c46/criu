#include <dlfcn.h>
#include <link.h>
#include <linux/limits.h>
#include <stdio.h>
#include <unistd.h>

#include "common/compiler.h"
#include "cr-libs.h"
#include "log.h"

struct so_desc {
	const char	*so_name;
	const char	*warning;
	int	lookup_version; /* On load major:minor:patch will be set */

	/* Set up during on load */
	void *so_handle;
	unsigned major, minor, patch;
};
static struct so_desc crlibs[SHARED_LIB_LAST] = {
	{ "libbsd.so", "Can't set title and using self-made strlcpy()" },
};

static int link_version(const char *link, unsigned *major,
			unsigned *minor, unsigned *patch)
{
	char buf[PATH_MAX];
	const char *version;
	const char *lib;
	ssize_t err;

	err = readlink(link, buf, ARRAY_SIZE(buf));
	if (err < 0) {
		/* It's not a link but a shared lib itself */
		if (errno == EINVAL)
			lib = link;
		pr_debug("Failed to lookup %s link\n", link);
		return -1;
	} else {
		lib = buf;
	}

	/*
	 * Looking up the version part of filename, i.e:
	 * "libnetfilter_conntrack.so.3.7.0" => "3.7.0"
	 */
	version = strstr(lib, ".so.");

	if (version == NULL)
		return -1;
	if (sscanf(version, ".so.%d.%d.%d", major, minor, patch) != 3)
		return -1;

	return 0;
}

static int lib_resolve_version(struct so_desc *lib)
{
	struct link_map *lm;

	if (dlinfo(lib->so_handle, RTLD_DI_LINKMAP, &lm) || lm->l_name == NULL) {
		pr_debug("Can't get %s's link_map: %s\n",
			lib->so_name, dlerror());
		return -1;
	}

	if (link_version(lm->l_name, &lib->major, &lib->minor, &lib->patch)) {
		pr_debug("Can't find %s shared lib version from filename %s\n",
			lib->so_name, lm->l_name);
		return -1;
	}

	return 0;
}

void shared_libs_load(void)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(crlibs); i++) {
		struct so_desc *lib = &crlibs[i];

		lib->so_handle = dlopen(lib->so_name, RTLD_LAZY);
		if (lib->so_handle == NULL) {
			pr_warn_once("CRIU functionality may be limited\n");
			pr_warn("Failed to load shared library `%s': %s\n",
				lib->so_name, dlerror());
			if (lib->warning)
				pr_warn("%s: %s\n", lib->so_name, lib->warning);
			continue;
		}

		/*
		 * Resolve libraries versions during CRIU load to minimize
		 * time interval between mmap() and path lookup.
		 * To shorten file-system race when CRIU loaded one library,
		 * but the disc content has changed (i.e. by packet manager).
		 */
		if (lib->lookup_version && lib_resolve_version(lib)) {
			pr_err("Failed to resolve %s version\n", lib->so_name);
			dlclose(lib->so_handle);
			lib->so_handle = NULL;
			continue;
		}

		pr_debug("Loaded `%s' succesfully\n", lib->so_name);
	}
}

void shared_libs_unload(void)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(crlibs); i++) {
		struct so_desc *lib = &crlibs[i];

		if (lib->so_handle == NULL)
			continue;

		if (dlclose(lib->so_handle))
			pr_warn("Failed to unload `%s': %s\n",
				lib->so_name, dlerror());
		lib->so_handle = NULL;
	}
}

void *shared_libs_lookup(enum shared_libs id, const char *func)
{
	struct so_desc *lib;
	void *ret;

	if (id > SHARED_LIB_LAST) {
		pr_err("BUG: shared library id is too big: %u\n", id);
		return NULL;
	}

	lib = &crlibs[id];
	if (lib->so_handle == NULL)
		return NULL;

	ret = dlsym(lib->so_handle, func);
	if (ret == NULL)
		pr_debug("Can't find `%s' function in %s\n", func, lib->so_name);

	return ret;
}

int shared_libs_get_version(enum shared_libs id, unsigned *major,
				   unsigned *minor, unsigned *patch)
{
	struct so_desc *lib;

	if (id > SHARED_LIB_LAST) {
		pr_err("BUG: shared library id is too big: %u\n", id);
		return -1;
	}

	lib = &crlibs[id];
	if (lib->so_handle == NULL)
		return -1;

	return 0;
}
