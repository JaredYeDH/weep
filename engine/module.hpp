#include "common.hpp"

struct Module {
	typedef void (*ModuleFunc)(uint msg, void* param);

	Module(const string& name);
	~Module();

	ModuleFunc func = nullptr;
	bool enabled = true;
	void* handle = nullptr;
	string name;
};


class Modules : public std::map<string, Module>
{
public:
	void load(const Json& modules);
	void reload(const string& name);

	void call(uint msg, void* param = nullptr);

	// TODO: Probably should remove...
	void init(Entities& entities);
	void deinit(Entities& entities);
	void update(Entities& entities);
};