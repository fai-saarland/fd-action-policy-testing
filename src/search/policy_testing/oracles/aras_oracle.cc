#include "aras_oracle.h"

#include <memory>

#include "../../option_parser.h"
#include "../../plugin.h"

namespace policy_testing {
ArasOracle::ArasOracle(const options::Options &opts)
    : Oracle(opts),
      aras_dir_(opts.get<std::string>("aras_dir")),
      aras_max_time_limit(opts.get<int>("aras_max_time_limit")),
      aras_(nullptr),
      cache_results(opts.get<bool>("cache_results")) {
}

void
ArasOracle::initialize() {
    if (initialized) {
        assert(aras_);
        return;
    }
    aras_ = std::make_unique<ArasWrapper>(aras_dir_, get_task(), get_task_proxy());
    Oracle::initialize();
}

void
ArasOracle::add_options_to_parser(options::OptionParser &parser) {
    Oracle::add_options_to_parser(parser);
    parser.add_option<std::string>("aras_dir", "Base directory of the ARAS plan improver");
    parser.add_option<int>("aras_max_time_limit", "Maximal time to run ARAS.", "14400");
    parser.add_option<bool>("cache_results", "Cache the results of oracle invocations", "true");
}

TestResult
ArasOracle::test(Policy &policy, const State &state) {
    if (cache_results) {
        auto it = result_cache.find(state.get_id());
        if (it != result_cache.end()) {
            return it->second;
        }
    }
    std::vector<OperatorID> plan;
    const auto[complete, solved] = policy.execute_get_plan(state, plan);
    if (!(complete && solved)) {
        if (cache_results) {
            result_cache[state.get_id()] = TestResult(NOT_APPLICABLE_INDICATOR);
        }
        return TestResult(NOT_APPLICABLE_INDICATOR);
    }
    const int base_cost = calculate_plan_cost(get_task(), plan);
    timestamp_t time_limit_long = std::min<timestamp_t>(get_remaining_time(), aras_max_time_limit);
    int time_limit_int;
    if (time_limit_long > std::numeric_limits<int>::max()) {
        time_limit_int = std::numeric_limits<int>::max();
    } else if (time_limit_long >= 0) {
        time_limit_int = static_cast<int>(time_limit_long);
    } else {
        // time_limit_long < 0
        std::cerr << "Cannot start ARAS with negative time limit." << std::endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }

    if (!aras_->improve_plan(time_limit_int, state, plan)) {
        if (cache_results) {
            result_cache[state.get_id()] = {};
        }
        return {};
    }
    const int improved_cost = calculate_plan_cost(get_task(), plan);
    if (improved_cost <= base_cost) {
        if (cache_results) {
            result_cache[state.get_id()] = TestResult(base_cost - improved_cost, improved_cost);
        }
        return TestResult(base_cost - improved_cost, improved_cost);
    }
    if (cache_results) {
        result_cache[state.get_id()] = {};
    }
    return {};
}

static Plugin<Oracle>
_plugin("aras", options::parse<Oracle, ArasOracle>);
} // namespace policy_testing
