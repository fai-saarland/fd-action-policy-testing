#ifndef SEARCH_ENGINES_RANDOM_SEARCH_H
#define SEARCH_ENGINES_RANDOM_SEARCH_H

#include "../open_list.h"
#include "../search_engine.h"

#include <memory>
#include <vector>

class Evaluator;
class PruningMethod;

namespace options {
class OptionParser;
class Options;
}

namespace random_search {
class RandomSearch : public SearchEngine {
protected:
    virtual void initialize() override;
    virtual SearchStatus step() override;

public:
    explicit RandomSearch(const options::Options &opts);
    virtual ~RandomSearch() = default;

    virtual void print_statistics() const override;
};

extern void add_options_to_parser(options::OptionParser &parser);
}

#endif
