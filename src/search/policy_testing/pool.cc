#include "pool.h"

#include <cstdlib>
#include <fstream>
#include <vector>

namespace policy_testing {
PoolFile::PoolFile(const std::shared_ptr<AbstractTask> &task, const std::string &path) {
    out_.open(path);
    out_ << "sas_variables\n" << task->get_num_variables() << "\n";
    for (int var = 0; var < task->get_num_variables(); ++var) {
        out_ << task->get_variable_domain_size(var);
        for (int val = 0; val < task->get_variable_domain_size(var); ++val) {
            out_ << ";" << task->get_fact_name(FactPair(var, val));
        }
        out_ << "\n";
    }
    out_ << "pool" << std::endl;
}

void
PoolFile::write(int ref_index, int steps, const State &state) {
    out_ << ref_index << ";" << steps << ";" << state.get_id();
    for (const auto f : state) {
        out_ << ";" << f.get_value();
    }
    out_ << std::endl;
}

void PoolFile::write(const PoolEntry &entry) {
    write(entry.ref_index, entry.steps, entry.state);
}

Pool
load_pool_file(
    const std::shared_ptr<AbstractTask> &task,
    StateRegistry &state_registry,
    const std::string &path) {
    std::ifstream in;
    in.open(path);
    Pool pool = load_pool(task, state_registry, in);
    in.close();
    return pool;
}

Pool
load_pool(
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
    if (s != "pool") {
        exit(-1);
    }
    return parse_pool_entries(task, state_registry, in);
}

Pool
parse_pool_entries(
    const std::shared_ptr<AbstractTask> &,
    StateRegistry &state_registry,
    std::istream &in) {
    std::string s;
    Pool result;
    while (std::getline(in, s)) {
        std::size_t a = s.find(';');
        std::size_t b = s.find(';', a + 1);
        std::size_t c = s.find(';', b + 1);
        int steps = 0;
        std::vector<int> values;
        int ref = std::stoi(s.substr(0, a));
        steps = std::stoi(s.substr(a + 1, b - a - 1));
        while (true) {
            b = c;
            c = s.find(';', b + 1);
            if (c == std::string::npos) {
                c = b;
                break;
            }
            values.push_back(std::stoi(s.substr(b + 1, c - b - 1)));
        }
        values.push_back(std::stoi(s.substr(c + 1)));
        State state = state_registry.insert_state(values);
        result.emplace_back(ref, ref >= 0 ? result[ref].state.get_id() : StateID::no_state, steps, state);
    }
    std::cout << "... loaded " << result.size() << " entries" << std::endl;
    return result;
}
} // namespace policy_testing
