#pragma once

#include "scscd.h"

static void benchmark(std::string description, std::function<void()> cb) {
	logger::info(description);
	auto start = std::chrono::steady_clock::now();
	cb();
	auto end = std::chrono::steady_clock::now();
	auto elapsed = duration_cast<std::chrono::milliseconds>(end - start);
	logger::info(std::format("completed {} in {} ms", description, elapsed.count()));
}
