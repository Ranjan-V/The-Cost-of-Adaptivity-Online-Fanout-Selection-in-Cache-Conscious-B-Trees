#include "../include/btree/btree.h"
#include <tlx/container/btree_map.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

typedef std::chrono::steady_clock Clock;
#ifndef CABTREE_TLX_COMMIT
#define CABTREE_TLX_COMMIT "UNRESOLVED"
#endif

struct Options {
    std::string variant, output, run_id, experiment_family, machine, source_id;
    std::string compiler_flags, cpu_model, environment;
    size_t records, operations, repetition, warmup, latency_rate, threads;
    unsigned long long seed;
    double zipf, reads, updates, hot_fraction;
    Options() : records(0), operations(0), repetition(0), warmup(0), latency_rate(128),
        threads(1), seed(0), zipf(0.99), reads(0.95), updates(0.05), hot_fraction(0.2) {}
};

static std::string next(int argc, char** argv, int& i) {
    if (++i >= argc) throw std::invalid_argument("missing CLI value");
    return argv[i];
}
static Options parse(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string k(argv[i]), v = next(argc, argv, i);
        if (k == "--variant") o.variant = v;
        else if (k == "--output") o.output = v;
        else if (k == "--run-id") o.run_id = v;
        else if (k == "--experiment-family") o.experiment_family = v;
        else if (k == "--machine") o.machine = v;
        else if (k == "--source-id") o.source_id = v;
        else if (k == "--compiler-flags") o.compiler_flags = v;
        else if (k == "--cpu-model") o.cpu_model = v;
        else if (k == "--environment") o.environment = v;
        else if (k == "--records") o.records = std::strtoull(v.c_str(), NULL, 10);
        else if (k == "--operations") o.operations = std::strtoull(v.c_str(), NULL, 10);
        else if (k == "--repetition") o.repetition = std::strtoull(v.c_str(), NULL, 10);
        else if (k == "--warmup") o.warmup = std::strtoull(v.c_str(), NULL, 10);
        else if (k == "--latency-sampling-rate") o.latency_rate = std::strtoull(v.c_str(), NULL, 10);
        else if (k == "--threads") o.threads = std::strtoull(v.c_str(), NULL, 10);
        else if (k == "--seed") o.seed = std::strtoull(v.c_str(), NULL, 10);
        else if (k == "--zipf") o.zipf = std::strtod(v.c_str(), NULL);
        else if (k == "--reads") o.reads = std::strtod(v.c_str(), NULL);
        else if (k == "--updates") o.updates = std::strtod(v.c_str(), NULL);
        else if (k == "--hot-fraction") o.hot_fraction = std::strtod(v.c_str(), NULL);
        else if (k == "--family" && v != "zipf") throw std::invalid_argument("external baseline supports zipf only");
        else if (k == "--fanout" && v != "64") throw std::invalid_argument("external cohort fixes project fanout 64");
        else if (k == "--phase-length" || k == "--sample-rate" || k == "--segments" ||
                 k == "--candidate-fanouts" || k == "--adapt-interval") { /* recorded by campaign metadata */ }
        else throw std::invalid_argument("unknown CLI option: " + k);
    }
    if ((o.variant != "STATIC" && o.variant != "TLX-BTREE") || o.output.empty() ||
        o.run_id.empty() || !o.records || !o.operations || !o.seed || !o.latency_rate ||
        o.threads != 1 || std::fabs(o.reads + o.updates - 1.0) > 1e-9)
        throw std::invalid_argument("invalid external-baseline options");
    return o;
}

struct Op { int key, value; bool read; };
static unsigned long long hash_step(unsigned long long h, unsigned long long x) {
    for (int i = 0; i < 8; ++i) { h ^= x & 255ULL; h *= 1099511628211ULL; x >>= 8; }
    return h;
}
static std::vector<Op> workload(const Options& o, unsigned long long& fingerprint) {
    std::mt19937_64 rng(o.seed);
    std::uniform_real_distribution<double> coin(0.0, 1.0);
    std::vector<double> weights(o.records);
    for (size_t i = 0; i < o.records; ++i) weights[i] = 1.0 / std::pow(static_cast<double>(i + 1), o.zipf);
    std::discrete_distribution<size_t> rank(weights.begin(), weights.end());
    std::vector<Op> ops; ops.reserve(o.operations);
    fingerprint = 1469598103934665603ULL;
    for (size_t i = 0; i < o.operations; ++i) {
        Op op = { static_cast<int>(rank(rng) % o.records), static_cast<int>((i + 1) & 0x7fffffff), coin(rng) < o.reads };
        ops.push_back(op);
        fingerprint = hash_step(fingerprint, static_cast<unsigned long long>(op.key));
        fingerprint = hash_step(fingerprint, static_cast<unsigned long long>(op.value));
        fingerprint = hash_step(fingerprint, op.read ? 1ULL : 2ULL);
    }
    return ops;
}

class ExternalIndex {
public:
    explicit ExternalIndex(bool tlx) : tlx_(tlx), project_(64) {}
    void load(size_t n) { for (size_t i = 0; i < n; ++i) put(static_cast<int>(i), static_cast<int>(i)); }
    void put(int key, int value) {
        if (!tlx_) { project_.insert(key, value); return; }
        tlx::btree_map<int, int>::iterator it = external_.find(key);
        if (it == external_.end()) external_.insert(std::make_pair(key, value));
        else it->second = value;
    }
    bool get(int key, int& value) {
        if (!tlx_) return project_.search(key, value);
        tlx::btree_map<int, int>::iterator it = external_.find(key);
        if (it == external_.end()) return false;
        value = it->second; return true;
    }
private:
    bool tlx_;
    cabtree::BPlusTree<int, int> project_;
    tlx::btree_map<int, int> external_;
};

static double percentile(std::vector<double>& v, double p) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end()); return v[static_cast<size_t>(p * (v.size() - 1))];
}
int main(int argc, char** argv) {
    try {
        const Options o = parse(argc, argv);
        unsigned long long fingerprint = 0;
        const std::vector<Op> ops = workload(o, fingerprint);
        const Clock::time_point build_begin = Clock::now();
        ExternalIndex index(o.variant == "TLX-BTREE"); index.load(o.records);
        const double build_seconds = std::chrono::duration<double>(Clock::now() - build_begin).count();
        int value = 0;
        for (size_t i = 0; i < o.warmup && i < ops.size(); ++i) index.get(ops[i].key, value);
        std::vector<double> latencies; latencies.reserve(o.operations / o.latency_rate + 1);
        unsigned long long checksum = 1469598103934665603ULL; size_t misses = 0;
        const Clock::time_point begin = Clock::now();
        for (size_t i = 0; i < ops.size(); ++i) {
            const bool sampled = i % o.latency_rate == 0; Clock::time_point sample;
            if (sampled) sample = Clock::now();
            if (ops[i].read) {
                if (!index.get(ops[i].key, value)) { ++misses; value = 0; }
                checksum = hash_step(checksum, static_cast<unsigned int>(value));
            } else {
                index.put(ops[i].key, ops[i].value);
                checksum = hash_step(checksum, static_cast<unsigned int>(ops[i].value));
            }
            if (sampled) latencies.push_back(std::chrono::duration<double, std::micro>(Clock::now() - sample).count());
        }
        const double seconds = std::chrono::duration<double>(Clock::now() - begin).count();
        std::ifstream exists(o.output.c_str()); if (exists.good()) throw std::runtime_error("output exists");
        std::ofstream out(o.output.c_str()); if (!out) throw std::runtime_error("cannot write output");
        double sum = 0; for (size_t i = 0; i < latencies.size(); ++i) sum += latencies[i];
        out << "run_id,experiment_family,timestamp,source_id,variant,compiler_flags,cpu_model,machine,environment,seed,repetition,records,operations,zipf,reads,updates,wall_seconds,throughput_ops_sec,build_seconds,mean_sample_latency_us,p50_us,p95_us,p99_us,sampled_latency_count,checksum,misses,workload_fingerprint,invariant_status,external_dependency\n";
        out << o.run_id << ',' << o.experiment_family << ',' << std::time(NULL) << ',' << o.source_id << ','
            << o.variant << ',' << o.compiler_flags << ',' << o.cpu_model << ',' << o.machine << ',' << o.environment << ','
            << o.seed << ',' << o.repetition << ',' << o.records << ',' << o.operations << ',' << o.zipf << ','
            << o.reads << ',' << o.updates << ',' << std::setprecision(15) << seconds << ',' << o.operations / seconds << ','
            << build_seconds << ',' << sum / latencies.size() << ',' << percentile(latencies, .5) << ','
            << percentile(latencies, .95) << ',' << percentile(latencies, .99) << ',' << latencies.size() << ','
            << checksum << ',' << misses << ',' << fingerprint << ",NOT_CHECKED,"
            << (o.variant == "TLX-BTREE" ? "tlx-v0.6.1@" CABTREE_TLX_COMMIT : "project") << '\n';
        std::ofstream phases((o.output + ".phases.csv").c_str());
        phases << "run_id,variant,phase_index,phase_operations,phase_seconds,throughput_ops_sec,phase_fingerprint\n"
               << o.run_id << ',' << o.variant << ",0," << o.operations << ',' << seconds << ','
               << o.operations / seconds << ',' << fingerprint << '\n';
    } catch (const std::exception& e) { std::cerr << "ERROR: " << e.what() << '\n'; return 1; }
    return 0;
}
