#pragma once

#include <string>

#include "../../external/bakkes/logging/logging.h"

namespace Log
{
	template <typename... Args>
	void Info(std::string_view format_str, Args&&... args)
	{
		LOG(std::string("[Syncify/Info]: ") + std::string(format_str), std::forward<Args>(args)...);
	}

	template <typename... Args>
	void Error(std::string_view format_str, Args&&... args)
	{
		LOG(std::string("[Syncify/Error]: ") + std::string(format_str), std::forward<Args>(args)...);
	}

	template <typename... Args>
	void Warning(std::string_view format_str, Args&&... args)
	{
		LOG(std::string("[Syncify/Warning]: ") + std::string(format_str), std::forward<Args>(args)...);
	}
}