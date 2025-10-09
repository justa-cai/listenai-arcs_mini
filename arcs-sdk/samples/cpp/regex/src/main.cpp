#include <iostream>
#include <string>
#include <regex>

using namespace std;

void match_pattern(const string& pattern, const string& str) {
    regex re(pattern);
    if (regex_search(str, re)) {
        cout << "Match: '" << pattern << "' in '" << str << "'" << endl;
    } else {
        cout << "No match: '" << pattern << "' in '" << str << "'" << endl;
    }
}

void extract_matches(const string& pattern, const string& str) {
    regex re(pattern);
    smatch matches;
    
    cout << "Pattern: '" << pattern << "'" << endl;
    cout << "String: '" << str << "'" << endl;
    
    string::const_iterator searchStart(str.cbegin());
    int i = 0;
    while (regex_search(searchStart, str.cend(), matches, re)) {
        for (size_t j = 0; j < matches.size(); ++j) {
            cout << "Match " << i << ": " << matches[j].str() << endl;
            i++;
        }
        searchStart = matches[0].second;
    }
    
    if (i == 0) {
        cout << "No matches found for pattern '" << pattern 
             << "' in string '" << str << "'" << endl;
    }
}

int main(int argc, char **argv) {
    cout << "=== C++ Regex Sample ===\n" << endl;

    // Basic pattern matching
    cout << "1. Basic pattern matching:" << endl;
    match_pattern("quick", "The quick brown fox");
    match_pattern("^The", "The quick brown fox");
    match_pattern("fox$", "The quick brown fox");
    match_pattern("\\s[a-z]{5}\\s", "The quick brown fox");
    
    cout << "\n2. Extracting matches with groups:" << endl;
    extract_matches("([a-z]+)\\s+([a-z]+)", "hello world");
    extract_matches("(\\d{4})-(\\d{2})-(\\d{2})", "Date: 2023-05-26");
    extract_matches("([a-zA-Z]+) (\\d+), (\\d+)", "May 26, 2023");

    cout << "Regex Sample Completed\r\n";

    return 0;
}