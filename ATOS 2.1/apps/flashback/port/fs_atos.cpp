// FileSystem over ATOS paths (replaces fs.cpp, which scans directories with
// dirent/stat).  ATOS FAT names are 8.3 and case-insensitive; the kernel
// resolves paths relative to the working directory, max 63 characters.
#include "fs.h"
#include "atos_sys.h"
#include "atos_port.h"

struct FileSystem_impl {
	char _root[96];
};

FileSystem::FileSystem(const char *dataPath) {
	_impl = new FileSystem_impl;
	atos_path(_impl->_root, sizeof(_impl->_root), dataPath);
	const size_t n = strlen(_impl->_root);
	if (n && _impl->_root[n - 1] == '\\') {
		_impl->_root[n - 1] = 0;
	}
}

FileSystem::~FileSystem() {
	delete _impl;
}

char *FileSystem::findPath(const char *filename) const {
	char joined[160], path[96];
	if (_impl->_root[0]) {
		snprintf(joined, sizeof(joined), "%s\\%s", _impl->_root, filename);
	} else {
		snprintf(joined, sizeof(joined), "%s", filename);
	}
	atos_path(path, sizeof(path), joined);
	if (strlen(path) > 63) {
		return 0;
	}
	uint32_t info[2];
	if (atos_stat(path, info) < 0 || (info[1] & 0x10)) {
		return 0;
	}
	return strdup(path);
}

bool FileSystem::exists(const char *filename) const {
	char *path = findPath(filename);
	if (path) {
		free(path);
		return true;
	}
	return false;
}
