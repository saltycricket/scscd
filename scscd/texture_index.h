#pragma once

#include "scscd.h"

// Maintains an index of texture paths by BA2 archive file. The first time contains() is called,
// the TESFile filename is used to construct a '[plugin] - Textures.ba2' filename, and that BA2
// is scanned. All filenames within it are indexed.
//
// This is needed because the engine-provided BSResourceNiBinaryStream does not scan texture archives,
// only general ones. Until some engine-provided substitute can be discovered, we have to index them
// ourselves if we want to know if a texture exists or not.
class TextureIndex {
	std::unordered_set<std::string> index;

public:
	bool contains(const std::string& path);
};
