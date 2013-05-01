/*
 * translator.cpp
 *
 *  Created on: Apr 30, 2013
 *      Author: tomaskrejci
 */

#include "translator.h"

#include <iostream>
#include <functional>
#include <sstream>
#include <cctype>


translator * translator::instance_ = nullptr;

translator & translator::get_instance()
{
	if (instance_ == nullptr)
		instance_ = new translator();

	return * instance_;
}

void translator::new_dictionary(std::string dict_filename)
{
	if (is_translating())
		return update_dictionary(dict_filename);
    
    print("loading dictionary");

	std::ifstream dict(dict_filename);
    if (!dict.is_open()) 
		throw std::runtime_error("unable to open file : " + dict_filename);
    
    dict_.clear();

	std::stringstream s;
    s << update_dictionary_helper(std::move(dict));
    print(s.str() + " words loaded");
}

void translator::update_dictionary(std::string dict_filename)
{
    print("updating dictionary");
    
	std::ifstream dict(dict_filename);
	if (!dict.is_open()) 
		throw std::runtime_error("unable to open file : " + dict_filename);

    std::stringstream s;
    s << update_dictionary_helper(std::move(dict));
    print(s.str() + " words loaded");
}

void translator::begin(std::string input_filename)
{
    // Did user supplied filename or default
    if (input_filename.empty())
        input_filename = default_input_filename_;

    // Open file
    std::ifstream input(input_filename);
    if (!input.is_open()) 
		throw std::runtime_error("unable to open file : " + input_filename);

    // Read whole file into memory
    input.seekg(0, std::ios::end);
    in_s_.resize(input.tellg());
    input.seekg(0, std::ios::beg);
    input.read(&in_s_[0], in_s_.size());


    unsigned hc = std::thread::hardware_concurrency();
    std::vector<std::vector<word>> jobs(hc);
    std::size_t last = 0, next;

    // All threads have to have equal sized job
    for (unsigned i = 0; i < hc; ++i) {
        next = jump_to_word(last + in_s_.length() / hc);
        std::thread t(&translator::begin_helper, *this, nullptr, last, next);
        t.detach();
        last = next;
    }
}

void translator::end(std::string output_filename)
{
    if (output_filename.empty())
        output_filename = default_output_filename_;

    set_should_end();
}

std::size_t translator::update_dictionary_helper(std::ifstream && file)
{
    std::string line, key, value;
	std::size_t o, words = 0;
	while (file) {
		std::getline(file, line);
        if ((o = line.find(':')) == std::string::npos) continue;
		key = trim(line.substr(0, o));
		value = trim(line.substr(o + 1, std::string::npos));
		dict_.insert(std::make_pair(key, value));
        ++words;
	}
    
    return words;
}

void translator::begin_helper(std::vector<word> * out, std::size_t from, std::size_t to)
{
    // [from, to)
}

std::size_t translator::jump_to_word(std::size_t i) const
{
    // Find end of word if not beginning of string
    if (i) for (; i < in_s_.length() && !std::isspace(in_s_[i]); ++i);

    // Skip whitespaces
    for (; i < in_s_.length() && std::isspace(in_s_[i]); ++i);
    return i;
}

void translator::print(std::string msg) const
{
    std::cout << " >>> " << msg << std::endl;
}

std::string translator::trim(const std::string & str) const
{
    std::size_t s = str.find_first_not_of(" \n\r\t");
    std::size_t e = str.find_last_not_of(" \n\r\t");

    if(std::string::npos == s || std::string::npos == e) return "";
    else return str.substr(s, e-s+1);
}

std::string translator::to_lower(const std::string& str) const
{
    std::string s = str;
    for (auto & c : s)
        if (c >= 'A' && c <= 'Z')
            c = c + 'a' - 'A';
    
    return s;
}

void translator::set_should_end()
{
    should_end_var_ = true;
}

bool translator::should_end() const
{
    return should_end_var_;
}

int translator::run(std::string input_filename, std::string output_filename)
{
    default_input_filename_ = input_filename;
    default_output_filename_ = output_filename;

    std::cout << " -- Wellcome User! I am high-speed translator" << std::endl;

    std::string command, arg, read;
	while (! should_end()) {
        std::getline(std::cin, read);
        read = trim(read);
		std::size_t p = read.find(' ');
        command = to_lower(read.substr(0, p));
        if (read.length() > p) 
            arg = trim(read.substr(p + 1, std::string::npos));
        else arg.empty();
        
        try {
            if (".dictionary" == command) new_dictionary(arg);
            else if (".update" == command) update_dictionary(arg);
            else if (".begin" == command) begin(arg);
            else if (".end" == command) end(arg);
            else print("unknown command " + command);
        } catch (std::runtime_error & e) {
            print(e.what());
        } catch (...) {
            std::cerr << "unknown exception" << std::endl;
            return 1;
        }
	}

}

bool translator::is_translating() const
{
    return is_translating_;
}