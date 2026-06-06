#define JLIB_IMPLEMENTATION
#include "jlib.h"

static inline void print_header(const char *h) {
	const char *sep = "********************************************";
	puts("");
	puts(sep);
	puts(h);
	puts(sep);
	puts("");
}

#define JLIB_TEST_AUTO_ARRAY

#if defined(JLIB_TEST_AUTO_ARRAY)
#include <stdio.h>

int main(int argc, char *argv[]) {
	jAuto_Array<int> arr = {};

	print_header("jAuto_Array");

	arr.clear();
	arr.append(1);
	assert(arr[0] == 1);
	arr.remove_unordered(0);
	assert(arr.len == 0);

	arr.clear();
	arr.append(1);
	arr.append(2);
	arr.remove_unordered(1);
	assert(arr[0] == 1 && arr.len == 1);

	arr.clear();
	arr.append(1);
	arr.append(2);
	arr.append(3);
	arr.remove_ordered(0);
	assert(arr[0] == 2 && arr.len == 2);

	puts("All went well!");

	return 0;
}

#elif defined(JLIB_TEST_STRINGS)
#include <stdio.h>

static inline void test_string_trim(
	const char *string_to_trim, char32_t *trim_chars, ssize trim_char_count, jString expected
) {
	jSlice<char32_t> chars_to_trim = jSlice<char32_t>(trim_chars, trim_char_count);
	jString trimmed_string         = jString(string_to_trim).trim_chars(chars_to_trim);

	printf("raw string: \"%s\"\n", string_to_trim);
	printf("trimmed string: \"%s\"\n", trimmed_string.clone_to_cstring(jAllocator::heap_allocator()));
	assert(trimmed_string == expected);
}

static inline void test_string_find_last_char(
	const char *string, char32_t find_char, ssize expected
) {
	ssize x = jString(string).find_last_char(find_char);
	printf("find last char in: %s\n", string);
	printf("expected: %lld, got: %lld\n", expected, x);
	assert(x == expected);
}

static inline void test_string_find_first_char(
	const char *string, char32_t find_char, ssize expected
) {
	ssize x = jString(string).find_first_char(find_char);
	printf("find first char in: %s\n", string);
	printf("expected: %lld, got: %lld\n", expected, x);
	assert(x == expected);
}

static inline void test_path_find_dir(
	const char *full, const char *expected
) {
	jString dir = j_path_dir(full);
	const char *dir_string = dir.clone_to_cstring(jAllocator::heap_allocator());
	printf("path: %s, dir: %s\n", full, dir_string);
	printf("expected: %s, got: %s\n", expected, dir_string);
	assert(jString(expected) == dir);
}

static inline void test_path_ext(
	const char *path, const char *expected
) {
	jString ext = j_path_ext(path);
	const char *ext_string = ext.clone_to_cstring(jAllocator::heap_allocator());
	printf("path: %s, file: %s\n", path, ext_string);
	printf("expected: %s, got: %s\n", expected, ext_string);
	assert(jString(expected) == ext);
}

static inline void test_path_file(
	const char *path, const char *expected
) {
	jString file = j_path_file(path);
	const char *file_string = file.clone_to_cstring(jAllocator::heap_allocator());
	printf("path: %s, ext: %s\n", path, file_string);
	printf("expected: %s, got: %s\n", expected, file_string);
	assert(jString(expected) == file);
}

static inline void test_join_strings(
	jSlice<const jString> strings, jString sep, const char *expected
) {
	jString result = jString::join(strings, sep, jAllocator::heap_allocator());

	printf("expected: \"%s\", got: \"%s\"\n", expected, result.clone_to_cstring(jAllocator::heap_allocator()));

	assert(result == jString(expected));
}

static inline void test_string_split(
	const char *str, jSlice<char32_t> sep
) {
	printf("Split string %s:\n", str);

	jSlice<jString> result = jString(str).split(sep, jAllocator::heap_allocator(), INT_MAX);

	for (jString r : result) {
		printf("\"%s\"\n", r.clone_to_cstring(jAllocator::heap_allocator()));
	}
}

int main(int argc, char *argv[]) {

	print_header("jString::trim");
	test_string_trim("&*&Hello, world!*&**&", U"&*", 2, "Hello, world!");
	test_string_trim("&*&Hello, world!", U"&*", 2, "Hello, world!");
	test_string_trim("Hello, world!*&**&", U"&*", 2, "Hello, world!");
	test_string_trim("Hello, world!", U"&*", 2, "Hello, world!");

	print_header("jString::find_*_char");
	test_string_find_last_char("///", '/', 2);
	test_string_find_last_char("///", '4', -1);
	test_string_find_first_char("$//", '/', 1);
	test_string_find_first_char("///", '4', -1);

	print_header("j_path_dir");
	#ifdef _WIN32
	test_path_find_dir("/foo\\bar/1234", "/foo\\bar");
	test_path_find_dir("\\foo\\bar\\", "\\foo\\bar");
	test_path_find_dir("\\foo", "\\");
	#endif
	test_path_find_dir("/foo/bar/1234", "/foo/bar");
	test_path_find_dir("/foo", "/");
	test_path_find_dir("/foo/bar", "/foo");
	test_path_find_dir("a/b/c", "a/b");
	test_path_find_dir("////", "///");
	test_path_find_dir("foobar", "");

	print_header("j_path_ext");

	test_path_ext("foo.bar", ".bar");
	test_path_ext("foo.", ".");
	test_path_ext("foo", "");
	test_path_ext(".bar", ".bar");
	test_path_ext("foo.bar.baz", ".baz");
	test_path_ext("foo.bar/foo.baz", ".baz");

	print_header("j_path_file");

	test_path_file("foo", "foo");
	test_path_file("foo/bar.baz", "bar.baz");

	print_header("jString::join");

	{
		jString strings1[] = {"Foo", "Bar", "Baz"};
		test_join_strings({strings1, ARRAY_LEN(strings1)}, ", ", "Foo, Bar, Baz");

		jString strings2[] = {"Foo"};
		test_join_strings({strings2, ARRAY_LEN(strings2)}, ", ", "Foo");

		jString strings3[] = {"Foo", "Bar", "Baz"};
		test_join_strings({strings3, ARRAY_LEN(strings3)}, "", "FooBarBaz");
	}

	print_header("jString::split");

	{
		test_string_split("Foo, Bar, Baz", {U", ", 2});
		test_string_split("Foo, Bar,", {U", ", 2});
		test_string_split(",,Foo, Bar,", {U", ", 2});
		test_string_split(",,Foo, ,,Bar,   Baz,,,  ", {U", ", 2});
		test_string_split(",,", {U", ", 2});
		test_string_split("A,B,C,", {U", ", 2});
	}

	return 0;
}

#elif defined(JLIB_TEST_FILESYSTEM)

static inline void test_walk_directory(const char *path) {
	printf("Iterating directory: %s\n", path);

	jDirectory_Iterator *iter = j_directory_iterate(path);
	jFile_Info file = {};

	if (iter) {
		while (iter->iterate(&file)) {
			puts(file.full_path.data);
		}
	}
	else {
		puts("Not found.");
	}
}

static inline void test_file_info(const char *path) {
	jFile_Info info = {};

	printf("Dumping file info for: %s\n", path);

	if (j_file_info_get(path, &info, jAllocator::heap_allocator())) {
		printf("Size: %llu\n", info.size);
		printf("Type: %d\n", info.type);
		printf("Full path: %s\n", info.full_path.data);
	}
	else {
		puts("File not found.");
	}
}

int main(int argc, char *argv[]) {

	print_header("j_directory_iterate");

	test_walk_directory(".");
	test_walk_directory("..");
	test_walk_directory("");

	print_header("j_file_info_get");

	test_file_info("jlib.h");

	return 0;
}

#endif