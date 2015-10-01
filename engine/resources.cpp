#include "resources.hpp"
#include "image.hpp"
#include "geometry.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#if defined(_WIN32) || defined(WIN32)
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#else
	#include <dirent.h>
	#include <sys/stat.h>
#endif


static bool fileExists(const string& path)
{
	std::ifstream f(path);
	return f.good();
}

static string getCanonicalDir(const string& path)
{
	// TODO: Not portable
	return path.back() == '/' ? path : (path + "/");
}

// Based on http://stackoverflow.com/a/1932861
static void findFiles(std::vector<string>& files, const string& path, const string& filter = "")
{
#if defined(_WIN32) || defined(WIN32)
	HANDLE dir;
	WIN32_FIND_DATA file_data;

	if ((dir = FindFirstFile((path + "*").c_str(), &file_data)) == INVALID_HANDLE_VALUE)
		return; // No files found

	do {
		const string file_name = file_data.cFileName;
		const string full_file_name = path + file_name;

		if (file_name[0] == '.')
			continue;

		if (!filter.empty() && file_name.find(filter) == string::npos)
			continue;

		if ((file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) // Directory?
			continue;

		files.push_back(file_name);
	} while (FindNextFile(dir, &file_data));

	FindClose(dir);
#else
	struct dirent *ent;
	struct stat st;

	DIR* dir = opendir(path.c_str());
	while ((ent = readdir(dir)) != NULL) {
		const string file_name = ent->d_name;
		const string full_file_name = path + file_name;

		if (file_name[0] == '.')
			continue;

		if (!filter.empty() && file_name.find(filter) == string::npos)
			continue;

		if (stat(full_file_name.c_str(), &st) == -1)
			continue;

		if ((st.st_mode & S_IFDIR) != 0) // Directory?
			continue;

		files.push_back(file_name);
	}
	closedir(dir);
#endif
}


Resources::Resources()
{
}

Resources::~Resources()
{
}

void Resources::reset()
{
	// Paths are not dropped
	m_texts.clear();
	m_images.clear();
	m_geoms.clear();
	logDebug("Resource cache dropped");
}

void Resources::addPath(const string& path)
{
	m_paths.insert(m_paths.begin(), getCanonicalDir(path));
}

void Resources::removePath(const string& path)
{
	m_paths.erase(std::find(m_paths.begin(), m_paths.end(), getCanonicalDir(path)));
}

string Resources::findPath(const string& path) const
{
	for (auto& it : m_paths) {
		string fullPath = it + path;
		if (fileExists(fullPath))
			return fullPath;
	}
	logError("Could not find file \"%s\" from any of the resource paths", path.c_str());
	ASSERT(0);
	return "";
}

std::vector<string> Resources::listFiles(const string& path, const string& filter) const
{
	std::vector<string> files;
	for (auto& it : m_paths) {
		string fullPath = it + getCanonicalDir(path);
		findFiles(files, fullPath, filter);
	}
	return files;
}

string Resources::getText(const string& path, CachePolicy cache)
{
	if (cache == USE_CACHE) {
		auto it = m_texts.find(path);
		if (it != m_texts.end())
			return it->second;
	}
	std::ifstream f(findPath(path));
	std::stringstream buffer;
	buffer << f.rdbuf();
	if (cache == USE_CACHE)
		m_texts[path] = buffer.str();
	return buffer.str();
}

Image* Resources::getImage(const string& path)
{
	auto& ptr = m_images[path];
	if (!ptr) ptr.reset(new Image(findPath(path), 4));
	return ptr.get();
}

Geometry* Resources::getGeometry(const string& path)
{
	auto& ptr = m_geoms[path];
	if (!ptr) ptr.reset(new Geometry(findPath(path)));
	return ptr.get();
}

Geometry* Resources::getHeightmap(const string& path)
{
	auto& ptr = m_geoms[path];
	if (!ptr) ptr.reset(new Geometry(*getImage(findPath(path))));
	return ptr.get();
}
