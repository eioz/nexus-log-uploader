#pragma once

#define DECLARE_MODULE(class_name, name) namespace addon { extern std::unique_ptr<class_name> name; }
#define IMPLEMENT_MODULE(class_name, name) \
	namespace addon \
	{ \
		std::unique_ptr<class_name> name = std::make_unique<class_name>(); \
	}
