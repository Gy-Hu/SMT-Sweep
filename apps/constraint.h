

using namespace smt;
using namespace std;
using namespace wasim;


// std::map<Term, Term> get_guarded_mem_reads(const Term &constraint, const SmtSolver &solver) {
//     Term true_guard = solver->make_term(true);
//     std::map<Term, Term> memread_guard_map;

//     // Stack holds (term, current_guard)
//     std::stack<std::pair<Term, Term>> work_stack;
//     work_stack.emplace(constraint, true_guard);

//     // Memoization to avoid visiting (term, guard) again
//     std::set<std::pair<Term, Term>> visited;

//     while (!work_stack.empty()) {
//         auto [term, guard] = work_stack.top();
//         work_stack.pop();

//         if (term->is_value()) continue;
//         auto key = std::make_pair(term, guard);
//         if (visited.count(key)) continue;
//         visited.insert(key);

//         TermVec children(term->begin(), term->end());
//         auto op = term->get_op();

//         if (op.prim_op == Op(Select) && children.size() == 2) {
//             memread_guard_map[term] = guard;
//             continue;
//         }

//         if (op.prim_op == Ite && children.size() == 3) {
//             Term cond = children[0];
//             Term then_branch = children[1];
//             Term else_branch = children[2];

//             Term new_guard_then = solver->make_term(And, guard, cond);
//             Term new_guard_else = solver->make_term(And, guard, solver->make_term(Not, cond));

//             work_stack.emplace(then_branch, new_guard_then);
//             work_stack.emplace(else_branch, new_guard_else);
//         } else {
//             for (const Term &child : children) {
//                 work_stack.emplace(child, guard);
//             }
//         }
//     }

//     return memread_guard_map;
// }







// void resolve_all_constraints(
//     const smt::TermVec & constraints,

// ) {
//     for(const auto & constraint : constraints) {

//     }
// }



// void extract_regfile_indices(const Term &t, std::unordered_set<int> &indices) {
//     if (t->is_symbol()) {
//         std::string name = t->to_string();
//         std::smatch match;
//         if (std::regex_match(name, match, std::regex(R"(.*\.regfile\.arr\[(\d+)\])"))) {
//             indices.insert(std::stoi(match[1]));
//         }
//     } else {
//         for (const auto &child : t) {
//             extract_regfile_indices(child, indices);
//         }
//     }
// }
