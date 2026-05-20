#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <utility>
#include <unordered_map>
#include <stack>
#include <algorithm>
#include <iostream>
#include <unordered_set>
#include <stdlib.h>
#include <chrono>
#include <thread>
#include <numeric>
#include <cstdlib>
#include <ctime>
#include <fstream>

class Matcher;

class State{
    public: 
        std::string name;
        std::vector<std::pair<std::shared_ptr<Matcher>, State*>> transitions;
        std::vector<int> starsGroups;
        std::vector<int> endsGroups;
    
    explicit State(const std::string& name) : name(name){}

    void addTransition(State* toState, std::shared_ptr<Matcher> matcher){
        transitions.emplace_back(matcher, toState);
    }

    void unshiftTransition(State* toState, std::shared_ptr<Matcher> matcher){
        transitions.insert(transitions.begin(), {matcher, toState});
    }

};

class Matcher{
    public:
        virtual ~Matcher() = default;

    virtual bool matches(char c) const{
        return false;
    }

    virtual bool isEpsilon() const{
        return false;
    }

    virtual std::string label() const {
        return "undefined-matcher";
    }
};

class CharacterMatcher : public Matcher{
    private:
        char c;
    
    public:
        explicit CharacterMatcher(char ch) : c(ch){};

    bool matches(char ch) const override{
        return ch == c;
    }

    bool isEpsilon() const override{
        return false;
    }

    std::string label() const override{
        return std::string(1,c);
    }
};

class EpsilonMatcher : public Matcher {
    public:
    bool matches(char) const override{
        return true;
    }

    bool isEpsilon() const override{
        return true;
    }

    std::string label() const override{
        return "ε";
    }
};

class EngineNFA{
    private:
        std::unordered_map<std::string, std::unique_ptr<State>> states;
        State* initialState = nullptr;
        std::vector<State*> endingStates;

    public:
        EngineNFA() = default;

    void setInitialState(const std::string& name){
        initialState = states.at(name).get();
    }

    void setEndingStates(const std::vector<std::string>& names){
        endingStates.clear();
        for (const auto& name : names){
            endingStates.push_back(states.at(name).get());
        }
    }

    void addState(const std::string& name) {
        states[name] = std::make_unique<State>(name);
    }

    void declareStates(std::initializer_list<std::string> names){
        for (const auto& n : names){
            addState(n);
        }
    }

    void addTransition(const std::string& fromstate, const std::string& toState, std::shared_ptr<Matcher> matcher){
        states.at(fromstate) -> addTransition(states.at(toState).get(), matcher);
    }

    void unshiftTransition(const std::string& fromstate, const std::string& toState, std::shared_ptr<Matcher> matcher){
        states.at(fromstate) -> unshiftTransition(states.at(toState).get(), matcher);
    }

    void appendNFA(EngineNFA& otherNFA, const std::string& unionState){
        for (auto& [name, statePtr] : otherNFA.states){
            states[name] = std::move(statePtr);
        }
        std::string otehrInitial = otherNFA.initialState->name;
        states.erase(otehrInitial);

        State* oldInitial = otherNFA.initialState;

        for (auto& [matcher, toState] : oldInitial->transitions) {
            addTransition(unionState, toState->name, matcher);
        }

        auto it = std::find(endingStates.begin(), endingStates.end(), states[unionState].get());

        if (it != endingStates.end()){
            endingStates.erase(it);
            for (auto* end: otherNFA.endingStates){
                endingStates.push_back(end);
            }
        }
    }

    std::vector<std::string> getEndingStateNames() const {
        std:: vector<std::string> names;
        for (auto* s : endingStates) {
            names.push_back(s->name);
        }
        return names;
    }
    
     const std::unordered_map<std::string, std::unique_ptr<State>>& getStates() const {
        return states;
    }

    const std::vector<State*>& getEndingStates() const {
        return endingStates;
    }

    bool isEndingState(State* s) const {
        return std::find(endingStates.begin(), endingStates.end(), s) != endingStates.end();
    }

    State* getInitialState() const {
        return initialState;
    }

    void absorb(EngineNFA& other) {
        for(auto& [name, statePtr] : other.states) {
            states[name] = std::move(statePtr);
        }
    }

    bool compute(const std::string& input){
        struct Frame {
            size_t i;
            State* currentState;
            std::unordered_set<std::string> epsilonVisited;
        };

        std::stack<Frame> stack;

        stack.push({0, initialState, {}});

        while (!stack.empty()){
            Frame frame = stack.top();
            stack.pop();

            State* currentState = frame.currentState;
            size_t i = frame.i;
            auto epsilonVisited = frame.epsilonVisited;

            if (i == input.size() &&
                std::find(endingStates.begin(), endingStates.end(), currentState) != endingStates.end()){
                return true;
            }

            char currentChar = (i < input.size()) ? input[i] : '\0';

            for (int t = static_cast<int>(currentState -> transitions.size()) - 1; t >= 0; --t) {
                auto& [matcher, toState] = currentState -> transitions[t];
                if (matcher -> isEpsilon()) {
                    if (epsilonVisited.count(toState->name)) continue;
                    auto copymemory = epsilonVisited;
                    copymemory.insert(toState->name);
                    stack.push({i, toState, copymemory});
                } else {
                    if (i < input.size() && matcher->matches(input[i])) {
                        stack.push({i + 1, toState, {}});
                    }
                }

            }

        }

        return false;
    }

    void debugPrint() const {
        std::cout << "===== NFA =====" << std::endl;
        
        if (initialState) {
            std::cout << "Initial: " << initialState->name << std::endl;
        }

        std::cout << "Accepting: ";
        for (auto* s: endingStates){
            std::cout << s->name << " ";
        }
        std::cout << std::endl << std::endl;

        for (const auto& [name, statePtr] : states){
            State* state = statePtr.get();

            for (const auto& [matcher, toState] : state->transitions){
                std::cout << state->name << "--" << matcher->label() << "-->" << toState->name;
                if (isEndingState(toState)){
                    std::cout << " (accept)";

                }
                std::cout << std::endl;
            }
        }
        std::cout << "===============" << std::endl;
    }

    
        
};

typedef enum {
    AtomicExpression,
    Quantifier,
    Alternative,
    Concatenation,
    CharacterClass
} RegexType;

typedef struct Regex {
    RegexType type;
    char value;

    char* charSet;
    int charCount;

    struct Regex* left;
    struct Regex* right;
} Regex;

typedef struct {
    Regex* root;
} Tree;

Regex* createRegex(RegexType type, char value){
    Regex* node = (Regex*) malloc(sizeof(*node));

    node -> type = type;
    node -> value = value;

    node->charSet = NULL;
    node->charCount = 0;

    node -> left = NULL;
    node -> right = NULL;

    return node;
}

Regex* createCharClass(char* chars, int count){
    Regex* node = createRegex(CharacterClass, 0);

    node->charSet = chars;
    node->charCount = count;

    return node;
}

typedef struct {
    const char* input;
    int pos;
} Parser;

char peek(Parser* p){
    return p->input[p->pos];
}

char consume(Parser* p){
    return p->input[p->pos++];
}

char parseEscape(Parser* p){
    consume(p);
    char c = consume(p);

    switch (c){
        case 'n': return '\n';
        case 't': return '\t';
        case 'r': return '\r';

        case '\\': return '\\';

        default:
            return c;
        
    }
}

Regex* parseCharClass(Parser* p){
    consume(p);

    char* buffer = (char*) malloc(256);
    int count = 0;

    while (peek(p) != ']' && peek(p) != '\0'){
        char c;

        if (peek(p) == '\\'){
            c = parseEscape(p);
        } else {
            c = consume(p);
        }

        if (peek(p) == '-' && p->input[p->pos + 1] != ']'){
            consume(p);

            char end;

            if (peek(p) == '\\'){
                end = parseEscape(p);
            } else {
                end = consume(p);
            }
               
            for (char x = c; x <= end; x++){
                buffer[count++] = x;
            }
        } else {
            buffer[count++] = c;
        }
    }

    consume(p);

    return createCharClass(buffer, count);
}

Regex* parseExpression(Parser* p);

Regex* parseAtom(Parser* p){
    char c = peek(p);

    if (c == '('){
        consume(p);
        Regex* node = parseExpression(p);

        if (peek(p) == ')'){
            consume(p);
        }

        return node;
    }

    if (c == '['){
        return parseCharClass(p);
    }

    if (c == '\\'){
        char escaped = parseEscape(p);
        return createRegex(AtomicExpression, escaped);
    }

    consume(p);

    return createRegex(AtomicExpression, c);
}

Regex* parseRepetition(Parser* p){
    Regex* node = parseAtom(p);

    while(1){
        char c = peek(p);

        if (c == '*' || c == '+' || c == '?'){
            consume(p);

            Regex* q = createRegex(Quantifier, c);

            q->left = node;
            node = q;

        } else {
            break;
        }
    }

    return node;
}

Regex* parseConcatenation(Parser* p){
    Regex* left = parseRepetition(p);

    while (1){
        char c = peek(p);

        if (c == '\0' || c == ')' || c == '|'){
            break;
        }

        Regex* right = parseRepetition(p);

        Regex* node = createRegex(Concatenation, '.');

        node->left = left;
        node->right = right;

        left = node;
    }

    return left;
}

Regex* parseExpression(Parser* p){
    Regex* left = parseConcatenation(p);

    while (peek(p) == '|'){
        consume(p);

        Regex* right = parseConcatenation(p);

        Regex* node = createRegex(Alternative, '|');

        node->left = left;
        node->right = right;

        left = node;
    }
    
    return left;
}

Tree regexToAst(const char* pattern){
    Parser p;

    p.input = pattern;
    p.pos = 0;

    Tree tree;

    tree.root = parseExpression(&p);

    return tree;
}

static int stateCounter = 0;

std::string newState() {
    return "s" + std::to_string(stateCounter++);
}

class CharClassMatcher : public Matcher {
    private:
        std::unordered_set<char> allowed;
    public: 
        CharClassMatcher(char* chars, int count){
            for (int i = 0; i < count; i++){
                allowed.insert(chars[i]);
            }
        }

        bool matches(char c) const override {
            return allowed.count(c) > 0;
        }

        std::string label() const override{
            return "[class]";
        }
};

class NFABuilder {
    private:
        int stateNumber = 0;
    
    public:
        std::string newState(){
            std::string name = "q" + std::to_string(stateNumber);
            stateNumber++;
            return name;
        }

        void resetStateNumbers(){
            stateNumber = 0;
        }

        EngineNFA regexToNfa(Tree& ast){
            resetStateNumbers();
            return _regexToNFA(ast.root);
        }

        EngineNFA charClassNfa(Regex* node){
            auto matcher = std::make_shared<CharClassMatcher>(node->charSet, node->charCount);
            return oneStepNFA(matcher);
        }

    private: 
        EngineNFA _regexToNFA(Regex* node){
            if (node->type == Alternative){
                return alternativeToNFA(node);
            }

            return singleRegexToNFA(node);
        }
        //_______________________________________________________
        EngineNFA oneStepNFA(std::shared_ptr<Matcher> matcher) {
            EngineNFA nfa;

            std::string a = newState();
            std::string b = newState();

            nfa.declareStates({a, b});
            nfa.setInitialState(a);
            nfa.setEndingStates({b});

            nfa.addTransition(a, b, matcher);

            return nfa;

        }

        EngineNFA atomicPatternNFA(char character) {
            std::shared_ptr<Matcher> matcher;

            if (character == '\0'){
                matcher = std::make_shared<EpsilonMatcher>();
            } else {
                matcher = std::make_shared<CharacterMatcher>(character);
            }

            return oneStepNFA(matcher);
        }

        EngineNFA concatenationNFA(EngineNFA left, EngineNFA right){
            auto epsilon = std::make_shared<EpsilonMatcher>();
            left.absorb(right);
            for (auto* end : left.getEndingStates()) {
                left.addTransition(end->name, right.getInitialState()->name, epsilon);
            }

            

            left.setEndingStates(right.getEndingStateNames());

            return left;
        }

        EngineNFA alternativeToNFA(Regex* alternativeAST) {
            EngineNFA left = singleRegexToNFA(alternativeAST->left);
            EngineNFA right = singleRegexToNFA(alternativeAST->right);
            EngineNFA nfa;

            std::string start = newState();
            std::string end = newState();

            nfa.declareStates({start, end});
            nfa.setInitialState(start);
            nfa.setEndingStates({end});

            auto epsilon = std::make_shared<EpsilonMatcher>();

            nfa.absorb(left);
            nfa.absorb(right);

            nfa.addTransition(start, left.getInitialState()->name, epsilon);
            nfa.addTransition(start, right.getInitialState()->name, epsilon);

            for (auto* s : left.getEndingStates()){
                nfa.addTransition(s->name, end, epsilon);
            }
            
            for (auto* s : right.getEndingStates()){
                nfa.addTransition(s->name, end, epsilon);
            }

            return nfa;
        }

        EngineNFA asterisk(std::function<EngineNFA()> builder, bool lazy){
            return asteriskPlus(builder, lazy, true);
        }

        EngineNFA plus(std::function<EngineNFA()> builder, bool lazy) {
            return asteriskPlus(builder, lazy, false);
        }

        EngineNFA asteriskPlus(std::function<EngineNFA()> builder, bool lazy, bool asterisk){
            std::string newInit = newState();

            EngineNFA base = builder(); 

            std::string newEnd = newState();

            base.addState(newInit);
            base.addState(newEnd);

            auto epsilon = std::make_shared<EpsilonMatcher>();

            std::string rInit = base.getInitialState()->name;
            std::string rEnd = base.getEndingStates()[0]->name;

            if (lazy) {
                if (asterisk){
                    base.addTransition(newInit, newEnd, epsilon);
                }
                base.addTransition(newInit, rInit, epsilon);
                base.addTransition(rEnd, newEnd, epsilon);
                base.addTransition(rEnd, rInit, epsilon);

            } else {
                base.addTransition(newInit, rInit, epsilon);
                base.addTransition(rEnd, rInit, epsilon);
                base.addTransition(rEnd, newEnd, epsilon);
                if (asterisk){
                    base.addTransition(newInit, newEnd, epsilon);
                }

            }
            base.setInitialState(newInit);
            base.setEndingStates({newEnd});

            return base;
        }

        EngineNFA singleRegexToNFA(Regex* node){
            if (!node) {
                return oneStepNFA(std::make_shared<EpsilonMatcher>());
            }

            switch (node->type){
                case AtomicExpression: {
                    return atomicPatternNFA(node->value);
                }

                case Alternative: {
                    return alternativeToNFA(node);
                }

                case CharacterClass: {
                    return charClassNfa(node);
                }

                case Quantifier: {
                    char q = node->value;
                    if (q == '*'){
                        return asterisk([&]() {return singleRegexToNFA(node->left);}, false);
                    }
                    
                    if (q == '+'){
                        return plus([&]() {return singleRegexToNFA(node->left);}, false);
                    }

                    if (q == '?'){
                        EngineNFA base = singleRegexToNFA(node->left);
                        auto epsilon = std::make_shared<EpsilonMatcher>();
                        std::string start = base.getInitialState()->name;
                        std::string end = base.getEndingStates()[0]->name;
                        base.addTransition(start, end, epsilon);
                        return base;
                    }
                    
                    break;
                }

                default: {
                    EngineNFA left = singleRegexToNFA(node->left);
                    EngineNFA right = singleRegexToNFA(node->right);
                    return concatenationNFA(std::move(left), std::move(right));
                }
            }

            throw std::runtime_error("Invalid regex node");
        }

        EngineNFA regexToNFA(Tree tree){
            return singleRegexToNFA(tree.root);
        }
        
};

class NFARegex {
    private:
        std::string source;
        EngineNFA nfa;
    
    public:
        NFARegex(const char* regex) : source(regex){
            Tree ast = regexToAst(regex);
            NFABuilder builder;
            nfa = builder.regexToNfa(ast);
        }

        bool compute(const char* input){
            return nfa.compute(input);
        }

        EngineNFA& getNfa() {
            return nfa;
        }
};

std::string regexTypeName(RegexType type){
    switch (type){
        case AtomicExpression: return "Atomic";
        case Quantifier: return "Quantifier";
        case Alternative: return "Alternative";
        case Concatenation: return "Concat";
        case CharacterClass: return "CharClass";
    }
    
    return "Unknown";
}

void printAST(Regex* node, int depth = 0){
    if (!node) return;

    for (int i = 0; i < depth; i++){
        std::cout << "  ";
    }

    std::cout << regexTypeName(node->type);

    if (node->type == AtomicExpression){
        std::cout << " '" << node->value << "' ";
    }
    if (node->type == Quantifier){
        std::cout << " '" << node->value << "' ";
    }
    
    std::cout << std::endl;

    printAST(node->left, depth + 1);
    printAST(node->right, depth + 1);
}

void printTree(Tree tree) {
    std::cout << "==== AST ====" << std::endl;
    printAST(tree.root);
    std::cout << "=============" << std::endl;
}

//_______________________________________________________ BENCHMARK 
std::string generate_string(int len){
    char alphanumeric[] = 
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        ".@"
        ;
        
    std::string temp;
    temp.reserve(len);

    for (int i = 0; i < len; i++){
        temp += alphanumeric[rand() % (sizeof(alphanumeric) - 1)];
    }

    return temp;
}

struct Result{
    int length;
    double throughput;
    int matches;
};

void nfa_worker(
    const std::vector<std::string>& data,
    const std::string& pattern,
    size_t begin,
    size_t end,
    long& result
) {
    NFARegex regex(pattern.c_str());

    long local = 0;
    for (size_t i = begin; i < end; ++i) {
        if (regex.compute(data[i].c_str())){
            local++;
        }
    }
    result = local;
}

long parallel_nfa_match(
    const std::vector<std::string>& data,
    const std::string& pattern
) {
    const unsigned threads = 
        std::max(1u, std::thread::hardware_concurrency());

    const size_t chunk_size = data.size() / threads;

    std::vector<std::thread> workers;
    std::vector<long> results(threads);
    size_t begin = 0;

    for (unsigned t = 0; t < threads; ++t){
        size_t end = (t == threads - 1) ? data.size() : begin + chunk_size;
        
        workers.emplace_back(
            nfa_worker,
            std::cref(data),
            std::cref(pattern),
            begin,
            end,
            std::ref(results[t])
        );
        begin = end;
    }

    for (auto& th : workers){
        th.join();
    }

    return std::accumulate(results.begin(), results.end(), 0L);
}


// TODO: implement modern regex logic for AST results

int main() {
    int n = 1 << 20;

    //10,20,30,40,50,60,70,80,90,100,110,120,130,140,150
    std::vector<int> lengths = {5,10,20,30,40,50,60,70};
    std::vector<std::vector<std::string>> datasets(lengths.size());
    for (size_t i = 0; i < lengths.size(); i++){
        datasets[i].reserve(n);
        for (int j = 0; j < n; j++){
            datasets[i].push_back(generate_string(lengths[i]));
        }
    }

    srand(time(NULL));



    const char* pattern = "([A-Za-z]*|[a-z0-9]*)";          
    //const char* pattern = "^[A-Za-z0-9]*Z$";            //Ends with Z
    //const char* pattern = "^A[A-Za-z0-9]*$";          //Starts with A
    //const char* pattern = "^A[A-Za-z0-9]*Z$";         //Starts with A ends with Z
    //const char* pattern = "^[A-Za-z0-9]*[A-Z][0-9]$"; //ends with capitalized and then numeral
    //const char* pattern = "^([A-Za-z][0-9])+$";       //letter numeral pairs
    //const char* pattern = "^([A-Z]|[0-9])*$";         //high selectivity => similar throughput (either capitalized or numeral)

    std::vector<Result> results;
    NFARegex regex(pattern);
    for (size_t i = 0; i < datasets.size(); i++){
        auto start = std::chrono::high_resolution_clock::now();

        long match_count = parallel_nfa_match(datasets[i], pattern);

        auto end = std::chrono::high_resolution_clock::now();
        double time = std::chrono::duration<double>(end-start).count();

        double throughput = n / time;
        results.push_back({lengths[i], throughput, match_count});

        std::cout << "Finsished dataset with length " << lengths[i] << "\n" << std::flush;
    }

    std::ofstream csv("results.csv", std::ios::app);
    csv << "mode,length,throughput_strings_per_sec,matches\n";
    for (const auto& r : results){
        csv << "parallel" << "," << r.length << "," << r.throughput << "," << r.matches << "\n";
    }


    /* _______________________________ DEBUG
    Tree tree = regexToAst(pattern);
    printTree(tree);
    regex.getNfa().debugPrint();*/
    return 0;

}

