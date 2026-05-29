#pragma once
#include <cstdio>
#include <unordered_map>
#include <string>
#include <windows.h>

// Helper to get host QPC frequency (call once, cached)
inline int64_t GetQPCFreq() {
	static int64_t freq = 0;
	if (!freq) {
		LARGE_INTEGER f;
		QueryPerformanceFrequency(&f);
		freq = f.QuadPart;
	}
	return freq;
}

namespace FuncProfile {
	struct Entry {
		int64_t total_ticks = 0;
		int     call_count = 0;
	};
	inline std::unordered_map<std::string, Entry>& map() {
		static std::unordered_map<std::string, Entry> s_map;
		return s_map;
	}
	inline void add(const char* name, int64_t ticks) {
		auto& e = map()[name];
		e.total_ticks += ticks;
		e.call_count++;
	}
	inline void dump() {
		static bool s_dumped = false;
		if (s_dumped) return;
		s_dumped = true;
		FILE* f = fopen("C:\\temp\\cxbx_func.txt", "w");
		if (!f) return;
		int64_t freq = GetQPCFreq();
		for (auto& [name, e] : map()) {
			double ms = (double)e.total_ticks * 1000.0 / (double)freq;
			double us_per_call = e.call_count ? (ms * 1000.0 / e.call_count) : 0.0;
			fprintf(f, "%s: %lld calls, %.3f ms total, %.3f us/call\n",
				name.c_str(), (long long)e.call_count, ms, us_per_call);
		}
		fclose(f);
	}
	class Scoped {
		const char* m_name;
		LARGE_INTEGER m_start;
	public:
		Scoped(const char* name) : m_name(name) { QueryPerformanceCounter(&m_start); }
		~Scoped() {
			LARGE_INTEGER end;
			QueryPerformanceCounter(&end);
			add(m_name, end.QuadPart - m_start.QuadPart);
		}
	};
}

#define FUNC_PROFILE(name) FuncProfile::Scoped _fpscope(name)
#define FUNC_PROFILE_DUMP() FuncProfile::dump()
