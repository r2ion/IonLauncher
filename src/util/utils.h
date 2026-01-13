#pragma once

void RemoveAsciiControlSequences(char* str, bool allow_color_codes);
bool IsBadReadPtr2(void* p);
bool IsBadStringPtr2(const char* str);

template <typename T> class ScopeGuard
{
public:
	auto operator=(ScopeGuard&) = delete;
	ScopeGuard(ScopeGuard&) = delete;

	ScopeGuard(T callback)
		: m_callback(callback)
	{
	}
	~ScopeGuard()
	{
		if (!m_dismissed)
			m_callback();
	}
	void Dismiss() { m_dismissed = true; }

private:
	bool m_dismissed = false;
	T m_callback;
};

template <class T>
inline void HashCombine(std::size_t& seed, const T& v) {
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

namespace std {
    template <>
    struct hash<std::pair<std::string, std::string>> {
        std::size_t operator()(const std::pair<std::string, std::string>& p) const {
            std::size_t seed = 0;
            HashCombine(seed, p.first);
            HashCombine(seed, p.second);
            return seed;
        }
    };
}
