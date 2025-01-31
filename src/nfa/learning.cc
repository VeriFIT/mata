/* nfa-learn.cc -- NFA learning algorithms
*/

//MATA headers
#include "mata/nfa/learning.hh"

Nfa learning(Nfa teacher, const ParameterMap& params){
    if(teacher.num_of_states() == 0){
        throw std::runtime_error("Invalid teacher automaton!\n");
    }
    const Alphabet& alph = create_alphabet(teacher);
    OrdVector<Symbol> alphabet = alph.get_alphabet_symbols();
    Nfa conjecture;
        OT table(alphabet, teacher, params);
        while(1){
        const_check_res res;
        if(table.mata_can_be_constructed(res, alphabet, params)){
            Nfa conjecture = construct_conjecture(table, alphabet, params);
            Word cex;
            bool equivalent = equivalence_query(conjecture, teacher, cex);
            if(equivalent){
                return conjecture;
            }
            else{
                table.update_after_cex(cex, teacher, params);
            }
        }
        //cant be constructed
        else{
            if(!res.consistent){
                table.update_consistency(teacher, res.c, params);
            }
            if(!res.closed){
                table.state_not_closed(res.not_closed, alphabet, teacher);
            }
        }
    }
}

Nfa learn(Nfa teacher, const ParameterMap& params){
    if (!haskey(params, "algorithm")) {
        throw std::runtime_error(std::to_string(__func__) +
            " requires setting the \"algorithm\" key in the \"params\" argument; "
            "received: " + std::to_string(params));
    }
    const std::string& algo = params.at("algorithm");
    if(algo == "nlstar" || algo == "lstar"){
        return learning(teacher, params);
    }
    else{
        throw std::runtime_error(std::to_string(__func__) +
            " received an unknown value of the \"algorithm\" key: " + algo);
    }
}

// very inefficient query for long words
bool membership_query(Nfa teacher, Word word){
    return teacher.is_in_lang(word);
}

bool equivalence_query(Nfa teacher, Nfa conjecture, Word& cex){
    Run counter_example_run;
    is_included_naive(conjecture, teacher, teacher.alphabet, &counter_example_run);
    if(counter_example_run.word.size() == 0) 
        is_included_naive(teacher, conjecture, teacher.alphabet, &counter_example_run);
    ParameterMap params;
    params["algorithm"] = "naive";
    bool equivalent = are_equivalent(teacher, conjecture, teacher.alphabet, params);
    cex = counter_example_run.word;
    return equivalent;
}//equivalence_query

Nfa construct_conjecture(OT table, OrdVector<Symbol> alphabet, const ParameterMap& params){
    Nfa conjecture;
    vector<State_construction>states;
    long unsigned int num_of_states = 0;
    // first get all states possible
    if(params.at("algorithm") == "nlstar"){
        for(auto row : table.S){
            if(table.is_prime(row)){
                if(table.covers(row->T, table.S.front()->T)) conjecture.initial.insert(num_of_states);
                if(row->T[0]) conjecture.final.insert(num_of_states);
                conjecture.add_state(num_of_states);
                states.emplace_back(State_construction({(*row), num_of_states}));
                num_of_states++;
            }
        }
        // go through all acquired states and compute the delta function
        for(State_construction state : states){
            for(Symbol symbol : alphabet){
                Word new_input = state.row.value;
                new_input.emplace_back(symbol);
                //we have to check if the new input is located in the table
                //if its in, it means that it represents some state and we have a delta
                auto res = table.all_map.find(new_input);
                if(res != table.all_map.end()){
                    for(State_construction state2 : states){
                        if(table.covers(state2.row.T, res->second->T)){
                            Transition new_transition;
                            new_transition.symbol = symbol;
                            new_transition.source = state.index;
                            new_transition.target = state2.index;
                            conjecture.delta.add(new_transition);
                        }
                    }
                }
            }
        }
    }
    if(params.at("algorithm") == "lstar"){
        for(auto row : table.S){        
            if(row->value.empty()) conjecture.initial.insert(num_of_states);
            if(row->T[0]) conjecture.final.insert(num_of_states);
            states.push_back({(*row), num_of_states});
            conjecture.add_state(num_of_states);
            num_of_states++;
        }
        for(State_construction state : states){
            for(Symbol symbol : alphabet){
                Word new_input = state.row.value;
                new_input.push_back(symbol);
                auto res = table.all_map.find(new_input);
                if(res != table.all_map.end()){
                    for(State_construction state2 : states){
                        if(res->second->T == state2.row.T){
                            Transition new_transition;
                            new_transition.symbol = symbol;
                            new_transition.source = state.index;
                            new_transition.target = state2.index;
                            conjecture.delta.add(new_transition);
                        }
                    }
                }
            }
        } 
    }

    //might remove this, ensure there is no trap state - not needed
    conjecture.trim();
    return conjecture;
}

OT::OT(OrdVector<Symbol> alphabet, Nfa teacher, const ParameterMap& params){
    // essentially epsilon, but Mata would try to find
    // eps transitions and explicitly add it to Words
    E.emplace_back(Word({}));
    auto eps_ptr = make_shared<Row>(Row{{membership_query(teacher, {})}, {}, {}});
    all.emplace_back(eps_ptr);
    S.emplace_back(eps_ptr);
    all_map.insert({{}, eps_ptr});

    for(Symbol sym : alphabet){
        auto row_ptr = make_shared<Row>(Row{{membership_query(teacher, {sym})}, {sym}, {}});
        all.emplace_back(row_ptr);
        Splus.emplace_back(row_ptr);
        all_map.insert({{sym}, row_ptr});
    }
    //initialize cover relation
    if(params.at("algorithm") == "nlstar"){
        get_covering(eps_ptr);
    }
}

OT::~OT(){
    S.clear();
    Splus.clear();
    all.clear();
    E.clear();
}

inline BoolVector OT::join_op(BoolVector r1, BoolVector r2){
    for(auto it1 = r1.begin(), it2 = r2.begin(); it1 != r1.end(); it1++, it2++){
        *it1 = *it1 || *it2;
    }
    return r1;
}

inline bool OT::covers(BoolVector r1, BoolVector r2){
    for(auto it1 = r1.begin(), it2 = r2.begin(); it1 != r1.end(); it1++, it2++){
        if(*it1 && !*it2){ // if r1(E) is true, it implies that r2(E) is true
            return false;
        }
    }
    return true;
}

bool OT::get_t(Nfa aut, Word e, Word value){
    Word input = value;
    for(Symbol symbol : e){
        input.emplace_back(symbol);
    }
    return membership_query(aut, input);
}

bool OT::is_prime(shared_ptr<Row> row){
    BoolVector join(E.size(), false);
    bool not_joined = true;
    for(auto it = all.begin(); it != all.end(); it++){
        if((*it)->T != row->T && covers((*it)->T, row->T)){
            join = join_op(join, (*it)->T);
            not_joined = false;
        }
    }
    if(not_joined || join != row->T) return true;

    return false;
}

bool OT::rfsa_closure(unclosed& not_closed) {
    vector<shared_ptr<Row>> primes_upp;
    vector<BoolVector> Ts;
    for (auto row : S) {
        if (is_prime(row)) {
            primes_upp.emplace_back(row);
        }
        Ts.emplace_back(row->T);
    }

    for (auto it = Splus.begin(); it != Splus.end(); it++) {
        BoolVector join(E.size(), false);

        bool not_joined = true;
        for (auto prime : primes_upp) {
            if ((*it)->T != prime->T && covers(prime->T, (*it)->T)) {
                join = join_op(join, prime->T);
                not_joined = false;
            }
        }
        bool closed = true;
        for(auto it1 = join.begin(), it2 = (*it)->T.begin(); it1 != join.end(); it1++, it2++){
            if((*it1) != (*it2)){
                closed = false;
                break;
            }
        }
        bool not_in = find(Ts.begin(), Ts.end(), (*it)->T) == Ts.end();

        if ((not_joined || !closed) && not_in) {
            not_closed.unclosed_word = (*it)->value;
            not_closed.unclosed_row = (*it);
            //move the row to the top part
            S.emplace_back(*it);
            get_covering(*it);
            update_covering_new_row(*it);
            Splus.erase(it);
            return false;
        }
    }

    return true;
}
bool OT::dfa_closure(unclosed& not_closed){
    set<BoolVector> S_set;
    for(auto S_row : S){
        S_set.insert(S_row->T);
    }
    for(auto it = Splus.begin(); it != Splus.end(); it++){
        if(S_set.find((*it)->T) == S_set.end()){
            not_closed.unclosed_word = (*it)->value;
            not_closed.unclosed_row = (*it);
            S.emplace_back(*it);
            Splus.erase(it);
            return false;
        }
    }
    return true;  
}

bool OT::rfsa_consistency(Word& c, OrdVector<Symbol>alphabet){
    for(auto row : S){
        for(auto covering : row->covering){
            for(Symbol sym : alphabet){       
                Word input1 = row->value;
                Word input2 = covering->value;       
                input1.emplace_back(sym);
                input2.emplace_back(sym);
                
                auto res1 = all_map.find(input1);
                auto res2 = all_map.find(input2);
                for(long unsigned int i = 0; i < res1->second->T.size(); i++){
                    if(res1->second->T[i] && !res2->second->T[i]){
                        c = {sym};
                        for(Symbol s : E[i]){
                            c.emplace_back(s);
                        }
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

bool OT::dfa_consistency(Word& c, OrdVector<Symbol> alphabet){
    multimap<BoolVector, Word> map; // one bool vector can be mapped to multiple access strings
    std::map<Word, BoolVector> map2;
    set<BoolVector> T_in_table; // all bool vectors in the table
    for(auto S_row : S){
        map.insert({S_row->T, S_row->value});
        map2.insert({S_row->value, S_row->T});
        T_in_table.insert(S_row->T);
    }
    for(auto Splus_row : Splus){
        map2.insert({Splus_row->value, Splus_row->T});
    }
    for(auto T : T_in_table){
        auto equal_range = map.equal_range(T);
        vector<Word> inputs; // all access string for this exact bool vector
        if(equal_range.first != equal_range.second){
            for(auto it = equal_range.first; it != equal_range.second; it++){
                inputs.push_back(it->second);
            }
            for(auto Symbol : alphabet){
                set<BoolVector>Ts;
                for(long unsigned int i = 0; i < inputs.size(); i++){
                    Word new_input = inputs[i];
                    new_input.push_back(Symbol);
                    Ts.insert(map2.find(new_input)->second);
                }
                for(long unsigned int e = 0; e < E.size(); e++){
                    set<bool>test;
                    for(auto T : Ts){
                        test.insert(T[e]);
                    }
                    if(test.size() > 1){
                        c = {Symbol};
                        for(long unsigned int sym = 0; sym < E[e].size(); sym++){
                            c.push_back(E[e][sym]);
                        }
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

void OT::get_covering(shared_ptr<Row> row){
    for(auto r : S){
        if(r->value == row->value) continue;
        if(covers(row->T, r->T)){
            row->covering.emplace_back(row);
        }
    }
}

void OT::update_covering_new_e() {
    // if we added a new True membership query to a row
    // it means that all rows in cover relation need to have true as well
    // if they dont, remove them from the cover relation vector
    for (auto row : S) {
        if (row->T.back()){
            for (auto it = row->covering.begin(); it != row->covering.end();){
                if (!((*it)->T.back())) {
                    it = row->covering.erase(it);
                }
                else {
                    it++;
                }
            }
        }
    }
}

void OT::update_covering_new_row(shared_ptr<Row> row){
    for(auto r : S){
        if(r->value == row->value) continue;
        if(covers(r->T, row->T)){
            r->covering.emplace_back(row);
        }
    }
}

inline void OT::add_to_Splus(shared_ptr<Row> row){
    Splus.emplace_back(row);
    all.emplace_back(row);
    all_map.insert({row->value, row});
}

void OT::state_not_closed(unclosed not_closed, OrdVector<Symbol> alphabet, Nfa teacher){
    //unclosed row is already moved to the top part
    for(Symbol symbol : alphabet){
        shared_ptr<Row> new_row = make_shared<Row>();
        new_row->value = not_closed.unclosed_word;
        new_row->value.emplace_back(symbol);
        for(Word e : E){
            Word input = new_row->value;
            for(Symbol sym : e){
                input.emplace_back(sym);
            }
            bool value = membership_query(teacher, input);
            new_row->T.emplace_back(value);
        }

        add_to_Splus(new_row);
    }
}


void OT::update_consistency(Nfa teacher, Word c, const ParameterMap& params){
    E.emplace_back(c);
    for(auto row : all){
        row->T.emplace_back(get_t(teacher, c, row->value));
    }
    if(params.at("algorithm") == "nlstar"){
        update_covering_new_e();
    }
}

inline set<Word> get_all_suffixes(Word cex){
    set<Word>suffixes;
    for(auto it = cex.begin(); it != cex.end(); it++){
        auto suffix = Word({it, cex.end()});
        suffixes.insert(suffix);
    }
    return suffixes;
}

void OT::update_after_cex(Word cex, Nfa teacher, const ParameterMap& params){
    set<Word> suffixes = get_all_suffixes(cex);
    for(Word suffix : suffixes){
        if(find(E.begin(), E.end(), suffix) == E.end()){ // dont need to add suffixes that are already in
            update_consistency(teacher, suffix, params);
        }
    }
}

bool OT::mata_can_be_constructed(const_check_res& res, OrdVector<Symbol> alphabet, const ParameterMap& params){
    if(params.at("algorithm") == "nlstar"){
        res.consistent = rfsa_consistency(res.c, alphabet);
        res.closed = rfsa_closure(res.not_closed);
    }
    if(params.at("algorithm") == "lstar"){
        res.consistent = dfa_consistency(res.c, alphabet);
        res.closed = dfa_closure(res.not_closed);
    }
    return res.consistent && res.closed;
}