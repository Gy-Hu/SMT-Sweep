#include <chrono>
#include "assert.h"
#include "config/testpath.h"
#include "frontend/btor2_encoder.h"
#include "framework/symsim.h"
#include "framework/ts.h"
#include "smt-switch/bitwuzla_factory.h"
#include "smt-switch/smtlib_reader.h"

using namespace wasim;
using namespace smt;
using namespace std;


int main(int argc, char* argv[]) {

    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <file.btor2/smt2> <bound>\n";
        return 1;
    }
    const std::string input_file = argv[1];
    // const int bound = stoi(argv[2]);

    // Create and configure the SMT solver
    SmtSolver solver = BitwuzlaSolverFactory::create(false);
    solver->set_logic("QF_UFBV");
    solver->set_opt("incremental", "true");
    solver->set_opt("produce-models", "true");
    solver->set_opt("produce-unsat-assumptions", "true");

    smt::SmtLibReader reader(solver);
    reader.parse(input_file);

    //FIXME
    auto assertion = reader.lookup_symbol("assertion");
    if (!assertion) {
        std::cerr << "Error: Could not find assertion in the input file.\n";
        return 1;
    } else {
        std::cout << "Assertion found: " << assertion->to_string() << std::endl;
    }



    return 0;
}
