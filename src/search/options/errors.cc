#include "errors.h"

#include <utility>

using namespace std;

namespace options {
OptionParserError::OptionParserError(string msg)
    : msg(std::move(msg)) {
}

void OptionParserError::print() const {
    cerr << "option parser error: " << msg << endl;
}


ParseError::ParseError(
    string msg, const ParseTree &parse_tree, string substring)
    : msg(std::move(msg)),
      parse_tree(parse_tree),
      substring(std::move(substring)) {
}

void ParseError::print() const {
    cerr << "parse error: " << endl
         << msg << " at: " << endl;
    kptree::print_tree_bracketed<ParseNode>(parse_tree, cerr);
    if (!substring.empty()) {
        cerr << " (cannot continue parsing after \"" << substring << "\")";
    }
    cerr << endl;
}


string get_demangling_hint(const string &type_name) {
    return "To retrieve the demangled C++ type for gcc/clang, you can call \n"
           "c++filt -t " + type_name;
}
}
