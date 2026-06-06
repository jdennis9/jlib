// -----------------------------------------------------------------------------
// A bunch of boilerplate for my C++ projects.
// -----------------------------------------------------------------------------

#pragma once

#include <stdint.h>
#include <limits.h>

// -----------------------------------------------------------------------------
// Definitions
// -----------------------------------------------------------------------------


#ifdef _WIN32
#define J_PATH_SEP             '\\'
#define J_PATH_SEP_STRING      "\\"
#define J_PATH_SEP_WIDE        L'\\'
#define J_PATH_SEP_WIDE_STRING L"\\"
#else
#define J_PATH_SEP             '/'
#define J_PATH_SEP_STRING      "/"
#define J_PATH_SEP_WIDE        L'/'
#define J_PATH_SEP_WIDE_STRING L"/"
#endif

#define J_ZERO_STRUCT(s) j_zero_memory(s, sizeof(*(s)))
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

#include <assert.h>
#define J_ASSERT(expr) assert(expr)

// -----------------------------------------------------------------------------
// Basic types
// -----------------------------------------------------------------------------

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;
typedef int64_t  s64;
typedef int32_t  s32;
typedef int16_t  s16;
typedef int8_t   s8;
typedef float    f32;
typedef double   f64;
typedef uint64_t usize;
typedef int64_t  ssize;
typedef uint8_t  byte;

// -----------------------------------------------------------------------------
// Defer
// -----------------------------------------------------------------------------
template <typename F>
struct jDefer_Holder_ {
	F f;
	jDefer_Holder_(F f) : f(f) {}
	~jDefer_Holder_() { f(); }
};

template <typename F>
jDefer_Holder_<F> create_defer_(F f) {
	return jDefer_Holder_<F>(f);
}

#define J_DEFER_CAT1_(x, y) x##y
#define J_DEFER_CAT2_(x, y) J_DEFER_CAT1_(x, y)
#define J_DEFER_DECL_(x) J_DEFER_CAT2_(x, __COUNTER__)
#define defer(code) auto J_DEFER_DECL_(defer__) = create_defer_([&](){code;})

// -----------------------------------------------------------------------------
// Core functions
// -----------------------------------------------------------------------------

void j_init();
void j_shutdown();
void j_clear_temp_memory();

// Memory management

void  j_zero_memory(void *mem, usize bytes);
void  j_memory_copy(void *dst, const void *src, usize bytes);
void *j_heap_alloc(usize bytes);
void  j_heap_free(void *ptr);
void *j_virtual_reserve(void *base, usize bytes);
void *j_virtual_commit(void *base, usize bytes);
void *j_virtual_alloc(usize bytes);
void  j_virtual_free(const void *base);
int   j_memcmp(const void *a, const void *b, usize bytes);
void  j_memmove(void *dst, const void *src, usize bytes);


// C strings

usize j_strlen(const char *str);
usize j_strlen_capped(const char *str, usize max);
usize j_strcpy(char *dst, const char *src, usize count); // Makes sure a null is written at the end

// -----------------------------------------------------------------------------
// Logging
// -----------------------------------------------------------------------------

enum Log_Level {
	LOG_LEVEL_ERROR,
	LOG_LEVEL_WARNING,
	LOG_LEVEL_INFO,
	LOG_LEVEL_DEBUG,
	LOG_LEVEL__COUNT,
};

void j_log(Log_Level level, const char *file, int line, const char *fmt, ...);

#define log_debug(...) j_log(LOG_LEVEL_DEBUG  , __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...)  j_log(LOG_LEVEL_WARNING, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...)  j_log(LOG_LEVEL_INFO   , __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) j_log(LOG_LEVEL_ERROR  , __FILE__, __LINE__, __VA_ARGS__)

// -----------------------------------------------------------------------------
// Static functions
// -----------------------------------------------------------------------------

template<typename T>
static T j_pad_to(T x, T align) {
	T m = x % align;
	return m ? x + (align - m) : x;
}

template<typename T> static T j_min(T a, T b) {return a < b ? a : b;}
template<typename T> static T j_max(T a, T b) {return a > b ? a : b;}

template<typename T> static T j_clamp(T x, T vmin, T vmax) {
	if (x < min) return vmin;
	else if (x > max) return vmax;
	return x;
}

// -----------------------------------------------------------------------------
// Allocator
// -----------------------------------------------------------------------------

class jAllocator {
public:
	virtual void *allocate_non_zeroed(usize size, usize alignment) = 0;
	virtual void free(const void *ptr) = 0;
	virtual void free_all() = 0;

	void *allocate(usize size, usize alignment) {
		void *ptr = allocate_non_zeroed(size, alignment);
		if (ptr) {
			j_zero_memory(ptr, size);
			return ptr;
		}

		return nullptr;
	}

	static jAllocator *heap_allocator();
	static jAllocator *virtual_allocator();
	static jAllocator *temp_allocator();
};

// -----------------------------------------------------------------------------
// Slice
// -----------------------------------------------------------------------------

template<typename T>
struct jSlice {
	T    *data = nullptr;
	ssize len  = 0;

	jSlice() {}
	jSlice(T *d, ssize l) : data(d), len(l) {}

	jSlice(jAllocator *a, ssize l) : len(l) {
		data = (T*)a->allocate(l * sizeof(T), alignof(T));
	}

	T& operator[](ssize i) const {
		J_ASSERT(i >= 0 && i < len);
		return data[i];
	}

	T *begin() {return data;}
	T *end() {return &data[len];}
	T const *cbegin() const {return data;}
	T const *cend() const {return &data[len];}

	static ssize copy(jSlice<T> dst, jSlice<T const> src) {
		ssize copy_count = j_min(dst.len, src.len);
		j_memory_copy(dst.data, src.data, copy_count * sizeof(T));
		return copy_count;
	}

	jSlice<T const> as_const() const {
		return jSlice<T const>((T const*)data, len);
	}

	jSlice<T> from(ssize offset) const {
		return jSlice<T>(&data[offset], len - offset);
	}

	jSlice<T> to(ssize end) const {
		return jSlice<T>(data, end);
	}

	jSlice<T> range(ssize start, ssize end) const {
		return jSlice<T>(&data[start], end - start);
	}

	ssize copy_to(jSlice<T> dst) const {
		return jSlice<T>::copy(dst, this->as_const());
	}

	void zero() const {
		j_zero_memory(data, (usize)len);
	}

	void free(jAllocator *a) const {
		a->free((void*)data);
	}

	bool contains(T const& elem) {
		for (T const& v : *this)
			if (v == elem) return true;
		return false;
	}

	jSlice<T> clone(jAllocator *a) {
		jSlice<T> out = jSlice<T>(a, len);
		this->copy_to(out);
		return out;
	}
};

// -----------------------------------------------------------------------------
// Auto array
// -----------------------------------------------------------------------------

template<typename T>
struct jAuto_Array {
	T         *data      = nullptr;
	ssize      len       = 0;
	ssize      cap       = 0;
	jAllocator *allocator = nullptr;

	jAuto_Array() {}
	explicit jAuto_Array(jAllocator *a) : allocator(a) {}

	T& operator[](int i) {return data[i];}
	T const& operator[](int i) const {return data[i];}

	T *begin() {return data;}
	T *end() {return &data[len];}
	T const *cbegin() const {return data;}
	T const *cend() const {return &data[len];}

	jSlice<T> slice() const {return jSlice<T>(data, len);}

	void clear() {len = 0;}
	
	void free() {
		if (allocator && data) allocator->free(data);
		data = nullptr;
	}

	void reserve(ssize want_cap) {
		ssize new_cap = j_pad_to<ssize>(want_cap, j_max<ssize>(4096 / sizeof(T), 8));
		
		if (new_cap <= cap) return;

		if (allocator == nullptr) allocator = jAllocator::heap_allocator();

		T *new_data = (T*)allocator->allocate(sizeof(T) * new_cap, alignof(T));

		if (data) {
			jSlice<T>::copy(jSlice<T>(new_data, new_cap), this->slice().as_const());
		}

		data = new_data;
		cap  = new_cap;
	}

	void resize(ssize want_size) {
		reserve(want_size);
		len = want_size;
	}

	ssize append(T const& elem) {
		ssize index = len;
		resize(len + 1);
		data[index] = elem;
		return index;
	}

	void remove_unordered(ssize index) {
		len -= 1;
		data[index] = data[len];
	}

	void remove_ordered(ssize index) {
		j_memmove(&data[index], &data[index+1], (len - index - 1) * sizeof(T));
		len -= 1;
	}

	T pop() {
		J_ASSERT(len > 0);
		len -= 1;
		return data[len];
	}
};

// -----------------------------------------------------------------------------
// Map
// -----------------------------------------------------------------------------

template<typename K, typename V>
struct jMap {
	K         *keys      = nullptr;
	V         *values    = nullptr;
	s64        len       = 0;
	s64        cap       = 0;
	jAllocator *allocator = nullptr;

	jMap() {}
	explicit jMap(jAllocator *a) : allocator(a) {}
};

// -----------------------------------------------------------------------------
// String
// -----------------------------------------------------------------------------

struct jString {
	char *data = nullptr;
	ssize len  = 0;

	jString() {}
	jString(char *d, ssize l) : data(d), len(l) {}
	jString(const char *s) : data((char*)s) {len = j_strlen(s);}
	explicit jString(jSlice<const char> s) : data((char*)s.data), len(s.len) {}
	explicit jString(jSlice<char> s) : data(s.data), len(s.len) {}
	jString(jAllocator *a, ssize l) {
		data = (char*)a->allocate(l+1, 8);
		len = l;
	}

	bool            operator        ==(const jString& other) const;
	bool            operator        !=(const jString& other) const;
	jSlice<char>    slice           () const;
	void            free            (jAllocator *allocator) const;
	jString         substring       (ssize start, ssize end) const;
	const char     *clone_to_cstring(jAllocator *a) const;
	jString         clone           (jAllocator *a) const;
	ssize           find_last_char  (char32_t c) const;
	ssize           find_last_char  (jSlice<const char32_t> c) const;
	ssize           find_first_char (char32_t c) const;
	ssize           find_first_char (jSlice<const char32_t> c) const;
	jString         trim_spaces     () const;
	jString         trim_chars      (jSlice<const char32_t> c) const;
	jString         trim_char       (char32_t c) const;
	jSlice<jString> split           (jSlice<const char32_t> c, jAllocator *a, int max_elems = -1) const;
	jSlice<jString> split           (char32_t c, jAllocator *a, int max_elems = -1) const;


	static jString join(jSlice<const jString> elems, jString separator, jAllocator *a);
};

class jString_Iterator {
	byte        m_mbstate[64] = {};
	const char *m_buf         = nullptr;
	ssize       m_len         = 0;
	ssize       m_index       = 0;
	ssize       m_last_index  = 0;
public:
	jString_Iterator(jString s) : m_buf(s.data), m_len(s.len) {}

	bool         iterate(char32_t *rune);
	inline ssize byte_index() {return m_index;}
	inline ssize last_byte_index() {return m_last_index;}
	inline bool  is_at_end() {return (m_index >= m_len) || m_buf[m_index] == 0;}
};

// -----------------------------------------------------------------------------
// Scratch allocator
// -----------------------------------------------------------------------------
class jScratch_Allocator : public jAllocator {
	jAuto_Array<void*>  m_leaked;
	byte               *m_arena;
	ssize               m_used;
	ssize               m_size;
	ssize               m_counter;
	jAllocator         *m_arena_allocator;
	jAllocator         *m_backup;
	ssize               m_initial_size;

	struct Header {
		static constexpr u32 SIG = 0x11111111;
		u32 sig;
		s32 padding1;
		s64 size;
		s64 index;
	};

	struct Guard {
		ssize              used_offset;
		ssize              leaked_offset;
		jScratch_Allocator *scratch;

		~Guard() {
			scratch->guard_end(*this);
		}
	};

public:
	void init(s64 size, jAllocator *arena_allocator = nullptr, jAllocator *backup_allocator = nullptr);
	void destroy();
	void *allocate_non_zeroed(usize size, usize alignment);
	void free(const void *ptr);
	void free_all();
	Guard guard();
	void guard_end(const Guard& g);
	s64 capacity() const {return m_size;}
};

// -----------------------------------------------------------------------------
// Filesystem
// -----------------------------------------------------------------------------

// Types

enum jFile_Type {
	FILE_TYPE_REGULAR,
	FILE_TYPE_DIR,
};

struct jFile_Info {
	jFile_Type type;
	usize      size;
	u64        created_time;
	u64        modified_time;
	jString    full_path;
};

class jDirectory_Iterator {
public:
	virtual ~jDirectory_Iterator() {}
	virtual bool iterate(jFile_Info *info_out) = 0;
};

// Paths

jString         j_path_dir        (jString path);
jString         j_path_stem       (jString path);
jString         j_path_ext        (jString path);
jString         j_path_file       (jString path);
jString         j_path_join       (jSlice<jString> elems, jAllocator *allocator);
jSlice<jString> j_path_split      (jString path, jAllocator *allocator);
ssize           j_path_count_parts(jString path);

#ifdef _WIN32
static inline bool j_is_path_sep(char c) {
	return c == '/' || c == '\\';
}

static inline bool j_is_path_sep(wchar_t c) {
	return c == L'/' || c == L'\\';
}

static inline bool j_is_path_sep(char32_t c) {
	return c == '/' || c == '\\';
}
#else
static inline bool j_is_path_sep(char c) {
	return c == '/';
}

static inline bool j_is_path_sep(char32_t c) {
	return c == '/';
}
#endif

// File access

jSlice<byte> j_read_entire_file(jString path);

// File info

bool j_file_info_get       (jString path, jFile_Info *info_out, jAllocator *allocator);
void j_file_info_free      (const jFile_Info& info, jAllocator *allocator);
void j_file_info_slice_free(jSlice<jFile_Info> infos, jAllocator *allocator);

// Directories

jDirectory_Iterator *j_directory_iterate (jString path);
jSlice<jFile_Info>   j_directory_read_all(jString path, jAllocator *allocator);

// Windows-specific helpers

#ifdef _WIN32

// Error checking

#define J_CHECK_HRESULT(expr) j_check_hresult(#expr, expr)
// Takes HRESULT and returns HRESULT
long j_check_hresult(const char *expr, long hr);

// String shenanigans

usize           j_ucs2_to_utf8 (const wchar_t *input, int input_len, char *output, int output_size);
usize           j_utf8_to_ucs2 (const char *input, int input_len, wchar_t *output, int output_size);
usize           j_wstring_len  (const wchar_t *s);
jSlice<wchar_t> j_wstring_slice(wchar_t *s);

#endif

// -----------------------------------------------------------------------------
// ****************************** IMPLEMENTATION *******************************
// -----------------------------------------------------------------------------

#define JLIB_IMPL

#ifdef JLIB_IMPL

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// -----------------------------------------------------------------------------
// OS-specific
// -----------------------------------------------------------------------------

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

long j_check_hresult(const char *expr, long hr) {
	if (!SUCCEEDED(hr)) {
		log_error("%s returned %ld", expr, hr);
	}

	return hr;
}

bool j_win32_file_info_get_(const wchar_t *path, jFile_Info *info_out, jAllocator *allocator) {
	wchar_t          full_path_u16[512] = {};
	WIN32_FIND_DATAW find_data = {};

	HANDLE find_handle = FindFirstFileW(path, &find_data);

	if (find_handle == 0 || find_handle == INVALID_HANDLE_VALUE) return false;

	DWORD full_path_len = GetFullPathNameW(path, ARRAY_LEN(full_path_u16), full_path_u16, nullptr);
	jSlice<char> full_path_buf = jSlice<char>(allocator, full_path_len + 1);

	j_ucs2_to_utf8(full_path_u16, full_path_len, full_path_buf.data, full_path_len + 1);

	// @TODO
	info_out->size          = ((usize)(find_data.nFileSizeHigh) << 32) | (usize)(find_data.nFileSizeLow);
	info_out->created_time  = find_data.ftCreationTime.dwLowDateTime;
	info_out->modified_time = find_data.ftLastWriteTime.dwLowDateTime;
	info_out->full_path     = jString(full_path_buf.data, full_path_buf.len-1);
	
	if (FILE_ATTRIBUTE_DIRECTORY & find_data.dwFileAttributes) {
		info_out->type = FILE_TYPE_DIR;
	}
	else {
		info_out->type = FILE_TYPE_REGULAR;
	}

	return true;
}

class Directory_Iterator_Win32_ : public jDirectory_Iterator {
	static constexpr int MAX_PATH_LEN = 511;

	wchar_t         *m_path_u16          = nullptr;
	int              m_base_len          = {};
	WIN32_FIND_DATAW m_find_data         = {};
	HANDLE           m_find_handle       = nullptr;

public:
	Directory_Iterator_Win32_(jString path) {
		const char *path_cstring = path.clone_to_cstring(jAllocator::temp_allocator());

		m_path_u16 = new wchar_t[MAX_PATH_LEN+1]{};
		m_base_len = j_utf8_to_ucs2(path.data, path.len, m_path_u16, MAX_PATH_LEN);

		for (wchar_t *c = m_path_u16; *c; c++) {
			if (*c == L'/') *c = L'\\';
		}

		if (m_path_u16[m_base_len] != L'\\') {
			m_path_u16[m_base_len] = L'\\';
			m_base_len++;
		}
	}
	
	~Directory_Iterator_Win32_() {
		delete m_path_u16;
	}
	
	bool iterate(jFile_Info *info_out) {
		j_zero_memory(&m_path_u16[m_base_len], sizeof(wchar_t) * (MAX_PATH_LEN - m_base_len));

		if (m_find_handle == nullptr) {
			m_path_u16[m_base_len] = '*';
			m_find_handle = FindFirstFileW(m_path_u16, &m_find_data);
			m_path_u16[m_base_len] = 0;

			if (m_find_handle == INVALID_HANDLE_VALUE || m_find_handle == nullptr) {
				return false;
			}

			if (!wcscmp(m_find_data.cFileName, L"..") && !wcscmp(m_find_data.cFileName, L".")) {
				goto L_FOUND;
			}
		}

		while (FindNextFileW(m_find_handle, &m_find_data)) {
			if (!wcscmp(m_find_data.cFileName, L"..")) continue;
			if (!wcscmp(m_find_data.cFileName, L".")) continue;
			goto L_FOUND;
		}

		return false;

	L_FOUND:
		j_zero_memory(&m_path_u16[m_base_len], sizeof(wchar_t) * (MAX_PATH_LEN - m_base_len));
		wcsncpy(&m_path_u16[m_base_len], m_find_data.cFileName, MAX_PATH_LEN - m_base_len);

		j_win32_file_info_get_(m_path_u16, info_out, jAllocator::heap_allocator());

		return true;
	}
};

void *j_sys_virtual_reserve_(void *base, usize bytes) {
	return VirtualAlloc(base, bytes, MEM_RESERVE, 0);
}

void *j_sys_virtual_commit_(void *base, usize bytes) {
	return VirtualAlloc(base, bytes, MEM_COMMIT, PAGE_READWRITE);
}

void *j_sys_virtual_alloc_(usize bytes) {
	return VirtualAlloc(nullptr, bytes, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
}

void j_sys_virtual_free_(const void *ptr) {
	VirtualFree((void*)ptr, 0, MEM_RELEASE);
}

bool j_sys_file_info_get_(jString path, jFile_Info *info_out, jAllocator *allocator) {
	jSlice<wchar_t> path_u16 = jSlice<wchar_t>(jAllocator::temp_allocator(), path.len + 1);
	defer(path_u16.free(jAllocator::temp_allocator()));

	j_utf8_to_ucs2(path.data, path.len, path_u16.data, path_u16.len);
	
	return j_win32_file_info_get_(path_u16.data, info_out, allocator);
}

jDirectory_Iterator *j_sys_directory_iterate_(jString path) {
	return new Directory_Iterator_Win32_(path);
}

#endif

// -----------------------------------------------------------------------------
// Logging
// -----------------------------------------------------------------------------

void j_log(Log_Level level, const char *file, int line, const char *fmt, ...) {
	const char *headers[LOG_LEVEL__COUNT];
	headers[LOG_LEVEL_DEBUG]   = "[DEBUG  ] ";
	headers[LOG_LEVEL_WARNING] = "[WARNING] ";
	headers[LOG_LEVEL_ERROR]   = "[ERROR  ] ";
	headers[LOG_LEVEL_DEBUG]   = "[INFO   ] ";

	printf("%s(%4d) %s", file, line, headers[level]);

	va_list va;
	va_start(va, fmt);
	vprintf(fmt, va);
	va_end(va);
}

// -----------------------------------------------------------------------------
// Core
// -----------------------------------------------------------------------------

void j_init() {}
void j_shutdown() {}

void j_clear_temp_memory() {
	jAllocator::temp_allocator()->free_all();
}

// -----------------------------------------------------------------------------
// Memory management
// -----------------------------------------------------------------------------

void j_zero_memory(void *mem, usize bytes) {
	memset(mem, 0, bytes);
}

void j_memory_copy(void *dst, const void *src, usize bytes) {
	memcpy(dst, src, bytes);
}

void *j_heap_alloc(usize bytes) {
	return malloc(bytes);
}

void j_heap_free(const void *ptr) {
	if (ptr) free((void*)ptr);
}

void *j_virtual_reserve(void *base, usize bytes) {
	return j_sys_virtual_reserve_(base, bytes);
}

void *j_virtual_commit(void *base, usize bytes) {
	return j_sys_virtual_commit_(base, bytes);
}

void *j_virtual_alloc(usize bytes) {
	return j_sys_virtual_alloc_(bytes);
}

void j_virtual_free(const void *base) {
	j_sys_virtual_free_(base);
}

int j_memcmp(const void *a, const void *b, usize bytes) {
	return memcmp(a, b, bytes);
}

void j_memmove(void *dst, const void *src, usize bytes) {
	memmove(dst, src, bytes);
}

// -----------------------------------------------------------------------------
// Windows string shenanigans
// -----------------------------------------------------------------------------

#ifdef _WIN32
usize j_ucs2_to_utf8(const wchar_t *input, int input_len, char *output, int output_size) {
	return WideCharToMultiByte(CP_UTF8, 0, input, input_len, output, output_size, nullptr, nullptr);
}

usize j_utf8_to_ucs2(const char *input, int input_len, wchar_t *output, int output_size) {
	return MultiByteToWideChar(CP_UTF8, 0, input, input_len, output, output_size);
}

usize j_wstring_len(const wchar_t *s) {
	usize len = 0;
	for (const wchar_t *c = s; *c; c++, len++) {}
	return len;
}

jSlice<wchar_t> j_wstring_slice(wchar_t *s) {
	return jSlice<wchar_t>(s, j_wstring_len(s));
}
#endif

// -----------------------------------------------------------------------------
// C strings
// -----------------------------------------------------------------------------

usize j_strlen(const char *str) {
	return (usize)strlen(str);
}

usize j_strlen_capped(const char *str, usize max) {
	usize i = 0;
	for (const char *c = str; *c && i < max; c++, i++) {}
	return i;
}

usize j_strcpy(char *dst, const char *src, usize count) {
	usize i = 0;

	for (const char *in = src; *in && i < (count - 1); i++, in++)
		dst[i] = *in;

	dst[i] = 0;

	return i;
}

// -----------------------------------------------------------------------------
// Filesystem
// -----------------------------------------------------------------------------

static inline ssize j_path_find_last_sep_(jString path) {
#ifdef _WIN32
	char32_t seps[] = {'/', '\\'};
	return path.find_last_char(jSlice<const char32_t>(seps, ARRAY_LEN(seps)));
#else
	return path.find_last_char('/');
#endif
}

jString j_path_dir(jString path) {
	ssize sep_index = j_path_find_last_sep_(path);

	if (sep_index < 0) return {};
	else if (sep_index == 0) return path.substring(0, 1);
	return path.substring(0, sep_index);
}

jString j_path_stem(jString path) {
	jString file = j_path_file(path);
	ssize ext_start = file.find_last_char(U'.');

	if (ext_start > 0) return file.substring(0, ext_start);
	else return file;
}

jString j_path_ext(jString path) {
	ssize dot_index = path.find_last_char(U'.');
	if (dot_index < 0) return {};
	return path.substring(dot_index, path.len);
}

jString j_path_file(jString path) {
	ssize sep_index = j_path_find_last_sep_(path);
	if (sep_index < 0) return path;
	return path.substring(sep_index+1, path.len);
}

jString j_path_join(jSlice<const jString> elems, jAllocator *allocator) {
	return jString::join(elems, J_PATH_SEP_STRING, allocator);
}

jSlice<jString> j_path_split(jString path, jAllocator *allocator) {
#ifdef _WIN32
	return path.split(jSlice<const char32_t>(U"\\/", 2), allocator);
#else
	return path.split(U'/', allocator);
#endif
}

// File access

jSlice<byte> j_read_entire_file(jString path) {
	return {};
}

// File info

bool j_file_info_get(jString path, jFile_Info *info_out, jAllocator *allocator) {
	return j_sys_file_info_get_(path, info_out, allocator);
}

void j_file_info_free(const jFile_Info& info, jAllocator *allocator) {
	info.full_path.slice().free(allocator);
}

void j_file_info_slice_free(jSlice<jFile_Info> infos, jAllocator *allocator) {
	for (const jFile_Info& f : infos) j_file_info_free(f, allocator);
}

// Directories

jDirectory_Iterator *j_directory_iterate(jString path) {
	if (path == "") return nullptr;
	return j_sys_directory_iterate_(path);
}

jSlice<jFile_Info> j_directory_read_all(jString path, jAllocator *allocator) {
	jDirectory_Iterator *iter = j_directory_iterate(path);
	jAuto_Array<jFile_Info> buf = {};
	jFile_Info info = {};

	if (!iter) return {};

	while (iter->iterate(&info)) {
		buf.append(info);
	}
	delete iter;

	jSlice<jFile_Info> ret = buf.slice().clone(allocator);
	buf.free();

	return ret;
}

// -----------------------------------------------------------------------------
// Strings
// -----------------------------------------------------------------------------
#include <cuchar>

jSlice<char> jString::slice() const {
	return jSlice<char>(data, len);
}

// Assumes the string was allocated with this allocator
void jString::free(jAllocator *allocator) const {slice().free(allocator);}

bool jString::operator==(const jString& other) const {
	if (other.len != len) return false;
	return other.data == data || !j_memcmp(other.data, data, len);
}

bool jString::operator!=(const jString& other) const {return !(other == *this);}

jString jString::substring(ssize start, ssize end) const {
	return jString(this->slice().range(start, end));
}

const char *jString::clone_to_cstring(jAllocator *a) const {
	jSlice<char> dst = jSlice<char>(a, len + 1);
	jSlice<char>::copy(dst, this->slice().as_const());
	return dst.data;
}

jString jString::clone(jAllocator *a) const {
	jSlice<char> dst = jSlice<char>(a, len);
	jSlice<char>::copy(dst, this->slice().as_const());
	return jString(dst);
}

ssize jString::find_last_char(char32_t c) const {
	jString_Iterator iter(*this);
	char32_t rune;
	ssize ret = -1;

	while (iter.iterate(&rune)) {
		if (rune == c) ret = iter.last_byte_index();
	}

	return ret;
}

ssize jString::find_last_char(jSlice<const char32_t> c) const {
	jString_Iterator iter(*this);
	char32_t rune;
	ssize ret = -1;

	while (iter.iterate(&rune)) {
		if (c.contains(rune)) ret = iter.last_byte_index();
	}

	return ret;
}

ssize jString::find_first_char(char32_t c) const {
	jString_Iterator iter(*this);
	char32_t rune;
	ssize ret = -1;

	while (iter.iterate(&rune)) {
		if (rune == c) return iter.last_byte_index();
	}
	return -1;
}

ssize jString::find_first_char(jSlice<const char32_t> c) const {
	jString_Iterator iter(*this);
	char32_t rune;
	ssize ret = -1;

	while (iter.iterate(&rune)) {
		if (c.contains(rune)) ret = iter.last_byte_index();
	}

	return ret;
}

jString jString::trim_spaces() const {
	char32_t spaces[] = {' ', '\t'};
	return this->trim_chars(jSlice<const char32_t>(spaces, ARRAY_LEN(spaces)));
}

jString jString::trim_chars(jSlice<const char32_t> c) const {
	jString_Iterator iter(*this);
	char32_t rune;
	ssize prev_byte_index = 0;
	ssize first_ok = -1;
	ssize last_ok = -1;
	bool have_first = false;

	while (iter.iterate(&rune)) {
		bool ok = !c.contains(rune);

		if (ok) {
			if (!have_first) {
				have_first = true;
				first_ok = prev_byte_index;
			}

			last_ok = iter.byte_index();
		}

		prev_byte_index = iter.byte_index();
	}

	if (!have_first) return {};
	return this->substring(first_ok, last_ok);
}

jString jString::trim_char(char32_t c) const {
	return this->trim_chars(jSlice<const char32_t>(&c, 1));
}

jSlice<jString> jString::split(jSlice<const char32_t> c, jAllocator *allocator, int max_elems) const {
	jAuto_Array<jString> buf(allocator);
	jString_Iterator iter(*this);
	char32_t rune;
	bool was_splitter = true;
	ssize split_start = 0;
	bool split_start_changed_after_split = false;

	while (iter.iterate(&rune)) {
		bool is_splitter = c.contains(rune);

		if (was_splitter && !is_splitter) {
			split_start = iter.last_byte_index();
		}

		if ((!was_splitter && is_splitter)) {
			buf.append(this->substring(split_start, iter.last_byte_index()));
			if (buf.len == max_elems) break;
		}

		was_splitter = is_splitter;
	}

	if (!was_splitter)
		buf.append(this->substring(split_start, iter.byte_index()));

	return buf.slice();
}

jSlice<jString> jString::split(char32_t c, jAllocator *a, int max_elems) const {
	return this->split(jSlice<const char32_t>(&c, 1), a, max_elems);
}

jString jString::join(jSlice<const jString> elems, jString separator, jAllocator *allocator) {
	jSlice<char> buf;
	ssize written;
	ssize len = 0;

	for (ssize i = 0; i < elems.len; i++) {
		len += elems[i].len;
		if (i != (elems.len-1)) len += separator.len;
	}

	buf = jSlice<char>(allocator, len+1);
	written = 0;

	for (ssize i = 0; i < elems.len; i++) {
		elems[i].slice().copy_to(buf.from(written));
		written += elems[i].len;

		if (i != (elems.len-1)) {
			separator.slice().copy_to(buf.from(written));
			written += separator.len;
		}
	}

	return jString(buf.data, buf.len-1);
}

bool jString_Iterator::iterate(char32_t *rune) {
	if (*m_buf == 0 || m_index >= m_len) return false;
	m_last_index = m_index;

	int x = std::mbrtoc32(rune, m_buf, 4, (mbstate_t*)m_mbstate);
	if (x <= 0) return false;

	m_buf += x;
	m_index += x;

	return true;
}

// -----------------------------------------------------------------------------
// Allocators
// -----------------------------------------------------------------------------

jAllocator *jAllocator::heap_allocator() {
	class Heap_Allocator : public jAllocator {
		void *allocate_non_zeroed(usize size, usize alignment) {
			return _aligned_malloc(size, alignment);
		}

		void free(const void *ptr) {
			_aligned_free((void*)ptr);
		}

		void free_all() {
		}
	};

	static Heap_Allocator heap_allocator;
	return &heap_allocator;
}

jAllocator *jAllocator::virtual_allocator() {
	class Virtual_Allocator : public jAllocator {
		void *allocate_non_zeroed(usize size, usize alignment) {
			return j_virtual_alloc(size);
		}

		void free(const void *ptr) {
			j_virtual_free(ptr);
		}

		void free_all() {
		}
	};

	static Virtual_Allocator virtual_allocator;
	return &virtual_allocator;
}

jAllocator *jAllocator::temp_allocator() {
	class Temp_Allocator : public jAllocator {
		jAuto_Array<void*> m_allocations;
	public:
		void *allocate_non_zeroed(usize size, usize alignment) {
			void *ptr = j_heap_alloc(size);
			if (!ptr) return nullptr;
			
			return ptr;
		}

		void free(const void *ptr) {
			j_heap_free(ptr);
		}

		void free_all() {
			for (const void *p : m_allocations) j_heap_free(p);
			m_allocations.free();
		}
	};

	static Temp_Allocator temp_allocator;
	return &temp_allocator;
}

// -----------------------------------------------------------------------------
// Scratch allocator
// -----------------------------------------------------------------------------

void jScratch_Allocator::init(
	s64 size, jAllocator *arena_allocator,
	jAllocator *backup_allocator
) {
	m_arena_allocator = arena_allocator  ? arena_allocator : jAllocator::heap_allocator();
	m_backup          = backup_allocator ? backup_allocator : jAllocator::heap_allocator();
	m_arena           = jSlice<byte>(m_arena_allocator, size).data;
	m_size            = size;
	m_initial_size    = size;
}

void jScratch_Allocator::destroy() {
	this->free_all();
	if (m_arena_allocator) m_arena_allocator->free(m_arena);
	m_arena = nullptr;
	m_size = 0;
	m_initial_size = 0;
}

void *jScratch_Allocator::allocate_non_zeroed(usize size, usize alignment) {
	s64 padded_size = j_pad_to<ssize>(size + sizeof(Header), j_max(alignment, alignof(Header)));

	if (padded_size + m_used > m_size) {
		if (m_backup) {
			void *p = m_backup->allocate(size, alignment);
			m_leaked.append(p);
			log_warn("Scratch allocator %p (%lld) used backup to allocate %p", this, size, p);
			return p;
		}
		return nullptr;
	}

	u8 *ptr = &m_arena[m_used];
	Header *header = (Header*) ptr;

	header->sig   = Header::SIG;
	header->size  = padded_size;
	header->index = m_counter;

	m_counter += 1;
	m_used += padded_size;

	return header + 1;
}

void jScratch_Allocator::free(const void *raw_ptr) {
	u8 *ptr = (u8*)raw_ptr;
	Header *header = (Header*)(ptr - sizeof(Header));

	J_ASSERT(header->sig == Header::SIG);
	if (header->sig != Header::SIG) return;

	// Was this the last allocation?
	if (header->index == (m_counter - 1)) {
		m_used -= header->size;
	}
	// Is it leaked
	for (int i = 0; i < m_leaked.len; i++) {
		if (raw_ptr == m_leaked[i]) {
			m_backup->free((void*)raw_ptr);
			m_leaked.remove_unordered(i);
			return;
		}
	}
}

void jScratch_Allocator::free_all() {
	m_used = 0;

	if (m_leaked.len != 0) J_ASSERT(m_backup != nullptr);

	for (void *leaked : m_leaked) {
		m_backup->free(leaked);
	}

	m_leaked.clear();
}

jScratch_Allocator::Guard jScratch_Allocator::guard() {
	Guard g = {};
	g.leaked_offset = m_leaked.len;
	g.used_offset = m_used;
	g.scratch = this;
	return g;
}

void jScratch_Allocator::guard_end(const Guard& g) {
	m_used = g.used_offset;

	ssize leak_count = m_leaked.len - g.leaked_offset;

	for (int i = 0; i < leak_count; ++i) {
		m_backup->free(m_leaked.pop());
	}
}

#endif
