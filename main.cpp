/* 
 * File:   main.cpp
 * Author: tomaskrejci
 *
 * Created on May 1, 2013, 10:15 AM
 */

#include <iostream>
#include <string>
#include <vector>

#include "translator.h"

int
main(int argc, char * argv[])
{
    std::vector<std::string> v(argv, argv + argc);
    translator & t = translator::get_instance();

    /* All cases for command line arguments */
	if (v.size() == 1) {
        return t.run("input.txt", "output.txt");
    } else if (v.size() == 3) {
        if (v[1] == "-i")
            return t.run(v[2], "output.txt");
        else if (v[1] == "-o")
            return t.run("input.txt", v[2]);
    } else if (v.size() == 5) {
        if (v[1] == "-i" && v[3] == "-o")
            return t.run(v[2], v[4]);
        else if (v[1] == "-o" && v[3] == "-i")
            return t.run(v[4], v[2]);
    }

    std::cerr << "invalid usage" << std::endl;
	return 1;
}


