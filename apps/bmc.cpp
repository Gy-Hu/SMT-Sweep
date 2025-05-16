#include "./sweeping.h"
#include <gflags/gflags.h>

int main(int argc, char* argv[]) {
    Config config;

    if (!parse_arguments(argc, argv, config)) {
        return EXIT_FAILURE;
    }

    // DEBUG
    if (config.debug) {
        std::cout << "==== DEBUG ====" << std::endl;
        std::cout << "BTOR2 File           : " << config.btor2_file << std::endl;
        std::cout << "Simulation Iterations: " << config.simulation_iterations << std::endl;
        std::cout << "Solver Timeout (ms)  : " << config.solver_timeout_ms << std::endl;
        std::cout << "Property Timeout (ms): " << config.property_check_timeout_ms << std::endl;
        std::cout << "Dump SMT Enabled     : " << (config.dump_smt ? "Yes" : "No") << std::endl;
        std::cout << "Debug Enabled        : " << (config.debug ? "Yes" : "No") << std::endl;
        std::cout << "===============" << std::endl;
    }

    //parameter
    std::string btor2_file = config.btor2_file;
    int num_iterations = config.simulation_iterations;
    bool dump_smt = config.dump_smt;
    int solver_timeout_ms = config.solver_timeout_ms;
    int property_check_timeout_ms = config.property_check_timeout_ms;
    bool debug = config.debug;
    int bound = config.bound;
    std::string dump_input_file = config.dump_input_file;
    std::string load_input_file = config.load_input_file;

    //logging solver
    auto program_start_time = std::chrono::high_resolution_clock::now();
    last_time_point = program_start_time;
    SmtSolver solver = BitwuzlaSolverFactory::create(false);

    solver->set_logic("QF_UFBV");
    solver->set_opt("incremental", "true");
    solver->set_opt("produce-models", "true");
    solver->set_opt("produce-unsat-assumptions", "true");

    // Loading and parsing BTOR2 files
    TransitionSystem sts(solver);
    BTOR2Encoder btor_parser(btor2_file, sts);

    const auto& input_terms = btor_parser.inputsvec(); // all input here
    const auto& output_terms = btor_parser.get_output_terms(); // all output here
    const auto& idvec = btor_parser.idvec();

    SymbolicSimulator sim(sts, solver);
    const auto & propvec = sts.prop();
    if (propvec.empty()) {
        std::cout << "No property to check!" << std::endl;
        return 1;
    }

    auto prop = and_vec(propvec, solver);
    sim.init();
    sim.set_input({},{});

    // if (! check_prop(
    //     sim.interpret_state_expr_on_curr_frame(prop, false),
    //     sim.all_assumptions(),
    //     solver )) {
    //     std::cout << "[bmc] failed at init!" << std::endl;
    //     return 2;
    // }

    
    for (unsigned i = 1; i<=bound; ++i) {
        
        sim.sim_one_step();
        sim.set_input({},{});
        
        if(i == bound) {

            TermVec cons = sim.all_assumptions(); // this is the constraint
            for(auto constraint : cons){
                std::cout << "constraint: " << constraint->to_string() << std::endl;
            }
          
            // init for each bound
            std::unordered_map<Term, NodeData> node_data_map; // term -> sim_data
            std::unordered_map<uint32_t, TermVec> hash_term_map; // hash -> TermVec
            std::unordered_map<Term, Term> substitution_map; // term -> term, for substitution
            std::unordered_map<Term, std::unordered_map<std::string, std::string>> all_luts; // state -> lookup table
            auto root = sim.interpret_state_expr_on_curr_frame(prop, false);

            // smt::TermVec combined_terms = input_terms;
            // smt::UnorderedTermSet out;
            // smt::get_free_symbols(root,out);
            // for (const auto & term : out) {
            //     if (std::find(input_terms.begin(), input_terms.end(), term) == input_terms.end()) {
            //         combined_terms.push_back(term);
            //     }
            // }

            smt::UnorderedTermSet combined_set(input_terms.begin(), input_terms.end());
            smt::UnorderedTermSet out;
            smt::get_free_symbols(root, out);
            combined_set.insert(out.begin(), out.end());
            smt::TermVec combined_terms(combined_set.begin(), combined_set.end());

            

           
            double success_rate = 0.00;


            initialize_arrays({&sts}, all_luts, substitution_map, debug);
            
            auto gen_input_start_time = std::chrono::high_resolution_clock::now();

            std::cout << "========Input generation start==========" << std::endl;
            // simulation(combined_terms, num_iterations, node_data_map, dump_input_file, load_input_file, constraints);
            simulation_using_constraint(combined_terms, num_iterations, node_data_map, dump_input_file, load_input_file, solver, success_rate, cons);


            auto gen_input_end_time = std::chrono::high_resolution_clock::now();
            auto input_time = std::chrono::duration_cast<std::chrono::milliseconds>(gen_input_end_time - gen_input_start_time).count();
            std::cout << "========Input generation time: " << input_time << " ms for " << num_iterations  << " inputs ===========" << std::endl;
            std::cout << "========Success rate: " << success_rate * 100 << "% for " << num_iterations  << " inputs ===========" << std::endl;

            for(auto i : input_terms){
                assert(node_data_map[i].get_simulation_data().size() == num_iterations);
                substitution_map.insert({i, i});
                hash_term_map[node_data_map[i].hash()].push_back(i);
            }
            
            int count = 0;
            int unsat_count = 0;
            int sat_count = 0;
            int total_nodes = 0;
            int total_nodes_after = 0;
            std::chrono::milliseconds total_sat_time(0);
            std::chrono::milliseconds total_unsat_time(0); 
            
            //end of init
            post_order(root, node_data_map, hash_term_map, substitution_map, all_luts, count, unsat_count, sat_count, solver, num_iterations,dump_smt, input_terms, property_check_timeout_ms, debug, dump_input_file, load_input_file, total_sat_time,  total_unsat_time);
            print_time();
            root = substitution_map.at(root);
            count_total_nodes(root, total_nodes_after);
            cout << "total nodes: " << total_nodes_after << endl;
            std::cout<<std::endl;
            if (check_prop(
                root,
                sim.all_assumptions(),
                solver )) {
                print_time();
                std::cout << "[bmc] bound " << i << " passed." << std::endl;
                cout << "total: " << count << " , unsat: " << unsat_count << " , sat: " << sat_count << ", unsat_time: "<< total_unsat_time.count() << " ms, sat_time: " << total_sat_time.count() << " ms" << endl;
            } else {
                print_time();
                std::cout << "[bmc] failed at bound " << i << std::endl;
                cout << "total: " << count << " , unsat: " << unsat_count << " , sat: " << sat_count << ", unsat_time: "<< total_unsat_time.count() << " ms , sat_time: " << total_sat_time.count() << " ms" << endl;
                return 2;
            }

            node_data_map.clear();
            substitution_map.clear();
            hash_term_map.clear();
        }
    }

    auto program_end_time = std::chrono::high_resolution_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(program_end_time - program_start_time).count();
    std::cout << "Total execution time: " << total_time / 1000.0 << " s" << std::endl;
    gflags::ShutDownCommandLineFlags();

    return 0;
}



            //FIXME: constraint get value -> input quality
            // auto const_start_time = std::chrono::high_resolution_clock::now();
            // std::ofstream outfile("simulation_results_sa_annealed.txt", std::ios::out | std::ios::app);
            // if (!outfile.is_open()) {
            //     std::cerr << "[ERROR] Failed to open output file!" << std::endl;
            //     return 0;
            // }

            // for(auto constraint : constraints){
            //     // std::cout << "====constraint: " << constraint->to_string() << endl;
            //     solver->push();
            //     solver->assert_formula(constraint);
            //     for(auto j = 0 ; j < num_iterations; ++j){
            //         auto result = solver->check_sat();
            //         if(result.is_sat()){
            //             outfile << "=== Iteration " << j << " ===" << std::endl;
            //             TermVec eq_terms;
            //             for(auto i : combined_terms){
            //                 auto val = solver->get_value(i);
            //                 Term eq  = solver->make_term(Equal, i, val);
            //                 eq_terms.push_back(eq);
                            
            //                 std::string val_str = val->to_string();
            //                 std::string clean_val = (val_str.size() > 2) ? val_str.substr(2) : val_str;
            //                 auto bv_input = btor_bv_const(clean_val.c_str(), i->get_sort()->get_width());
            //                 node_data_map[i].get_simulation_data().push_back(std::move(bv_input));
            //                 outfile << "Input: " << i->to_string() << " = " << clean_val << std::endl;
            //             }

            //             add_similarity_constraint_sa_annealed(solver, eq_terms, combined_terms.size(), j, num_iterations);
                        
            //         } else {
            //             std::cout << "[ERROR] constraint is unsat!" << std::endl;
            //         }
            //         cout << "iteration: " << j << " done" << endl;
            //     }
            // }
            // auto const_end_time = std::chrono::high_resolution_clock::now();
            // auto const_time = std::chrono::duration_cast<std::chrono::milliseconds>(const_end_time - const_start_time).count();
            // std::cout << "========Constraint check time: " << const_time  << " ms ============" << std::endl;

            // for (const auto &[term, data] : node_data_map) {
            //     if (data.get_simulation_data().size() != num_iterations) {
            //         std::cerr << "[ERROR] Term " << term->to_string()
            //                   << " has " << data.get_simulation_data().size()
            //                   << " values, expected " << num_iterations << std::endl;
            //         assert(false);
            //     }
            // }