#pragma once

#include "../JavaEnum.h"

class SizeMode : public JavaEnum<SizeMode>
{
public:
	static const SizeMode Static;
	static const SizeMode Dynamic;

private:
	SizeMode(std::string name, int ordinal)
		: JavaEnum(std::move(name), ordinal) {
	}
};

const SizeMode SizeMode::Static = SizeMode("Static", 0);
const SizeMode SizeMode::Dynamic = SizeMode("Dynamic", 1);