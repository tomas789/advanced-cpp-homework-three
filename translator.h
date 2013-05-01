/* 
 * File:   translator.h
 * Author: tomaskrejci
 *
 * Created on May 1, 2013, 10:25 AM
 */

#ifndef TRANSLATOR_H
#define	TRANSLATOR_H

#include <utility>
#include <map>
#include <string>
#include <vector>
#include <tuple>
#include <thread>
#include <fstream>
#include <utility>
#include <regex>
#include <stdexcept>
#include <atomic>

class translator {
    static translator * instance_;
    translator() = default;
public:
    static translator & get_instance();

    using dictionary = std::map<std::string, std::string>;
    using word = std::tuple<dictionary::iterator, bool, std::string>;

private:
    dictionary dict_;
    std::string in_s_;
    std::vector<word> in_v_;

    std::string default_input_filename_;
    std::string default_output_filename_;
   
    bool is_translating_ = false;
    
    void new_dictionary(std::string dict_filename);
    void update_dictionary(std::string dict_filename);
    void begin(std::string input_filename);
    void end(std::string output_filenames);
    
    std::size_t update_dictionary_helper(std::ifstream && file);
    void begin_helper(std::vector<word> * out, std::size_t from, std::size_t to);
    
    std::size_t jump_to_word(std::size_t i) const;
               
    void print(std::string msg) const;
    std::string trim(const std::string & str) const;
    std::string to_lower(const std::string & str) const;
        
    bool should_end_var_ = false;
    void set_should_end();
    bool should_end() const;

public:
    int run(std::string input, std::string output);
    bool is_translating() const;
};

#endif	/* TRANSLATOR_H */

