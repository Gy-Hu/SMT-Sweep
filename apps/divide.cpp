
#include <chrono>
#include "assert.h"
#include "config/testpath.h"
#include "frontend/btor2_encoder.h"
#include "framework/symsim.h"
#include "framework/ts.h"
#include "smt-switch/bitwuzla_factory.h"
#include "smt-switch/smtlib_reader.h"

#include "./sweeping.h"

using namespace wasim;
using namespace smt;

int main(int argc, char* argv[]) {    
    using std::chrono::high_resolution_clock;
    using std::chrono::duration_cast;
    using std::chrono::duration;
    using std::chrono::milliseconds;

    SmtSolver solver = BitwuzlaSolverFactory::create(false);
    solver->set_logic("QF_UFBV");
    solver->set_opt("incremental", "true");
    solver->set_opt("produce-models", "true");
    solver->set_opt("produce-unsat-assumptions", "true");

    // std::cout << "---------------------------Verilog btor---------------------------" << std::endl;
  
    TransitionSystem sts(solver);
    BTOR2Encoder btor_parser("../design/suoglu_div.btor2", sts);
    std::cout << "Trans:" << sts.trans()->to_string() << std::endl;

    SymbolicSimulator executor(sts,solver);
    assignment_type initdiv = {};
    auto initial_state = executor.convert(initdiv);
    executor.init(initial_state);
    auto initial_input = executor.convert(
        {{"clk",1},{"dividend","a"},{"divisor","b"},{"rst",0},{"start",1}});
    auto v_a = initial_input.at(sts.lookup("dividend"));
    auto v_b = initial_input.at(sts.lookup("divisor"));


    std::cout <<"Vlg a expr: " << v_a ->to_string() << std::endl;
    std::cout <<"Vlg b expr: " << v_b ->to_string() << std::endl;

    executor.set_input(initial_input,{});
    executor.sim_one_step();

    auto s1 = executor.get_curr_state();
    auto v_ret = s1.get_sv().at(sts.lookup("quotient"));

    std::vector<decltype(s1)> states;
    states.push_back(s1);

    for(int i = 0; i < 65; i++){
        auto iv_map = executor.convert({{"clk",1},{"start",0},{"rst",0}});
        executor.set_input(iv_map,{});
        executor.sim_one_step();

        s1 = executor.get_curr_state();
        states.push_back(s1);
    }

    for (const auto & a: s1.get_assumptions() ) {
         // HZ: this is the initial constraint
        solver->assert_formula(a);
    }
    
    std::cout << "---------------------------C++ smtlib2---------------------------" << std::endl;

    smt::SmtLibReader smtlib_reader(solver);
    smtlib_reader.parse("../design/aaa.smt2");

    auto c_a = smtlib_reader.lookup_symbol("main::1::dividend!0@1#1");
    auto c_b = smtlib_reader.lookup_symbol("main::1::divisor!0@1#1");
    auto c_ret = smtlib_reader.lookup_symbol("main::1::quotient!0@1#7");

    std::cout << "C a:    " << c_a->to_string() << std::endl;
    std::cout << "C b:    " << c_b->to_string() << std::endl;
    std::cout << "C expr: " << c_ret->to_string() << std::endl;

    std::cout << "------------------------equvi check------------------------------" << std::endl;

    // auto v_a_extend = solver->make_term(smt::Op(smt::Zero_Extend,16),v_a);
    auto check_a = solver->make_term(smt::Equal, v_a, c_a);
    solver->assert_formula(check_a);

    auto check_b = solver->make_term(smt::Equal, v_b, c_b);
    solver->assert_formula(check_b);

    auto bvsort8 = solver->make_sort(smt::SortKind::BV, 32);
    auto divisor_not_eq_0 = solver->make_term(smt::Not,
        solver->make_term(smt::Equal, v_b, solver->make_term(0, bvsort8)));
    solver->assert_formula(divisor_not_eq_0);

    for (int i=0; i<states.size(); ++i) {

        std::cout << "Cycle:"<<i+1;
        std::cout.flush();

        const auto & s = states.at(i);
        auto v_ret = s.get_sv().at(sts.lookup("quotient"));
        auto iv_map = executor.convert({{"clk",1},{"start",0},{"rst",0}});
        auto v_valid = s.interpret_expr_on_curr_state_and_input(sts.lookup("valid"), solver, iv_map);
        TermVec assumptions({v_valid});
        auto res_ret = solver->check_sat_assuming(assumptions);
        if (!res_ret.is_sat()) {
            std::cout << " (skipped)" << std::endl;
            continue;
        }


        auto check_ret = solver->make_term(smt::Equal, v_ret, c_ret);

        Config config;

        if (!parse_arguments(argc, argv, config)) {
            return EXIT_FAILURE;
        }

        //parameter
        int num_iterations = config.simulation_iterations;
        bool dump_smt = config.dump_smt;
        int solver_timeout_ms = config.solver_timeout_ms;
        int property_check_timeout_ms = config.property_check_timeout_ms;
        bool debug = config.debug;
        std::string dump_input_file = config.dump_input_file;
        std::string load_input_file = config.load_input_file;
        std::unordered_map<Term, NodeData> node_data_map; // term -> sim_data
        std::unordered_map<uint32_t, TermVec> hash_term_map; // hash -> TermVec
        std::unordered_map<Term, Term> substitution_map; // term -> term, for substitution
        std::unordered_map<Term, std::unordered_map<std::string, std::string>> all_luts; // state -> lookup table
        const auto& input_terms = btor_parser.inputsvec(); // all input here
        const auto& constraints = btor_parser.get_const_terms(); // all constraints here
        
        int count = 0;
        int unsat_count = 0;
        int sat_count = 0;
        int total_nodes = 0;
        int total_nodes_after = 0;
        std::chrono::milliseconds total_sat_time(0);
        std::chrono::milliseconds total_unsat_time(0); 

        smt::TermVec combined_terms = input_terms;
            smt::UnorderedTermSet out;
            smt::get_free_symbols(check_ret,out);
            for (const auto & term : out) {
                if (std::find(input_terms.begin(), input_terms.end(), term) == input_terms.end()) {
                    combined_terms.push_back(term);
                }
            }

        simulation(combined_terms, num_iterations, node_data_map, dump_input_file, load_input_file, constraints);
        post_order(check_ret, node_data_map, hash_term_map, substitution_map, all_luts, count, unsat_count, sat_count, solver, num_iterations,dump_smt, input_terms, property_check_timeout_ms, debug, dump_input_file, load_input_file, total_sat_time,  total_unsat_time);
        check_ret = substitution_map.at(check_ret);

        Term not_equal = solver->make_term(Not, check_ret);
        assumptions.push_back(not_equal);

        // we are checking (valid |-> v_ret == c_ret) is a valid formula for all the cycles that valid could be 1
        auto t1 = high_resolution_clock::now();
        res_ret = solver->check_sat_assuming(assumptions);
        auto t2 = high_resolution_clock::now();
        duration<double, std::milli> ms_double = t2 - t1;
        
        std::cout << " result: " << res_ret.to_string()  << " time:" << ms_double.count() << "ms" << std::endl;
        if (!res_ret.is_unsat()) {
            std::ofstream fout("cex.txt");
            for (size_t j = 0; j<=i; ++j) {
                for(const auto & sv_val_pair : states[j].get_sv()) {
                    fout << sv_val_pair.first->to_string() << " = ";
                    fout << solver->get_value(sv_val_pair.second)->to_string() << " , ";
                }
                fout << "\n";
            }
            std::cout << "CEX written into cex.txt" << std::endl;
            break;
        }
    }

    return 0;
}