#include "plan_file_parser.h"

#include <fstream>

namespace policy_testing {
static std::string
stolower(std::string s) {
    std::transform(
        s.begin(),
        s.end(),
        s.begin(),
        [](unsigned char c) {return std::tolower(c);}   // correct
        );
    return s;
}

PlanFileParser::PlanFileParser(TaskProxy &task_proxy) {
    for (OperatorProxy op : task_proxy.get_operators()) {
        reverse_mapping_[stolower(op.get_name())] = op.get_id();
    }
}

bool
PlanFileParser::parse(const std::string &path, std::vector<OperatorID> &plan)
const {
    std::ifstream f(path);
    if (!f.good()) {
        return false;
    }
    bool res = parse(f, plan);
    f.close();
    return res;
}

bool
PlanFileParser::parse(std::istream &in, std::vector<OperatorID> &result) const {
    std::string operator_line;
    while (std::getline(in, operator_line)) {
        std::string op = stolower(operator_line);
        auto pos = op.find(';');
        if (pos != std::string::npos) {
            op = op.substr(0, pos);
        }
        pos = op.find('(');
        if (pos == std::string::npos) {
            continue;
        }
        op = op.substr(op.find('(') + 1, op.find(')') - 1);
        auto it = reverse_mapping_.find(op);
        if (it == reverse_mapping_.end()) {
            std::cerr << "operator " << op << " not found" << std::endl;
            return false;
        } else {
            result.emplace_back(it->second);
        }
    }
    return true;
}
} // namespace policy_testing
