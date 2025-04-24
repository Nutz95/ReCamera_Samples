#pragma once
#include <chrono>
#include <iostream>
#include <string>

class Profiler {
public:
    Profiler(const std::string& label) : label_(label), start_(std::chrono::high_resolution_clock::now()) {}

    ~Profiler() {
        auto end      = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_).count();
        std::cout << "[Profiler] " << label_ << ": " << duration / 1000.0 << " ms" << std::endl;
    }

    void stop() {
        auto end      = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_).count();
        std::cout << "[Profiler] " << label_ << ": " << duration / 1000.0 << " ms (manual stop)" << std::endl;
    }

private:
    std::string label_;
    std::chrono::high_resolution_clock::time_point start_;
};

class ProfilerBlock {
public:
    ProfilerBlock(const std::string& label) : label_(label), started_(false) {}

    void start() {
        start_ = std::chrono::high_resolution_clock::now();
        started_ = true;
    }

    void stop() {
        if (!started_) return;
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_).count();
        std::cout << "[ProfilerBlock] " << label_ << ": " << duration / 1000.0 << " ms" << std::endl;
        started_ = false;
    }

private:
    std::string label_;
    std::chrono::high_resolution_clock::time_point start_;
    bool started_;
};
