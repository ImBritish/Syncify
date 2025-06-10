#pragma once

#include "../JavaEnum.h"

class DisplayMode : public JavaEnum<DisplayMode>
{
public:
	static const DisplayMode Simple;
	static const DisplayMode Compact;
	static const DisplayMode Extended;

private:
	DisplayMode(std::string name, int ordinal)
		: JavaEnum(std::move(name), ordinal) {
	}
};

const DisplayMode DisplayMode::Simple = DisplayMode("Simple", 0);
const DisplayMode DisplayMode::Compact = DisplayMode("Compact", 1);
const DisplayMode DisplayMode::Extended = DisplayMode("Extended", 2);