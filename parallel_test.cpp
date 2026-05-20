#include <string>
#include <iostream>
#include <vector>
#include <regex>
#include <chrono>
#include <thread>
#include <numeric>
#include <cstdlib>
#include <ctime>
#include <fstream>


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

void regex_worker(
    const std::vector<std::string>& data,
    const std::regex& pattern,
    size_t begin,
    size_t end,
    long& result
) {
    long local = 0;
    for (size_t i = begin; i < end; ++i) {
        if (std::regex_match(data[i], pattern)){
            local++;
        }
    }
    result = local;
}

long parallel_regex_match(
    const std::vector<std::string>& data,
    const std::regex& pattern
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
            regex_worker,
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

int main(){
    int n = 1 << 20;

    std::vector<int> lengths = {10,20,30,40,50,60,70,80,90,100,110,120,130,140,150};
    std::vector<std::vector<std::string>> datasets(lengths.size());
    for (size_t i = 0; i < lengths.size(); i++){
        datasets[i].reserve(n);
        for (int j = 0; j < n; j++){
            datasets[i].push_back(generate_string(lengths[i]));
        }
    }
    srand(time(NULL));

    //std::regex pattern("^[A-Za-z0-9]*Z$");                // ends with z
    //std::regex pattern("^A[A-Za-z0-9]*$");              // starts with A
    //std::regex pattern("^A[A-Za-z0-9]*Z$");             // starts with A ends with Z
    //std::regex pattern("^[A-Za-z0-9]*[A-Z][0-9]$");     // ends with capitalized and then numeral
    //std::regex pattern("^([A-Za-z][0-9])+$");           // letter numeral pairs
    //std::regex pattern("^([A-Z]|[0-9])*$");             // high selectivity => similar throughput (either capitalized or numeral)
    
    /*std::regex pattern(
        R"(\b(?:(?:ass+(?:\s+)?|i+(?:\s+)?|butt+(?:\s+)?|mo(?:(?:m|t|d)h?(?:e|a)?r?)(?:\s+)?)?f(?:(?:\s+)?u+)?(?:(?:\s+)?c+)?(?:(?:\s+)?k+)?(?:(?:e|a)(?:r+)?|i(?:n(?:g)?)?)?(?:s+)?(?:\s+)?(?:hole|head|(?:yo?)?u?)?)+\b)",
        std::regex_constants::icase
    ); //profanity filter*/
    
    /*std::regex pattern(
        R"(#?([\da-fA-F]{2})([\da-fA-F]{2})([\da-fA-F]{2}))",
        std::regex_constants::icase
    );*/
    
    std::regex pattern(
        R"(^([A-Z]([a-z0-9])*|"[A-Z]{0,1}([a-z0-9])*((\. | | & |, |-|\\|\/|! |\? |: |\t)[A-Z]{0,1}([a-z0-9])*)*")((\. | | & |, |-|\\|\/|! |\? |: |\t)[A-Z]{0,1}([a-z0-9])*| "[A-Z]{0,1}([a-z0-9])*((\. | | & |, |-|\\|\/|! |\? |: )[A-Z]{0,1}([a-z0-9])*)*(\.|!|\?){0,1}")*(\.|!|\?)$)",
        std::regex_constants::icase
    ); //Text Regex K
    
    /*std::regex pattern(
        R"(([13][a-km-zA-HJ-NP-Z0-9]{26,33}))",
        std::regex_constants::icase
    );*/
    
    /*std::regex pattern(
        R"(\d{1,3}(?=(\d{3})+(?!\d)))",
        std::regex_constants::icase
    );*/
    
    /*std::regex pattern(
        R"([a-z0-9!#$%&'*+/=?^_`{|}~-]+(?:\.[a-z0-9!#$%&'*+/=?^_`{|}~-]+)*@(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\.)+[a-z0-9](?:[a-z0-9-]*[a-z0-9])?)",
        std::regex_constants::icase
    ); //Email validation*/
    
    std::regex patternx(
        R"()",
        std::regex_constants::icase
    );



    struct Result{
        int length;
        double throughput;
        int matches;
    };

    std::vector<Result> results;
    
    for (size_t i = 0; i < datasets.size(); i++){
        auto start = std::chrono::high_resolution_clock::now();

        long match_count = parallel_regex_match(datasets[i], pattern);

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
    return 0;
    
}
    
