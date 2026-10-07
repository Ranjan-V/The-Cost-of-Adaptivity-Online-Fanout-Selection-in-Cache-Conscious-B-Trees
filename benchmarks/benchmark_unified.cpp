#include "../include/btree/btree.h"
#include "../include/monitor/bounded_monitor.h"
#include "../include/monitor/monitor.h"
#include "../include/adaptive/segmented_adaptive_btree.h"
#include "../include/adaptive/segmented_adaptive_v2.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#elif defined(__linux__)
#include <sys/resource.h>
#include <unistd.h>
#endif

typedef cabtree::BPlusTree<int, int> Tree;
typedef std::chrono::steady_clock Clock;

static unsigned int logical_cpu_count() {
#ifdef _WIN32
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return static_cast<unsigned int>(info.dwNumberOfProcessors);
#elif defined(__linux__)
    const long count = sysconf(_SC_NPROCESSORS_ONLN);
    return count > 0 ? static_cast<unsigned int>(count) : 0u;
#else
    return 0u;
#endif
}

struct Options {
    std::string variant, output, run_id, family, experiment_family, machine, phase_fanouts, source_id, candidates;
    std::string compiler_flags, cpu_model, environment;
    size_t records, operations, segments, phase_length, sample_rate, warmup, latency_rate, adapt_interval, hot_segment;
    size_t threads, fanout, repetition;
    unsigned long long seed;
    double zipf, reads, updates, hot_fraction;
    Options() : variant("STATIC"), output(""), run_id(""), family("uniform"), experiment_family("UNSPECIFIED"),
        machine("UNSPECIFIED"), phase_fanouts(""), source_id("UNSPECIFIED"),
        candidates("8:16:32:64:128:256"),
        compiler_flags("UNSUPPORTED"), cpu_model("UNSUPPORTED"), environment("UNSPECIFIED"),
        records(100000), operations(100000), segments(8), phase_length(0),
        sample_rate(32), warmup(0), latency_rate(128), adapt_interval(5000), hot_segment(0),
        threads(1), fanout(64), repetition(0),
        seed(1), zipf(0.99), reads(0.95), updates(0.05), hot_fraction(0.20) {}
};

static std::string value(int argc, char** argv, int& i) {
    if (i + 1 >= argc) throw std::invalid_argument("missing CLI value");
    return argv[++i];
}
static Options parse(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        std::string k(argv[i]), v = value(argc, argv, i);
        if (k == "--variant") o.variant = v;
        else if (k == "--output") o.output = v;
        else if (k == "--run-id") o.run_id = v;
        else if (k == "--family") o.family = v;
        else if (k == "--experiment-family") o.experiment_family = v;
        else if (k == "--machine") o.machine = v;
        else if (k == "--source-id") o.source_id = v;
        else if (k == "--compiler-flags") o.compiler_flags = v;
        else if (k == "--cpu-model") o.cpu_model = v;
        else if (k == "--environment") o.environment = v;
        else if (k == "--phase-fanouts") o.phase_fanouts = v;
        else if (k == "--candidate-fanouts") o.candidates = v;
        else if (k == "--records") o.records = std::strtoull(v.c_str(), NULL, 10);
        else if (k == "--operations") o.operations = std::strtoull(v.c_str(), NULL, 10);
        else if (k == "--segments") o.segments = std::strtoull(v.c_str(), NULL, 10);
        else if (k == "--phase-length") o.phase_length = std::strtoull(v.c_str(), NULL, 10);
        else if (k == "--adapt-interval") o.adapt_interval = std::strtoull(v.c_str(), NULL, 10);
        else if (k == "--hot-segment") o.hot_segment = std::strtoull(v.c_str(), NULL, 10);
        else if (k == "--sample-rate") o.sample_rate = std::strtoull(v.c_str(), NULL, 10);
        else if (k == "--warmup") o.warmup = std::strtoull(v.c_str(), NULL, 10);
        else if (k == "--latency-sampling-rate") o.latency_rate = std::strtoull(v.c_str(), NULL, 10);
        else if (k == "--threads") o.threads = std::strtoull(v.c_str(), NULL, 10);
        else if (k == "--repetition") o.repetition = std::strtoull(v.c_str(), NULL, 10);
        else if (k == "--fanout") o.fanout = std::strtoull(v.c_str(), NULL, 10);
        else if (k == "--seed") o.seed = std::strtoull(v.c_str(), NULL, 10);
        else if (k == "--zipf") o.zipf = std::strtod(v.c_str(), NULL);
        else if (k == "--reads") o.reads = std::strtod(v.c_str(), NULL);
        else if (k == "--updates") o.updates = std::strtod(v.c_str(), NULL);
        else if (k == "--hot-fraction") o.hot_fraction = std::strtod(v.c_str(), NULL);
        else if (k == "--inserts" || k == "--deletes")
            throw std::invalid_argument(k + " is unsupported in this driver; do not silently ignore it");
        else throw std::invalid_argument("unknown CLI option: " + k);
    }
    if (!o.records || o.records > static_cast<size_t>(std::numeric_limits<int>::max()) ||
        !o.operations || !o.segments || o.fanout < 3 ||
        o.fanout > static_cast<size_t>(std::numeric_limits<int>::max()) || !o.sample_rate ||
        !o.latency_rate || !o.adapt_interval || o.threads != 1 || o.reads < 0 || o.updates < 0 ||
        !std::isfinite(o.reads) || !std::isfinite(o.updates) || !std::isfinite(o.hot_fraction) ||
        !std::isfinite(o.zipf) || std::fabs(o.reads + o.updates - 1.0) > 1e-9 ||
        o.hot_fraction <= 0 || o.hot_fraction > 1 || o.zipf < 0 ||
        (o.family != "uniform" && o.family != "zipf" && o.family != "shifting") ||
        o.run_id.empty() || o.output.empty())
        throw std::invalid_argument("invalid options; supply run-id/output and one single-thread workload");
    return o;
}

struct Op { int key, value; bool read; Op(int k, int v, bool r) : key(k), value(v), read(r) {} };
static std::vector<size_t> parse_candidate_fanouts(const std::string& source) {
    std::vector<size_t> values;
    std::stringstream stream(source); std::string token;
    while (std::getline(stream, token, ':')) {
        if (token.empty()) throw std::invalid_argument("empty candidate fanout");
        values.push_back(static_cast<size_t>(std::strtoull(token.c_str(), NULL, 10)));
    }
    if (values.empty()) throw std::invalid_argument("empty candidate fanout list");
    return values;
}
struct PhaseRow {
    size_t index, operations;
    double seconds;
    unsigned long long fingerprint;
    PhaseRow(size_t i, size_t n, double s, unsigned long long h)
        : index(i), operations(n), seconds(s), fingerprint(h) {}
};
static unsigned long long hash_step(unsigned long long h, unsigned long long x) {
    for (int i = 0; i < 8; ++i) { h ^= (x & 255ULL); h *= 1099511628211ULL; x >>= 8; }
    return h;
}
static std::vector<Op> workload(const Options& o, unsigned long long& fingerprint) {
    std::mt19937_64 rng(o.seed);
    std::uniform_real_distribution<double> coin(0.0, 1.0);
    const size_t hot_size = std::max<size_t>(1, static_cast<size_t>(o.records * o.hot_fraction));
    const size_t rank_count = o.family == "zipf" ? o.records : hot_size;
    std::vector<double> weights(rank_count);
    for (size_t i = 0; i < rank_count; ++i) weights[i] = 1.0 / std::pow(static_cast<double>(i + 1), o.zipf);
    std::discrete_distribution<size_t> rank(weights.begin(), weights.end());
    std::vector<Op> ops; ops.reserve(o.operations);
    std::uniform_int_distribution<size_t> uniform_key(0, o.records - 1);
    fingerprint = 1469598103934665603ULL;
    for (size_t i = 0; i < o.operations; ++i) {
        size_t phase = o.phase_length ? i / o.phase_length : 0;
        size_t windows = (o.records + hot_size - 1) / hot_size;
        size_t base = o.family == "shifting" ? ((phase + o.hot_segment) % windows) * hot_size : 0;
        size_t key = o.family == "uniform" ? uniform_key(rng) :
            (o.family == "zipf" ? rank(rng) % o.records :
             std::min(o.records - 1, base + rank(rng)));
        bool is_read = coin(rng) < o.reads;
        int val = static_cast<int>((i + 1) & 0x7fffffff);
        ops.push_back(Op(static_cast<int>(key), val, is_read));
        fingerprint = hash_step(fingerprint, static_cast<unsigned long long>(key));
        fingerprint = hash_step(fingerprint, static_cast<unsigned long long>(val));
        fingerprint = hash_step(fingerprint, is_read ? 1ULL : 2ULL);
    }
    return ops;
}

class Index {
public:
    explicit Index(const Options& o) : options_(o), shadow_(), routing_events_(0) {
        if (o.variant == "STATIC+OLD-MONITOR")
            old_.reset(new cache_adaptive::AccessMonitor(10000, 0.1, 1, 0.99));
        if (o.variant.find("BOUNDED-MONITOR") != std::string::npos)
            bounded_.reset(new cabtree::BoundedMonitor<int>(256, 4, 16, o.sample_rate));
        const bool region = (o.variant == "STATIC-REGIONAL" || o.variant == "STATIC+ROUTER" ||
            o.variant == "STATIC+BOUNDED-MONITOR+ROUTER");
        if (region) {
            for (size_t i = 0; i < o.segments; ++i) trees_.push_back(std::unique_ptr<Tree>(new Tree(static_cast<int>(o.fanout))));
        } else if (o.variant == "ADAPT-V1" || o.variant == "PERFECT-V1") {
            v1_.reset(new cache_adaptive::SegmentedAdaptiveBPlusTree<int, int>(
                o.segments, 0, static_cast<int>(o.records - 1), o.fanout, o.adapt_interval, 8, 256, o.sample_rate));
            v1_->set_adaptation_enabled(false);
        } else if (o.variant == "ADAPT-V2" || o.variant == "PERFECT-V2" ||
                   o.variant == "V2-DATAPATH" || o.variant == "V2-MONITOR" ||
                   o.variant == "V2-CONTROLLER") {
            v2_.reset(new cabtree::SegmentedAdaptiveV2<int, int>(
                o.segments, 0, static_cast<int>(o.records - 1), o.fanout, o.adapt_interval,
                o.sample_rate, 8, 256, parse_candidate_fanouts(o.candidates)));
            v2_->set_adaptation_enabled(false);
            v2_->set_monitoring_enabled(o.variant != "V2-DATAPATH");
            v2_->set_rebuild_enabled(o.variant != "V2-CONTROLLER");
        } else if (o.variant == "STATIC" || o.variant == "STATIC+BOUNDED-MONITOR" ||
                   o.variant == "STATIC+OLD-MONITOR" || o.variant == "STATIC+OLD-SHADOW-RECORDS") {
            trees_.push_back(std::unique_ptr<Tree>(new Tree(static_cast<int>(o.fanout))));
        } else throw std::invalid_argument("unknown variant: " + o.variant);
    }
    void load(size_t n) {
        for (size_t i = 0; i < n; ++i) put(static_cast<int>(i), static_cast<int>(i));
        reset_measurement();
    }
    void reset_measurement() {
        if (v1_) v1_->reset_adaptation_state();
        if (v2_) v2_->reset_measurement_state();
        if (old_) old_->clear();
        if (bounded_) bounded_->reset();
        routing_events_ = 0;
    }
    void start_measurement() {
        if (v1_) v1_->set_adaptation_enabled(options_.variant == "ADAPT-V1");
        if (v2_) v2_->set_adaptation_enabled(options_.variant == "ADAPT-V2" ||
                                               options_.variant == "V2-CONTROLLER");
    }
    void put(int k, int v) {
        if (v1_) v1_->insert(k, v);
        else if (v2_) v2_->insert(k, v);
        else trees_[route(k)]->insert(k, v);
        if (options_.variant == "STATIC+OLD-SHADOW-RECORDS") shadow_[k] = v;
    }
    bool get(int k, int& v) {
        if (v1_) return v1_->search(k, v);
        if (v2_) return v2_->search(k, v);
        return trees_[route(k)]->search(k, v);
    }
    void observe(int k) {
        if (old_) old_->record_access(k);
        if (options_.variant == "STATIC+BOUNDED-MONITOR" ||
            options_.variant == "STATIC+BOUNDED-MONITOR+ROUTER") bounded_->record(k);
    }
    void phase_target(size_t phase, const std::vector<size_t>& targets) {
        if (!v1_ && !v2_) return;
        if (phase >= targets.size()) throw std::invalid_argument("missing phase fanout target");
        // The measured phase envelope compares whole STATIC-REGIONAL layouts.
        // Apply its chosen fanout to every region and charge every rebuild.
        for (size_t s = 0; s < options_.segments; ++s) {
            if (v1_) v1_->force_rebuild(s, targets[phase]);
            else v2_->force_rebuild(s, targets[phase]);
        }
    }
    size_t monitor_bytes() const { return v2_ ? v2_->stats().monitor_bytes :
        bounded_ ? bounded_->memory_bytes() : 0; }
    size_t shadow_bytes() const { return v1_ ? v1_->get_stats().total_records * sizeof(std::pair<const int, int>) :
        shadow_.size() * sizeof(std::pair<const int, int>); }
    size_t rebuilds() const { return v2_ ? v2_->stats().rebuilds : v1_ ? v1_->get_stats().total_restructures : 0; }
    double rebuild_ms() const { return v2_ ? v2_->stats().rebuild_ms :
        v1_ ? v1_->get_stats().rebuild_ms : 0.0; }
    size_t monitor_samples() const {
        if (v2_) return v2_->stats().monitor_samples;
        if (v1_) {
            size_t n = 0;
            for (size_t i = 0; i < options_.segments; ++i) n += v1_->get_segment_stats(i).sampled_accesses;
            return n;
        }
        if (old_) return old_->get_sampled_accesses();
        return bounded_ ? bounded_->samples() : 0;
    }
    size_t monitor_events() const {
        if (v2_) return v2_->stats().monitor_events;
        if (v1_) return v1_->get_stats().total_accesses;
        if (old_) return old_->get_total_accesses();
        return bounded_ ? bounded_->events() : 0;
    }
    size_t tree_bytes() const {
        if (v2_) return v2_->approximate_tree_bytes();
        size_t total = 0;
        for (size_t i = 0; i < trees_.size(); ++i) total += trees_[i]->approximate_owned_bytes();
        return total;
    }
    size_t temp_rebuild_bytes() const { return v2_ ? v2_->stats().temp_rebuild_bytes : 0; }
    std::string transitions() const {
        if (!v2_) return "UNSUPPORTED";
        const cabtree::SegmentedAdaptiveV2<int, int>::Stats state = v2_->stats();
        std::ostringstream out;
        for (size_t i = 0; i < state.transitions.size(); ++i) {
            if (i) out << ';';
            out << state.transitions[i].first << '>' << state.transitions[i].second;
        }
        return out.str();
    }
    std::string decision_history() const {
        if (!v2_) return "UNSUPPORTED";
        const cabtree::SegmentedAdaptiveV2<int, int>::Stats state = v2_->stats();
        std::ostringstream out;
        for (size_t i = 0; i < state.decision_history.size(); ++i) {
            if (i) out << ';';
            out << state.decision_history[i];
        }
        return out.str();
    }
    size_t policy_evaluations() const { return v2_ ? v2_->stats().evaluations :
        v1_ ? v1_->get_stats().total_adaptations : 0; }
    size_t maintained() const { return v2_ ? v2_->stats().maintained :
        v1_ ? v1_->get_stats().fanout_maintained : 0; }
    size_t hysteresis_skips() const { return v2_ ? v2_->stats().hysteresis_skips :
        v1_ ? v1_->get_stats().rebuilds_skipped_hysteresis : 0; }
    size_t cooldown_skips() const { return v2_ ? v2_->stats().cooldown_skips :
        v1_ ? v1_->get_stats().rebuilds_skipped_cooldown : 0; }
    size_t cost_skips() const { return v2_ ? v2_->stats().cost_skips :
        v1_ ? v1_->get_stats().rebuilds_skipped_cost : 0; }
    size_t routing_events() const { return v2_ ? v2_->stats().route_events : routing_events_; }
    size_t rebuilds_suppressed() const { return v2_ ? v2_->stats().rebuilds_suppressed : 0; }
    double monitor_ms() const { return v2_ ? v2_->stats().monitor_ms : 0.0; }
    double max_rebuild_ms() const { return v2_ ? v2_->stats().max_rebuild_ms :
        v1_ ? v1_->get_stats().max_rebuild_ms : 0.0; }
private:
    size_t route(int key) const {
        ++routing_events_;
        if (trees_.size() == 1) return 0;
        size_t s = static_cast<size_t>(key) * trees_.size() / options_.records;
        return std::min(s, trees_.size() - 1);
    }
    Options options_;
    std::vector<std::unique_ptr<Tree> > trees_;
    std::unique_ptr<cache_adaptive::SegmentedAdaptiveBPlusTree<int, int> > v1_;
    std::unique_ptr<cabtree::SegmentedAdaptiveV2<int, int> > v2_;
    std::unique_ptr<cache_adaptive::AccessMonitor> old_;
    std::unique_ptr<cabtree::BoundedMonitor<int> > bounded_;
    std::unordered_map<int, int> shadow_;
    mutable size_t routing_events_;
};

static std::vector<size_t> targets(const std::string& value) {
    std::vector<size_t> result;
    if (value.empty()) return result;
    std::stringstream stream(value); std::string token;
    while (std::getline(stream, token, ',')) result.push_back(static_cast<size_t>(std::strtoull(token.c_str(), NULL, 10)));
    return result;
}
static double percentile(std::vector<double>& values, double p) {
    if (values.empty()) return 0;
    std::sort(values.begin(), values.end());
    return values[static_cast<size_t>(p * (values.size() - 1))];
}
static std::string compiler() {
#ifdef __clang__
    return "Clang";
#elif defined(__GNUC__)
    return "GCC";
#else
    return "UNKNOWN";
#endif
}
static std::string compiler_version() {
#ifdef __VERSION__
    return __VERSION__;
#else
    return "UNSUPPORTED";
#endif
}
static std::string os_name() {
#ifdef _WIN32
    return "Windows";
#elif defined(__linux__)
    return "Linux";
#else
    return "UNSUPPORTED";
#endif
}
static std::string peak_rss_bytes() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
        return std::to_string(static_cast<unsigned long long>(counters.PeakWorkingSetSize));
#elif defined(__linux__)
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0)
        return std::to_string(static_cast<unsigned long long>(usage.ru_maxrss) * 1024ULL);
#endif
    return "UNSUPPORTED";
}
static void write_result(const Options& o, unsigned long long fingerprint, double seconds,
                         std::vector<double>& latencies, unsigned long long checksum,
                         size_t misses, const Index& index) {
    std::ifstream exists(o.output.c_str());
    if (exists.good()) throw std::runtime_error("output already exists; use unique per-run path");
    std::ofstream out(o.output.c_str());
    if (!out) throw std::runtime_error("cannot open result file");
    out << "run_id,experiment_family,timestamp,source_id,variant,os,compiler,compiler_version,compiler_flags,cpu_model,logical_cpus,physical_cores,ram_bytes,machine,host_label,environment,seed,repetition,records,operations,zipf,reads,updates,hot_fraction,phase_length,sample_rate,segments,fanout,candidate_fanouts,threads,wall_seconds,throughput_ops_sec,mean_sample_latency_us,p50_us,p95_us,p99_us,sampled_latency_count,monitor_samples,monitor_events,monitor_bytes,monitor_work_ms,policy_evaluations,maintained,hysteresis_skips,cooldown_skips,cost_skips,routing_events,rebuilds_suppressed,rebuild_count,rebuild_total_ms,rebuild_max_ms,fanout_transitions,candidate_decision_history,peak_rss_bytes,tree_bytes,shadow_bytes,temp_rebuild_bytes,bytes_per_key,checksum,misses,workload_fingerprint,invariant_status\n";
    double sum = 0; for (size_t i = 0; i < latencies.size(); ++i) sum += latencies[i];
    out << o.run_id << ',' << o.experiment_family << ',' << std::time(NULL) << ',' << o.source_id << ','
        << o.variant << ',' << os_name() << ',' << compiler() << ',' << compiler_version() << ','
        << o.compiler_flags << ',' << o.cpu_model << ',' << logical_cpu_count()
        << ",UNSUPPORTED,UNSUPPORTED," << o.machine << ','
        << o.machine << ',' << o.environment << ',' << o.seed << ',' << o.repetition << ','
        << o.records << ',' << o.operations << ',' << o.zipf << ','
        << o.reads << ',' << o.updates << ',' << o.hot_fraction << ',' << o.phase_length << ','
        << o.sample_rate << ',' << o.segments << ',' << o.fanout << ',' << o.candidates << ',' << o.threads << ','
        << std::setprecision(15) << seconds << ',' << o.operations / seconds << ','
        << (latencies.empty() ? 0 : sum / latencies.size()) << ','
        << percentile(latencies, 0.50) << ',' << percentile(latencies, 0.95) << ','
        << percentile(latencies, 0.99) << ',' << latencies.size() << ','
        << index.monitor_samples() << ',' << index.monitor_events() << ',';
    if (o.variant == "ADAPT-V1" || o.variant == "PERFECT-V1" ||
        o.variant == "STATIC+OLD-MONITOR") out << "UNSUPPORTED,UNSUPPORTED,";
    else {
        out << index.monitor_bytes() << ',';
        if (o.variant.find("BOUNDED-MONITOR") != std::string::npos) out << "UNSUPPORTED,";
        else out << index.monitor_ms() << ',';
    }
    out << index.policy_evaluations() << ',' << index.maintained() << ','
        << index.hysteresis_skips() << ',' << index.cooldown_skips() << ',' << index.cost_skips() << ','
        << index.routing_events() << ',' << index.rebuilds_suppressed() << ','
        << index.rebuilds() << ',';
    out << index.rebuild_ms() << ',' << index.max_rebuild_ms() << ',';
    out << index.transitions() << ',' << index.decision_history() << ',' << peak_rss_bytes() << ',';
    if (o.variant == "ADAPT-V1" || o.variant == "PERFECT-V1") out << "UNSUPPORTED";
    else out << index.tree_bytes();
    out << ',' << index.shadow_bytes() << ',' << index.temp_rebuild_bytes() << ',';
    if (o.variant == "ADAPT-V1" || o.variant == "PERFECT-V1") out << "UNSUPPORTED";
    else out << static_cast<double>(index.tree_bytes() + index.monitor_bytes() + index.shadow_bytes()) / o.records;
    out << ',' << checksum << ',' << misses << ',' << fingerprint << ",NOT_CHECKED\n";
    if (!out) throw std::runtime_error("failed writing result");
}
static void write_phases(const Options& o, const std::vector<PhaseRow>& phases) {
    const std::string path = o.output + ".phases.csv";
    std::ifstream exists(path.c_str());
    if (exists.good()) throw std::runtime_error("phase output already exists");
    std::ofstream out(path.c_str());
    if (!out) throw std::runtime_error("cannot write phase output");
    out << "run_id,variant,machine,source_id,compiler_version,compiler_flags,experiment_family,seed,repetition,records,operations,zipf,reads,updates,hot_fraction,phase_length,sample_rate,segments,fanout,phase_index,phase_operations,phase_seconds,throughput_ops_sec,phase_fingerprint\n";
    for (size_t i = 0; i < phases.size(); ++i) {
        const PhaseRow& row = phases[i];
        out << o.run_id << ',' << o.variant << ',' << o.machine << ',' << o.source_id << ','
            << compiler_version() << ',' << o.compiler_flags << ',' << o.experiment_family << ','
            << o.seed << ',' << o.repetition << ',' << o.records << ',' << o.operations << ','
            << o.zipf << ',' << o.reads << ',' << o.updates << ',' << o.hot_fraction << ','
            << o.phase_length << ',' << o.sample_rate << ',' << o.segments << ',' << o.fanout << ',' << row.index << ','
            << row.operations << ',' << std::setprecision(15) << row.seconds << ','
            << (row.seconds > 0 ? row.operations / row.seconds : 0) << ',' << row.fingerprint << '\n';
    }
    if (!out) throw std::runtime_error("failed writing phase output");
}
int main(int argc, char** argv) {
    try {
        const Options o = parse(argc, argv);
        const std::vector<size_t> phases = targets(o.phase_fanouts);
        const bool perfect = o.variant == "PERFECT-V1" || o.variant == "PERFECT-V2";
        if (perfect && (!o.phase_length || phases.empty()))
            throw std::invalid_argument("perfect detector requires --phase-length and --phase-fanouts");
        unsigned long long fingerprint;
        const std::vector<Op> ops = workload(o, fingerprint);
        Index index(o); index.load(o.records);
        int result = 0;
        for (size_t i = 0; i < o.warmup && i < ops.size(); ++i) index.get(ops[i].key, result);
        index.reset_measurement();
        index.start_measurement();
        std::vector<double> latencies; latencies.reserve(o.operations / o.latency_rate + 1);
        std::vector<PhaseRow> phase_rows;
        unsigned long long checksum = 1469598103934665603ULL;
        size_t misses = 0;
        Clock::time_point start = Clock::now();
        Clock::time_point phase_start = start;
        unsigned long long phase_hash = 1469598103934665603ULL;
        size_t phase_begin = 0;
        for (size_t i = 0; i < ops.size(); ++i) {
            if (o.phase_length && i && i % o.phase_length == 0) {
                const double elapsed = std::chrono::duration<double>(Clock::now() - phase_start).count();
                phase_rows.push_back(PhaseRow(phase_rows.size(), i - phase_begin, elapsed, phase_hash));
                phase_begin = i;
                phase_hash = 1469598103934665603ULL;
                phase_start = Clock::now();
            }
            if (perfect && i % o.phase_length == 0) index.phase_target(i / o.phase_length, phases);
            Clock::time_point begin;
            bool sampled = i % o.latency_rate == 0;
            if (sampled) begin = Clock::now();
            index.observe(ops[i].key);
            if (ops[i].read) {
                if (!index.get(ops[i].key, result)) { ++misses; result = 0; }
                checksum = hash_step(checksum, static_cast<unsigned long long>(static_cast<unsigned int>(result)));
            } else {
                index.put(ops[i].key, ops[i].value);
                checksum = hash_step(checksum, static_cast<unsigned long long>(static_cast<unsigned int>(ops[i].value)));
            }
            if (sampled) latencies.push_back(std::chrono::duration<double, std::micro>(Clock::now() - begin).count());
            phase_hash = hash_step(phase_hash, static_cast<unsigned long long>(static_cast<unsigned int>(ops[i].key)));
            phase_hash = hash_step(phase_hash, static_cast<unsigned long long>(static_cast<unsigned int>(ops[i].value)));
            phase_hash = hash_step(phase_hash, ops[i].read ? 1ULL : 2ULL);
        }
        double seconds = std::chrono::duration<double>(Clock::now() - start).count();
        if (seconds <= 0) throw std::runtime_error("invalid elapsed duration");
        phase_rows.push_back(PhaseRow(phase_rows.size(), ops.size() - phase_begin,
            std::chrono::duration<double>(Clock::now() - phase_start).count(), phase_hash));
        write_result(o, fingerprint, seconds, latencies, checksum, misses, index);
        write_phases(o, phase_rows);
        std::cout << o.run_id << " throughput=" << o.operations / seconds << " checksum=" << checksum << "\n";
    } catch (const std::exception& e) { std::cerr << "ERROR: " << e.what() << "\n"; return 1; }
    return 0;
}
