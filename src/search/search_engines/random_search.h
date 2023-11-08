#ifndef SEARCH_ENGINES_RANDOM_SEARCH_H
#define SEARCH_ENGINES_RANDOM_SEARCH_H

#include "../open_list.h"
#include "../search_engine.h"
#include "../utils/rng.h"

#include <map>
#include <memory>
#include <vector>

namespace random_search {
class RandomSearch : public SearchEngine {
    State current_state;
    int last_action_cost;
    SearchStatus search_status;
    std::map<StateID, OperatorID> visited_states; 
    virtual void initialize() override;
    virtual SearchStatus step() override;

public:
    explicit RandomSearch(const options::Options &opts);
    virtual ~RandomSearch() = default;

    virtual void print_statistics() const override;


private:
    std::shared_ptr<utils::RandomNumberGenerator> rng;
};


extern void add_options_to_parser(options::OptionParser &parser);
}

#endif
