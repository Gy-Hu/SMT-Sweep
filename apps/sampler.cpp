#include <chrono>
#include "smt-switch/bitwuzla_factory.h"
#include "smt-switch/smtlib_reader.h"

using namespace smt;
using namespace std;

class AssertCapturingReader : public smt::SmtLibReader {
public:
    vector<Term> captured_assertions;
    
    AssertCapturingReader(smt::SmtSolver& solver) : SmtLibReader(solver) {}
    
    void assert_formula(const Term& assertion) override {
        captured_assertions.push_back(assertion);
        SmtLibReader::assert_formula(assertion);
    }
};

void print_ast(const Term& term, int depth = 0) {
    string indent(depth * 2, ' ');
    cout << indent << "Term: " << term->to_string() << endl;
    cout << indent << "Sort: " << term->get_sort()->to_string() << endl;
    cout << indent << "Op: " << term->get_op().to_string() << endl;
    
    // Print children
    if (term->begin() != term->end()) {
        cout << indent << "Children:" << endl;
        for (auto it = term->begin(); it != term->end(); ++it) {
            print_ast(*it, depth + 1);
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <file.smt2>\n";
        return 1;
    }

    SmtSolver solver = BitwuzlaSolverFactory::create(false);
    solver->set_logic("QF_UFBV");
    solver->set_opt("produce-models", "true");

    AssertCapturingReader reader(solver);
    reader.parse(argv[1]);

    cout << "=== Assertions AST ===" << endl;
    for (size_t i = 0; i < reader.captured_assertions.size(); ++i) {
        cout << "Assertion " << i << ":" << endl;
        print_ast(reader.captured_assertions[i]);
        cout << endl;
    }

    return 0;
}
