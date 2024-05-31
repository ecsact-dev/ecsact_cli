#include "ecsact/cli/commands/build/recipe/integrity.hh"

#include <sstream>
#include <iomanip>
#include <format>
#include <tuple>
#include <openssl/sha.h>

using namespace std::string_view_literals;

static_assert(sizeof(uint8_t) == sizeof(std::byte));
static_assert(
	std::tuple_size_v<decltype(ecsact::cli::detail::integrity_sha256::_data)> ==
	SHA256_DIGEST_LENGTH
);

auto ecsact::cli::detail::integrity_unknown::from_bytes( //
	const std::span<const std::byte>
) -> integrity_unknown {
	return {};
}

auto ecsact::cli::detail::integrity_sha256::from_bytes( //
	const std::span<const std::byte> bytes
) -> integrity_sha256 {
	auto result = integrity_sha256{};
	auto ctx = SHA256_CTX{};
	SHA256_Init(&ctx);
	SHA256_Update(&ctx, bytes.data(), bytes.size());
	SHA256_Final(reinterpret_cast<uint8_t*>(result._data.data()), &ctx);
	return result;
}

auto ecsact::cli::detail::integrity::from_bytes( //
	enum kind                        kind,
	const std::span<const std::byte> bytes
) -> integrity {
	switch(kind) {
		case integrity::sha256:
			return integrity_sha256::from_bytes(bytes);
		case integrity::unknown:
			return integrity_unknown::from_bytes(bytes);
	}
}

auto ecsact::cli::detail::integrity::from_string( //
	std::string_view str
) -> integrity {
	if(str.starts_with(integrity_sha256::string_suffix())) {
		auto result = integrity_sha256{};
		auto integrity_bytes_str =
			std::string{str.substr(integrity_sha256::string_suffix().size())};
		auto ss = std::istringstream{integrity_bytes_str};
		for(int i = 0; i < result._data.size(); i++) {
			auto byte = int{};
			ss >> std::hex >> std::setw(2) >> byte;
			result._data[i] = static_cast<std::byte>(byte);
		}
		return result;
	}

	return integrity_unknown{};
}

auto ecsact::cli::detail::integrity_sha256::to_string() const -> std::string {
	auto ss = std::ostringstream{};
	for(int i = 0; i < _data.size(); i++) {
		ss << std::hex << std::setw(2) << (int)_data[i];
	}
	return std::format("sha256-{}", ss.str());
}
