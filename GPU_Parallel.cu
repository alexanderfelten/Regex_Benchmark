#include <vector>
#include <string>
#include <iostream>
#include <unordered_map>
#include <cassert>
#include <cstring>
#include <stdexcept>
#include <cuda_runtime.h>

enum RegexType {
    REG_CHAR,
    REG_CONCAT,
    REG_ALT,
    REG_STAR,
    REG_CLASS
};

struct Regex {
    RegexType type;

    char value;
    std::vector<char> char_class;

    Regex* left;
    Regex* right;
};

struct Parser {
    const std::string& s;
    int pos = 0;

    char peek() {
        if (pos >= s.size()) {
            return '\0';
        }
        return s[pos];
    }

    char get() {
        return s[pos++];
    }
};

Regex* parseClass(Parser& p) {
    p.get();

    std::vector<char> chars;

    while (p.peek() != ']' && p.peek() != '\0') {
        char start = p.get();

        if (p.peek() == '-' && p.s[p.pos + 1] != ']') {
            p.get();
            char end = p.get();

            for (char c = start; c <= end; c++) {
                chars.push_back(c);
            }

        } else {
            chars.push_back(start);
        }
    }

    p.get();

    return new Regex{REG_CLASS, 0, chars, nullptr, nullptr};
}

Regex* parseExpr(Parser& p);

Regex* parseAtom(Parser& p) {
    char c = p.peek();

    if (c == '(') {
        p.get();
        Regex* node = parseExpr(p);
        if (p.peek() == ')') {
            p.get();
        }
        return node;
    }

    if (c == '[') {
        return parseClass(p);
    }

    p.get();
    return new Regex{REG_CHAR, c, {}, nullptr, nullptr};
}

Regex* parseRepeat(Parser& p) {
    Regex* node = parseAtom(p);

    while (p.peek() == '*') {
        p.get();
        node = new Regex{REG_STAR, 0, {}, node, nullptr};
    }

    return node;
}

Regex* parseConcat(Parser& p) {
    Regex* left = parseRepeat(p);

    while (true) {
        char c = p.peek();
        if (c == '\0' || c == ')' || c == '|') {
            break;
        }
        Regex* right = parseRepeat(p);
        left = new Regex{REG_CONCAT, 0, {}, left, right};
    }

    return left;
}

Regex* parseExpr(Parser& p) {
    Regex* left = parseConcat(p);

    while (p.peek() == '|') {
        p.get();
        Regex* right = parseConcat(p);
        left = new Regex{REG_ALT, 0, {}, left, right};
    }

    return left;
}

enum TransitionType {
    TRANS_CHAR,
    TRANS_EPSILON,
    TRANS_CLASS
};

struct CPUTransition {
    int to;
    TransitionType type;
    char symbol;
    bool char_class[256];
};

struct CPUState {
    std::vector<CPUTransition> transitions;
    bool is_accepting = false;
};

struct EngineNFA {
    std::vector<CPUState> states;
    int initial_state = 0;
    
    int addState(bool accept = false) {
        states.push_back({});
        int id = states.size() - 1;
        states[id].is_accepting = accept;
        return id;
    }

    void addTransition(int from, int to, TransitionType type, char symbol = 0) {
        states[from].transitions.push_back({to, type, symbol});
    }
};

struct Fragment {
    int start;
    int end;
    EngineNFA nfa;
};

Fragment buildChar(char c) {
    EngineNFA nfa;

    int start = nfa.addState();
    int end = nfa.addState(true);

    nfa.initial_state = start;
    nfa.addTransition(start, end, TRANS_CHAR, c);

    return {start, end, nfa};
}

Fragment concat(Fragment a, Fragment b) {
    int offset = a.nfa.states.size();

    for (auto& s : b.nfa.states) {
        a.nfa.states.push_back(s);
    }

    for (int i = offset; i < a.nfa.states.size(); i++){
        for (auto& t : a.nfa.states[i].transitions) {
            t.to += offset;
        }
    }

    a.nfa.addTransition(a.end, b.start + offset, TRANS_EPSILON);
    a.nfa.states[a.end].is_accepting = false;
    a.nfa.states[b.end + offset].is_accepting = true;

    return {a.start, b.end + offset, a.nfa};
}

Fragment alternate(Fragment a, Fragment b){
    EngineNFA nfa;

    int start = nfa.addState();
    int end = nfa.addState(true);

    int offsetA = nfa.states.size();
    for (auto& s : a.nfa.states) {
        nfa.states.push_back(s);
    }
    int offsetB = nfa.states.size();
    for (auto& s : b.nfa.states) {
        nfa.states.push_back(s);
    }

    for (int i = offsetA; i < nfa.states.size(); i++) {
        for (auto& t : nfa.states[i].transitions) {
            if (i < offsetB) {
                t.to += offsetA;
            } else {
                t.to += offsetB;
            }
        }
    }

    nfa.addTransition(start, a.start + offsetA, TRANS_EPSILON);
    nfa.addTransition(start, b.start + offsetB, TRANS_EPSILON);
    nfa.addTransition(a.end + offsetA, end, TRANS_EPSILON);
    nfa.addTransition(b.end + offsetB, end, TRANS_EPSILON);

    return {start, end, nfa};
}

Fragment star(Fragment a) {
    EngineNFA nfa;

    int start = nfa.addState();
    int end = nfa.addState(true);

    int offset = nfa.states.size();
    for (auto& s : a.nfa.states) {
        nfa.states.push_back(s);
    }

     for (int i = offset; i < nfa.states.size(); i++) {
        for (auto& t : nfa.states[i].transitions) {
            t.to += offset;
        }
    }

    nfa.addTransition(start, end, TRANS_EPSILON);
    nfa.addTransition(start, a.start + offset, TRANS_EPSILON);
    nfa.addTransition(a.end + offset, end, TRANS_EPSILON);
    nfa.addTransition(a.end + offset, a.start + offset, TRANS_EPSILON);

    return {start, end, nfa};
}

Fragment buildCharClass(const std::vector<char>& chars) {
    EngineNFA nfa;

    int start = nfa.addState();
    int end = nfa.addState(true);

    nfa.initial_state = start;

    CPUTransition t{};
    t.to = end;
    t.type = TRANS_CLASS;

    for (int i = 0; i < 256; i++) {
        t.char_class[i] = false;
    }
    for (char c : chars) {
        t.char_class[(unsigned char) c] = true;
    }

    nfa.states[start].transitions.push_back(t);

    return {start, end, nfa};
}

Fragment buildFromAST(Regex* node) {
    switch (node->type) {
        case REG_CHAR:
            return buildChar(node->value);
        
        case REG_CLASS:
            return buildCharClass(node->char_class);

        case REG_CONCAT: {
            auto a = buildFromAST(node->left);
            auto b = buildFromAST(node->right);
            return concat(a, b);
        }

        case REG_ALT: {
            auto a = buildFromAST(node->left);
            auto b = buildFromAST(node->right);
            return alternate(a, b);
        }

        case REG_STAR: {
            auto a = buildFromAST(node->left);
            return star(a);
        } 
    }

    throw std::runtime_error("Invalid AST");
}

void epsilonClosure(const EngineNFA& nfa, std::vector<bool>& states) {
    std::vector<int> stack;

    for (int i = 0; i < states.size(); i++) {
        if (states[i]) {
            stack.push_back(i);
        }
    }

    while (!stack.empty()) {
        int s = stack.back(); stack.pop_back();

        for (const auto& t : nfa.states[s].transitions) {
            if (t.type == TRANS_EPSILON && !states[t.to]) {
                states[t.to] = true;
                stack.push_back(t.to);
            }
        }
    }
}

bool computeCPU(const EngineNFA& nfa, const std::string& input) {
    std::vector<bool> current(nfa.states.size(), false);
    std::vector<bool> next(nfa.states.size(), false);

    current[nfa.initial_state] = true;
    epsilonClosure(nfa, current);

    for (char c : input) {
        std::fill(next.begin(), next.end(), false);

        for (int s = 0; s < nfa.states.size(); s++) {
            if(!current[s]) {
                continue;
            }

            for (auto& t : nfa.states[s].transitions) {
                if (t.type == TRANS_CHAR && t.symbol == c) {
                    next[t.to] = true;
                }

                if (t.type == TRANS_CLASS && t.char_class[(unsigned char)c]) {
                    next[t.to] = true;
                }
            }
        }
        epsilonClosure(nfa, next);
        current = next;
    }

    for (int s = 0; s < nfa.states.size(); s++) {
        if (current[s] && nfa.states[s].is_accepting) return true;
    }
    return false;
}


struct GPUNFA {
    int num_states;
    int num_transitions;
    int initial_state;

    int* state_offsets;
    int* state_counts;
    unsigned char* is_accepting;

    int* trans_to;
    char* trans_symbol;
    int* trans_type;
    unsigned char* trans_class;
};

struct DeviceNFA {
    int num_states;
    int initial_state;

    int* state_offsets;
    int* state_counts;
    unsigned char* is_accepting;

    int* trans_to;
    char* trans_symbol;
    int* trans_type;
    unsigned char* trans_class;
};

inline bool cudaCheck(cudaError_t err, const char* msg) {
    if (err != cudaSuccess) {
        std::cerr << "CUDA error: " << msg << ": " << cudaGetErrorString(err) << std::endl;
        return false;
    }
    return true;
}


void freeGPUNFA(GPUNFA& gpu) {
    delete[] gpu.state_offsets;
    delete[] gpu.state_counts;
    delete[] gpu.is_accepting;
    delete[] gpu.trans_to;
    delete[] gpu.trans_symbol;
    delete[] gpu.trans_type;
    delete[] gpu.trans_class;
    gpu = GPUNFA{};
}

void freeDeviceNFA(DeviceNFA& d) {
    cudaFree(d.state_offsets);
    cudaFree(d.state_counts);
    cudaFree(d.is_accepting);
    cudaFree(d.trans_to);
    cudaFree(d.trans_symbol);
    cudaFree(d.trans_type);
    cudaFree(d.trans_class);
    d = DeviceNFA{};
}

GPUNFA convertToGPU(const EngineNFA& cpu) {
    GPUNFA gpu{};
    
    gpu.num_states = cpu.states.size();
    gpu.initial_state = cpu.initial_state;

    int total_transitions = 0;
    for (const auto& s : cpu.states) {
        total_transitions += s.transitions.size();
    }
    gpu.num_transitions = total_transitions;

    gpu.state_offsets = new int[gpu.num_states];
    gpu.state_counts = new int[gpu.num_states];
    gpu.is_accepting = new unsigned char[gpu.num_states];

    gpu.trans_to = new int[total_transitions];
    gpu.trans_symbol = new char[total_transitions];
    gpu.trans_type = new int[total_transitions];
    gpu.trans_class = new unsigned char[total_transitions * 256];

    int offset = 0;
    for (int i = 0; i < gpu.num_states; i++) {

        gpu.state_offsets[i] = offset;
        gpu.state_counts[i] = cpu.states[i].transitions.size();
        gpu.is_accepting[i] = cpu.states[i].is_accepting ? 1 : 0;

        for (const auto& t : cpu.states[i].transitions) {
            gpu.trans_to[offset] = t.to;
            gpu.trans_symbol[offset] = t.symbol;
            gpu.trans_type[offset] = t.type;
            for (int j = 0; j < 256; j++) {
                gpu.trans_class[offset * 256 + j] = t.char_class[j] ? 1 : 0;
            }
            offset++;
        }
    }

    return gpu; 
}

DeviceNFA copyToDevice(const GPUNFA& h) {
    DeviceNFA d{};
    d.num_states = h.num_states;
    d.initial_state = h.initial_state;

    cudaMalloc(&d.state_offsets, h.num_states * sizeof(int));
    cudaMalloc(&d.state_counts, h.num_states * sizeof(int));
    cudaMalloc(&d.is_accepting, h.num_states * sizeof(unsigned char));
    
    cudaMalloc(&d.trans_to, h.num_transitions * sizeof(int));
    cudaMalloc(&d.trans_symbol, h.num_transitions * sizeof(char));
    cudaMalloc(&d.trans_type, h.num_transitions * sizeof(int));
    cudaMalloc(&d.trans_class, h.num_transitions * 256 * sizeof(unsigned char));
    
    
    cudaMemcpy(d.state_offsets, h.state_offsets, h.num_states * sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(d.state_counts, h.state_counts, h.num_states * sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(d.is_accepting, h.is_accepting, h.num_states * sizeof(unsigned char), cudaMemcpyHostToDevice);


    cudaMemcpy(d.trans_to, h.trans_to , h.num_transitions * sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(d.trans_symbol, h.trans_symbol , h.num_transitions * sizeof(char), cudaMemcpyHostToDevice);
    cudaMemcpy(d.trans_type, h.trans_type , h.num_transitions * sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(d.trans_class, h.trans_class, h.num_transitions * 256 * sizeof(unsigned char), cudaMemcpyHostToDevice);

    return d;
}

__device__
void epsilonClosureDevice(const DeviceNFA& nfa, bool* states) {
    bool changed = true;

    while (changed) {
        changed = false;

        for (int s = 0; s < nfa.num_states; s++) {
            if (!states[s]) {
                continue;
            }

            int start = nfa.state_offsets[s];
            int count = nfa.state_counts[s];

            for (int t = 0; t < count; t++) {
                int idx = start + t;

                if (nfa.trans_type[idx] == TRANS_EPSILON) {
                    int to = nfa.trans_to[idx];

                    if (!states[to]) {
                        states[to] = true;
                        changed = true;
                    }
                }
            }
        }
    }
}

#define MAX_STATES 128

__global__
void match_kernel (DeviceNFA nfa, const char* inputs, int str_len, int num_strings, unsigned char* results) {
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= num_strings) return;

    const char* str = inputs + tid * str_len;
    bool current[MAX_STATES] = {false};
    bool next[MAX_STATES] = {false};

    current[nfa.initial_state] = true;
    epsilonClosureDevice(nfa, current);

    for (int i = 0; i < str_len; i++) {
        char c = str[i];

        for (int s = 0; s < nfa.num_states; s++) {
            next[s] = false;
        }

        for (int s = 0; s < nfa.num_states; s++) {
            if (!current[s]) {
                continue;
            }
            
            int start = nfa.state_offsets[s];
            int count = nfa.state_counts[s];
            
            for (int t = 0; t < count; t++) {
                int idx = start + t;

                if (nfa.trans_type[idx] == TRANS_CHAR && nfa.trans_symbol[idx] == c) {
                    next[nfa.trans_to[idx]] = true;
                }
                if (nfa.trans_type[idx] == TRANS_CLASS && nfa.trans_class[idx * 256 + (unsigned char)c]) {
                    next[nfa.trans_to[idx]] = true;
                }
            }
        }

        epsilonClosureDevice(nfa, next);

        for (int s = 0; s < nfa.num_states; s++) {
            current[s] = next[s];
        }
    }

    for (int s = 0; s < nfa.num_states; s++) {
        if (current[s] && nfa.is_accepting[s]) {
            results[tid] = 1;
            return;
        }
    }

    results[tid] = 0;
}

std::vector<bool> runOnGPU(const GPUNFA& gpu_nfa, const std::vector<std::string>& inputs) {
    int num_strings = inputs.size();
    int str_len = inputs[0].size();

    std::vector<char> flat(num_strings * str_len);
    for (int i = 0; i < num_strings; i++) {
        memcpy(&flat[i * str_len], inputs[i].data(), str_len);
    }

    char* d_inputs = nullptr;
    unsigned char* d_results = nullptr;

    cudaMalloc(&d_inputs, flat.size());
    cudaMalloc(&d_results, num_strings * sizeof(unsigned char));

    cudaMemcpy(d_inputs, flat.data(), flat.size(), cudaMemcpyHostToDevice);

    DeviceNFA d_nfa = copyToDevice(gpu_nfa);

    int threads = 256;
    int blocks = (num_strings + threads - 1) / threads;

    match_kernel<<<blocks, threads>>>(
        d_nfa,
        d_inputs,
        str_len,
        num_strings,
        d_results
    );

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        std::cerr << "CUDA kernel launch error: " << cudaGetErrorString(err) << std::endl;
        return std::vector<bool>(num_strings, false);
    }

    err = cudaDeviceSynchronize();
    if (err != cudaSuccess) {
        std::cerr << "CUDA kernel sync error: " << cudaGetErrorString(err) << std::endl;
        return std::vector<bool>(num_strings, false);
    }

    std::vector<unsigned char> results_host(num_strings);
    cudaMemcpy(results_host.data(), d_results, num_strings * sizeof(unsigned char), cudaMemcpyDeviceToHost);

    std::vector<bool> results(num_strings);
    for (int i = 0; i < num_strings; i++) {
        results[i] = results_host[i] != 0;
    }

    freeDeviceNFA(d_nfa);
    cudaFree(d_inputs);
    cudaFree(d_results);

    return results;
}

std::vector<std::string> generate_data(int n, int len) {
    std::vector<std::string> data;
    for (int i = 0; i < n; i++) {
        std::string s(len, 'A');
        data.push_back(s);
    }
    return data;
}

int main() {
    EngineNFA cpu;

    cpu.states.resize(2);
    cpu.initial_state = 0;

    cpu.states[1].is_accepting = true;
    cpu.states[0].transitions.push_back({1, TRANS_CHAR, 'A'});

    GPUNFA gpu = convertToGPU(cpu);

    auto data = generate_data(1024, 1);

    auto results = runOnGPU(gpu, data);

    int matches = 0;
    for (bool r : results) if (r) matches++;

    std::cout << "Matches: " << matches << std::endl;

    freeGPUNFA(gpu);
    return 0;
}