/**
 * Benchmark: Bool_comb (b-param)
 *
 * The benchmark program reproduces the results of CADE'23 for benchmarks in directory /nfa-bench/benchmarks/bool_comb/cox
 *
 * Optimal Inputs: inputs/bench-double-bool-comb-cox.in
 *
 * NOTE: Input automata, that are of type `NFA-bits` are mintermized!
 *  - If you want to skip mintermization, set the variable `MINTERMIZE_AUTOMATA` below to `false`
 */

#include "utils/utils.hh"

constexpr bool MINTERMIZE_AUTOMATA{ true};

//Bug: a problem with concatenation happens with the benchmarks below and with t=false. After the third concatenation the lang becomes empty.
//Performance: with t=true, the bug disappears, but the performance of trim degrades terribly.
//nfa-bench/bench/benchmarks/bool_comb/cox/diff_sat-100-lhs.mata
//nfa-bench/benchmarks/bool_comb/cox/diff_sat-100-rhs.mata

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Input files missing\n";
        return EXIT_FAILURE;
    }

    std::vector<std::string> filenames {argv[1], argv[2]};
    std::vector<Nfa> automata;
    mata::OnTheFlyAlphabet alphabet;
    if (load_automata(filenames, automata, alphabet, MINTERMIZE_AUTOMATA) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    Nfa aut1 = automata[0];
    Nfa aut2 = automata[1];

    bool t = false;

    TIME_BEGIN(concatenate1);
    Nfa aut3 =  aut1.concatenate( aut2) ;
    TIME_END(concatenate1);

    TIME_BEGIN(empty1);
    std::cout<<"is empty: "<<aut3.is_lang_empty()<<std::endl;
    TIME_END(empty1);

    if (t) {
        TIME_BEGIN(trim1);
        aut3.trim();
        TIME_END(trim1);
    }

    TIME_BEGIN(concatenate2);
    Nfa aut4 =  aut3.concatenate( aut3) ;
    TIME_END(concatenate2);

    TIME_BEGIN(empty2);
    std::cout<<"is empty: "<<aut4.is_lang_empty()<<std::endl;
    TIME_END(empty2);

    if (t) {
        TIME_BEGIN(trim2);
        aut4.trim();
        TIME_END(trim2);
    }

    TIME_BEGIN(concatenate3);
    Nfa aut5 = aut4.concatenate(aut4);
    TIME_END(concatenate3);

    TIME_BEGIN(empty3);
    std::cout<<"is empty: "<<aut5.is_lang_empty()<<std::endl;
    TIME_END(empty3);

    if (t) {
        TIME_BEGIN(trim3);
        aut5.trim();
        TIME_END(trim3);
    }

    TIME_BEGIN(concatenate4);
    Nfa aut6 = aut5.concatenate(aut5);
    TIME_END(concatenate4);

    if (t) {
        TIME_BEGIN(trim4);
        aut6.trim();
        TIME_END(trim4);
    }

    TIME_BEGIN(concatenate5);
    Nfa aut7 = aut6.concatenate(aut6);
    TIME_END(concatenate5);

    if (t) {
        TIME_BEGIN(trim5);
        aut7.trim();
        TIME_END(trim5);
    }

    TIME_BEGIN(concatenate6);
    Nfa aut8 = aut7.concatenate(aut7);
    TIME_END(concatenate6);

    if (t) {
        TIME_BEGIN(trim6);
        aut8.trim();
        TIME_END(trim6);
    }

    TIME_BEGIN(concatenate7);
    Nfa aut9 = aut8.concatenate(aut8);
    TIME_END(concatenate7);

    if (t) {
        TIME_BEGIN(trim7);
        aut9.trim();
        TIME_END(trim7);
    }

    TIME_BEGIN(concatenate8);
    Nfa aut10 = aut9.concatenate(aut9);
    TIME_END(concatenate8);

    if (t) {
        TIME_BEGIN(trim8);
        aut10.trim();
        TIME_END(trim8);
    }

    TIME_BEGIN(concatenate9);
    Nfa aut11 = aut10.concatenate(aut10);
    TIME_END(concatenate9);

    if (t) {
        TIME_BEGIN(trim9);
        aut11.trim();
        TIME_END(trim9);
    }

    TIME_BEGIN(concatenate10);
    Nfa aut12 = aut11.concatenate( aut11);
    TIME_END(concatenate10);

    std::cout<<"orig size: "<<aut12.num_of_states()<<std::endl;
    std::cout<<"orig final: "<<aut12.final.size()<<std::endl;

    TIME_BEGIN(empty10);
    std::cout<<"is empty: "<<aut12.is_lang_empty()<<std::endl;
    TIME_END(empty10);

    TIME_BEGIN(trim10);
    aut12.trim();
    TIME_END(trim10);

    std::cout<<"trimed size: "<<aut12.num_of_states()<<std::endl;

    TIME_BEGIN(empty);
    std::cout<<"is empty: "<<aut12.is_lang_empty()<<std::endl;
    TIME_END(empty);

    return EXIT_SUCCESS;
}
