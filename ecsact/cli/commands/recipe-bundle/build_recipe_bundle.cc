#include "ecsact/cli/commands/recipe-bundle/build_recipe_bundle.hh"

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <filesystem>
#include <fstream>
#include <vector>
#include <span>
#include <ratio>
#include <cstddef>
#ifdef __cpp_lib_stacktrace
#	include <stacktrace>
#endif
#include <curl/curl.h>
#include "magic_enum.hpp"
#undef fopen
#include <boost/url.hpp>
#include <archive.h>
#include <archive_entry.h>
#include "ecsact/cli/report.hh"
#include "xxhash.h"

using namespace std::string_literals;
using namespace std::string_view_literals;
namespace fs = std::filesystem;

template<class... Ts>
struct overloaded : Ts... {
	using Ts::operator()...;
};
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

template<typename T>
class uninitialized {
	T _val;

public:
	uninitialized() {
	}

	operator T() const {
		return _val;
	}
};

using download_file_buffer_t = std::vector<std::byte>;

constexpr auto valid_bundle_entry_prefixes = std::array{
	"files/"sv,
	"codegen/"sv,
};

constexpr auto valid_bundle_paths = std::array{
	"ecsact-build-recipe.yml"sv,
};

static auto archive_error_as_logic_error(archive* a) -> std::logic_error {
	auto msg = std::string{archive_error_string(a)};

#ifdef __cpp_lib_stacktrace
	msg += "\n";
	msg += std::to_string(std::stacktrace::current(1));
#endif

	return std::logic_error{msg};
}

static auto is_valid_bundle_entry_path(std::string_view path) -> bool {
	auto valid_entry_prefix_itr =
		std::ranges::find_if(valid_bundle_entry_prefixes, [&](auto& valid_prefix) {
			return path.starts_with(valid_prefix);
		});

	if(valid_entry_prefix_itr != valid_bundle_entry_prefixes.end()) {
		return true;
	}

	auto valid_bundle_path_itr =
		std::ranges::find_if(valid_bundle_paths, [&](auto& valid_bundle_path) {
			return valid_bundle_path == path;
		});

	return valid_bundle_path_itr != valid_bundle_paths.end();
}

static auto _download_file_write_callback(
	const void* buffer,
	size_t      size,
	size_t      count,
	void*       userdata
) -> size_t {
	auto& out_data = *static_cast<download_file_buffer_t*>(userdata);
	auto  buffer_span = std::span{
    static_cast<const std::byte*>(buffer),
    size * count,
  };

	out_data.insert(out_data.end(), buffer_span.begin(), buffer_span.end());
	return 0;
}

static auto download_file(boost::url url) -> std::vector<std::byte> {
	auto ret_out_data = download_file_buffer_t{};

	curl_global_init(CURL_GLOBAL_ALL);
	auto curl = curl_easy_init();

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ret_out_data);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _download_file_write_callback);

	auto res = curl_easy_perform(curl);

	curl_easy_cleanup(curl);
	curl_global_cleanup();

	if(res != CURLE_OK) {
	}

	return ret_out_data;
}

static auto read_file(fs::path path) -> std::vector<std::byte> {
	auto file_size = fs::file_size(path);
	auto file = std::ifstream{path, std::ios_base::binary};
	assert(file);

	auto file_buffer =
		std::vector<std::byte>{file_size, uninitialized<std::byte>{}};
	file.read(reinterpret_cast<char*>(file_buffer.data()), file_size);

	return file_buffer;
}

static auto write_file(fs::path path, std::span<std::byte> data) -> void {
	if(path.has_parent_path()) {
		auto ec = std::error_code{};
		fs::create_directories(path.parent_path(), ec);
	}
	auto file = std::ofstream(path, std::ios_base::binary | std::ios_base::trunc);
	assert(file);

	file.write(reinterpret_cast<const char*>(data.data()), data.size());
}

ecsact::build_recipe_bundle::build_recipe_bundle() = default;
ecsact::build_recipe_bundle::build_recipe_bundle(build_recipe_bundle&&) =
	default;
ecsact::build_recipe_bundle::~build_recipe_bundle() = default;

auto ecsact::build_recipe_bundle::create( //
	const build_recipe& recipe
) -> create_result {
	auto recipe_bundle_bytes = std::vector<std::byte>{};
	auto new_recipe_sources = std::vector<build_recipe::source>{};
	new_recipe_sources.reserve(recipe.sources().size());
	auto a = std::unique_ptr<archive, decltype(&archive_write_free)>{
		archive_write_new(),
		archive_write_free,
	};

	auto write_callback =
		[](archive* a, void* userdata, const void* buffer, size_t length)
		-> la_ssize_t {
		auto& out_recipe_bundle_bytes =
			*static_cast<decltype(&recipe_bundle_bytes)>(userdata);
		out_recipe_bundle_bytes.insert(
			out_recipe_bundle_bytes.end(),
			static_cast<const std::byte*>(buffer),
			static_cast<const std::byte*>(buffer) + length
		);
		return length;
	};

	archive_write_add_filter_lzma(a.get());
	archive_write_set_format_pax_restricted(a.get());
	archive_write_open2(
		a.get(),
		&recipe_bundle_bytes,
		nullptr,
		static_cast<archive_write_callback*>(write_callback),
		nullptr,
		nullptr
	);

	auto entry = archive_entry_new2(a.get());

	auto add_simple_buffer =
		[&](const std::string& path, std::span<const std::byte> buffer) {
			archive_entry_clear(entry);
			archive_entry_set_pathname(entry, path.c_str());
			archive_entry_set_size(entry, buffer.size());
			archive_entry_set_filetype(entry, AE_IFREG);
			archive_write_header(a.get(), entry);
			archive_write_data(a.get(), buffer.data(), buffer.size());
		};

	auto source_visitor = overloaded{
		[&](build_recipe::source_fetch src) -> build_recipe::source {
			auto src_url = boost::url{src.url};
			auto src_path_basename = fs::path{src_url.path().c_str()}.filename();
			auto archive_rel_path =
				(fs::path{"files"} / src.outdir.value_or(".") / src_path_basename)
					.lexically_normal();
			auto file_buffer = download_file(src_url);
			add_simple_buffer(archive_rel_path.generic_string(), file_buffer);

			return build_recipe::source_path{
				.path = archive_rel_path,
				.outdir = src.outdir,
			};
		},
		[&](build_recipe::source_codegen src) -> build_recipe::source {
			return src;
		},
		[&](build_recipe::source_path src) -> build_recipe::source {
			auto src_path_basename = src.path.filename();
			auto archive_rel_path =
				(fs::path{"files"} / src.outdir.value_or(".") / src_path_basename)
					.lexically_normal();
			auto file_buffer = read_file(src.path);
			add_simple_buffer(archive_rel_path.generic_string(), file_buffer);

			return build_recipe::source_path{
				.path = archive_rel_path,
				.outdir = src.outdir,
			};
		},
	};

	for(auto& source : recipe.sources()) {
		auto new_source = std::visit(source_visitor, source);
		new_recipe_sources.emplace_back(std::move(new_source));
	}

	auto new_recipe = recipe.copy();
	new_recipe.update_sources(std::move(new_recipe_sources));

	add_simple_buffer("ecsact-build-recipe.yml", new_recipe.to_yaml_bytes());

	archive_entry_free(entry);
	if(archive_write_close(a.get()) != ARCHIVE_OK) {
		return archive_error_as_logic_error(a.get());
	}

	auto result = build_recipe_bundle{};
	result._bundle_bytes = recipe_bundle_bytes;
	return result;
}

auto ecsact::build_recipe_bundle::from_file( //
	std::filesystem::path bundle_path
) -> create_result {
	auto result = build_recipe_bundle{};
	result._bundle_bytes = read_file(bundle_path);
	return result;
}

auto ecsact::build_recipe_bundle::bytes() const -> std::span<const std::byte> {
	return std::span{_bundle_bytes.data(), _bundle_bytes.size()};
}

auto ecsact::build_recipe_bundle::extract( //
	std::filesystem::path extract_dir
) const -> extract_result {
	XXH64_hash_t hash = XXH3_64bits(_bundle_bytes.data(), _bundle_bytes.size());
	auto dir = extract_dir / "ecsact-recipe-bundle" / std::format("{}", hash);
	if(!fs::exists(dir)) {
		fs::create_directories(dir);
	}

	auto a = std::unique_ptr<archive, decltype(&archive_read_free)>{
		archive_read_new(),
		archive_read_free,
	};

	auto result = int{};

	result = archive_read_support_filter_all(a.get());
	if(result != ARCHIVE_OK) {
		return archive_error_as_logic_error(a.get());
	}
	result = archive_read_support_format_all(a.get());
	if(result != ARCHIVE_OK) {
		return archive_error_as_logic_error(a.get());
	}
	/* auto test_file = fopen("test.ecsact-recipe-bundle", "r+"); */
	/* result = archive_read_open_FILE(a.get(), test_file); */
	/* if(result != ARCHIVE_OK) { */
	/* 	return archive_error_as_logic_error(a.get()); */
	/* } */
	result = archive_read_open_memory(
		a.get(),
		_bundle_bytes.data(),
		_bundle_bytes.size()
	);
	if(result != ARCHIVE_OK) {
		return archive_error_as_logic_error(a.get());
	}

	auto entry = archive_entry_new2(a.get());
	auto data = std::vector<std::byte>{};

	for(;;) {
		result = archive_read_next_header2(a.get(), entry);
		if(result == ARCHIVE_EOF) {
			break;
		}

		if(result < ARCHIVE_OK) {
			return archive_error_as_logic_error(a.get());
		}

		auto path = //
			fs::path{archive_entry_pathname(entry)} //
				.lexically_normal()
				.generic_string();

		printf("PATH: %s\n", path.c_str());

		if(!is_valid_bundle_entry_path(path)) {
			return std::logic_error{
				std::format("Invalid path '{}' found in recipe bundle", path)
			};
		}

		auto size = archive_entry_size(entry);

		if(size == 0) {
			return std::logic_error{std::format("No size for {}", path)};
		}

		data.resize(size);
		auto read_size = archive_read_data(a.get(), data.data(), data.size());

		if(read_size != size) {
			return std::logic_error{std::format("Failed to read {}", path)};
		}

		write_file(dir / path, data);
	}

	archive_entry_free(entry);

	auto recipe_path = dir / "ecsact-build-recipe.yml";

	if(!fs::exists(recipe_path)) {
		return std::logic_error{
			"Missing required ecsact-build-recipe.yml at root of bundle"
		};
	}

	return recipe_path;
}