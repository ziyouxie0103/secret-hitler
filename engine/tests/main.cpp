#include <iostream>
#include <string>

void run_players_5_tests();
void run_players_6_tests();
void run_players_7_tests();
void run_players_8_tests();
void run_players_9_tests();
void run_players_10_tests();

int main(int argc, char** argv) {
    const std::string suite = argc > 1 ? argv[1] : "all";

    if (suite == "5" || suite == "all") {
        run_players_5_tests();
    }
    if (suite == "6" || suite == "all") {
        run_players_6_tests();
    }
    if (suite == "7" || suite == "all") {
        run_players_7_tests();
    }
    if (suite == "8" || suite == "all") {
        run_players_8_tests();
    }
    if (suite == "9" || suite == "all") {
        run_players_9_tests();
    }
    if (suite == "10" || suite == "all") {
        run_players_10_tests();
    }

    if (suite != "all"
        && suite != "5"
        && suite != "6"
        && suite != "7"
        && suite != "8"
        && suite != "9"
        && suite != "10") {
        std::cerr << "Unknown test suite: " << suite << '\n';
        return 1;
    }

    std::cout << "All engine tests passed.\n";
    return 0;
}
