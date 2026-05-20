#include <string>
#include <iostream>
#include <vector>
#include <regex>
#include <chrono>
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

struct Result{
    int length;
    double throughput;
    int matches;
};

int main(){
    int n = 1 << 20;

    std::vector<int> lengths = {10,20,30,40,50,60,70,80,90,100,110,120,130,140,150};
    std::vector<std::vector<std::string>> datasets(lengths.size());
    for (auto& d : datasets){
        d.reserve(n);
    }

    srand(time(NULL));

    for (int i = 0; i < n; i++){
        for (size_t j = 0; j < lengths.size(); j++){
            datasets[j].push_back(generate_string(lengths[j]));
        }
    }

    //std::regex pattern("^[A-Za-z0-9]*Z$");                // ends with z
    //std::regex pattern("^A[A-Za-z0-9]*$");              // starts with A
    //std::regex pattern("^A[A-Za-z0-9]*Z$");             // starts with A ends with Z
    //std::regex pattern("^[A-Za-z0-9]*[A-Z][0-9]$");     // ends with capitalized and then numeral
    //std::regex pattern("^([A-Za-z][0-9])+$");           // letter numeral pairs
    //std::regex pattern("^([A-Z]|[0-9])*$");             // high selectivity => similar throughput (either capitalized or numeral)

    std::regex pattern(
        R"(^([A-Z]([a-z0-9])*|"[A-Z]{0,1}([a-z0-9])*((\. | | & |, |-|\\|\/|! |\? |: |\t)[A-Z]{0,1}([a-z0-9])*)*")((\. | | & |, |-|\\|\/|! |\? |: |\t)[A-Z]{0,1}([a-z0-9])*| "[A-Z]{0,1}([a-z0-9])*((\. | | & |, |-|\\|\/|! |\? |: )[A-Z]{0,1}([a-z0-9])*)*(\.|!|\?){0,1}")*(\.|!|\?)$)",
        std::regex_constants::icase
    ); //Text Regex K

    /*std::regex pattern(
        R"([a-z0-9!#$%&'*+/=?^_`{|}~-]+(?:\.[a-z0-9!#$%&'*+/=?^_`{|}~-]+)*@(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\.)+[a-z0-9](?:[a-z0-9-]*[a-z0-9])?)",
        std::regex_constants::icase
    ); //Email validation*/

    std::vector<Result> results;

    for (size_t i = 0; i < datasets.size(); i++){
        long match_count = 0;

        auto start = std::chrono::high_resolution_clock::now();
        for (const auto& x : datasets[i]){
            if (std::regex_match(x, pattern)){
                match_count++;
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        double time = std::chrono::duration<double>(end - start).count();
        double throughput = n / time;

        results.push_back({lengths[i], throughput, (int)match_count});

        std::cout << "Finsished dataset with length " << lengths[i] << "\n" << std::flush;

    }
    std::ofstream csv("results.csv", std::ios::app);
    csv << "mode,length,throughput_strings_per_sec,matches\n";
    for (const auto& r : results){
        csv << "single" << "," << r.length << "," << r.throughput << "," << r.matches << "\n";
    }
    
    return 0;
    
}
    
