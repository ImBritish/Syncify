#pragma once
#include <string>
#include <vector>

template <typename Derived>
class JavaEnum {
public:
    const std::string& name() const { return _name; }
    int ordinal() const { return _ordinal; }

    static const std::vector<const Derived*>& values() {
        return _values();
    }

    bool operator==(const Derived& other) const {
        return _name == other.name() && _ordinal == other.ordinal();
    }
    bool operator!=(const Derived& other) const {
        return !(*this == other);
    }

    bool operator==(const JavaEnum& other) const {
        return _name == other._name && _ordinal == other._ordinal;
    }
    bool operator!=(const JavaEnum& other) const {
        return !(*this == other);
	}

protected:
    JavaEnum(std::string name, int ordinal)
        : _name(std::move(name)), _ordinal(ordinal)
    {
        _values().push_back(static_cast<Derived*>(this));
    }

private:
    std::string _name;
    int _ordinal;

    static std::vector<const Derived*>& _values() {
        static std::vector<const Derived*> instances;
        return instances;
    }
};
