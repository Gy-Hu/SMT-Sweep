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

int main() {
    SmtSolver solver = BitwuzlaSolverFactory::create(false);
    solver->set_logic("QF_UFBV");
    solver->set_opt("incremental", "true");
    solver->set_opt("produce-models", "true");
    solver->set_opt("produce-unsat-assumptions", "true");

    TransitionSystem sts(solver);
    BTOR2Encoder btor_parser("../design/eda2_case2.btor2", sts);
    std::cout << "Trans:" << sts.trans()->to_string() << std::endl;

    auto D = sts.lookup("D");
    int width = D->get_sort()->get_width();
    std::cout << "D width: " << width << std::endl;
    std::vector<Term> D_bits;
    for (int i = 0; i < width; i++) {
        Term bit = solver->make_term(Op(Extract, i, i), D);
        std::cout << "D bit " << i << ": " << bit->to_string() << std::endl;
        D_bits.push_back(bit);
    }

    //todo: add random assignments
    UnorderedTermMap rand_assignments;

    for (auto& D_bit : D_bits) {
        Term D_val_bit = solver->substitute(D_bit, rand_assignments);
        // 这里就可以得到每一位的值（按位仿真）
        std::cout << "D bit: " << D_bit->to_string() << " = " << D_val_bit->to_string() << std::endl;
    }

    return 0;
}