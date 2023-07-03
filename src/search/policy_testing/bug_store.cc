#include "bug_store.h"

#include <cstdlib>
#include <fstream>
#include <limits>
#include <utility>
#include <vector>

namespace policy_testing {
BugStoreFile::BugStoreFile(
    const std::shared_ptr<AbstractTask> &task,
    const std::string &path) {
    out_.open(path);

    out_ << "sas_variables\n" << task->get_num_variables() << "\n";
    for (int var = 0; var < task->get_num_variables(); ++var) {
        out_ << task->get_variable_domain_size(var);
        for (int val = 0; val < task->get_variable_domain_size(var); ++val) {
            out_ << ";" << task->get_fact_name(FactPair(var, val));
        }
        out_ << "\n";
    }

    out_ << "bugs" << std::endl;
}

void
BugStoreFile::write(const State &state, const BugValue &bug_value) {
    out_ << state.get_id() << ";" << bug_value;
    for (const auto f : state) {
        out_ << ";" << f.get_value();
    }
    out_ << std::endl;
}

BugStore
load_bug_file(
    const std::shared_ptr<AbstractTask> &task,
    StateRegistry &state_registry,
    const std::string &path) {
    std::ifstream in;
    in.open(path);
    BugStore pool = load_bugs(task, state_registry, in);
    in.close();
    return pool;
}

BugStore
load_bugs(
    const std::shared_ptr<AbstractTask> &task,
    StateRegistry &state_registry,
    std::istream &in) {
    std::string s;
    std::getline(in, s);
    int num_vars;
    std::getline(in, s);
    num_vars = std::stoi(s);
    while (num_vars > 0) {
        std::getline(in, s);
        --num_vars;
    }
    std::getline(in, s);
    if (s != "bugs") {
        exit(-1);
    }
    return parse_bugs(task, state_registry, in);
}

BugStore
parse_bugs(
    const std::shared_ptr<AbstractTask> &,
    StateRegistry &state_registry,
    std::istream &in) {
    std::string s;
    BugStore result;
    while (std::getline(in, s)) {
        std::size_t a = s.find(';');
        std::size_t b = s.find(';', a + 1);
        std::vector<int> values;
        double bv = std::stod(s.substr(a + 1, b - a - 1));
        while (true) {
            a = b;
            b = s.find(';', a + 1);
            if (b == std::string::npos) {
                b = a;
                break;
            }
            values.push_back(std::stoi(s.substr(a + 1, b - a - 1)));
        }
        values.push_back(std::stoi(s.substr(b + 1)));
        State state = state_registry.insert_state(std::move(values));
        result.emplace_back(state, bv);
    }
    std::cout << "... loaded " << result.size() << " entries" << std::endl;
    return result;
}
} // namespace policy_testing
