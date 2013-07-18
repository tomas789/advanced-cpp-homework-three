/*
 * translator.cpp
 *
 *  Created on: Apr 30, 2013
 *      Author: tomaskrejci
 */

#include "translator.h"

#include <iostream>
#include <functional>
#include <cctype>
#include <condition_variable>
#include <chrono>

translator::translator()
{
}

/**
 * Nacita novy slovnik
 *
 * Pokud je volan pred .begin -> nahrazuje aktualni slovnik. Pokud je volan az
 * po .begin -> aktualizuje stavajici slovnik.
 *
 * @param dict_filename
 */
void translator::new_dictionary(std::string dict_filename)
{
    // Should I only update dictionary?
    if (in_v_.size()) return update_dictionary(dict_filename);
	//##NOTE## pouzijte "empty()" to je jasnejsi
	//##NOTE## toto hrozne mate, ked vracite void tak nevracejte volanim neceho co vraci void, jde to ale zle sa to cita
    
    print("loading dictionary");

	std::ifstream dict(dict_filename);
    if (!dict.is_open()) 
        throw std::runtime_error("unable to open file : " + dict_filename);
    
    dict_.clear();

    auto count = update_dictionary_helper(std::move(dict));
    print(std::to_string(count) + " words loaded");
}

/**
 * Aktualizuje aktualni slovnik
 *
 * Pokud je volana jeste pred prvnim volanim .dictionary, je mu ekvivalentni
 * (updatuje se prazdny slovnik)
 *
 * @param dict_filename
 */
void translator::update_dictionary(std::string dict_filename)
{
    print("updating dictionary");
    
	std::ifstream dict(dict_filename);
	if (!dict.is_open()) 
        throw std::runtime_error("unable to open file : " + dict_filename);

    auto count = update_dictionary_helper(std::move(dict));
    print(std::to_string(count) + " words loaded");
}

/**
 * Zacne prekladat vstupni text
 *
 * Uvazovane moznosti:
 *  - Paralelene budou probihat vsechny operace (cteni i preklad)
 *  - Nacte se cely text a pote se zavolaji na jeho jednotlive casti vlakna pro
 *    preklad (velmi usetrim na synchronizaci ktera prakticky odpada, ale musim
 *    pockat na nacteni vsech dat)
 *  - Hybridni pristip - Zacne se delat vse paralelne a az se nactou veskera
 *    data, tak se prepne na separovane ukoly.
 *
 * Stejne jako ve fci update_dictionary_helper se ukazalo, ze vkladani do mapy
 * je nejnarocnejsi ukol a vysledna implementace se snazi o to, aby jedno vlakno
 * pouze vkladalo do mapy s soucasne s tim delalo co nejmene veci. Dalsi vlakno
 * tomuto pripravuje co mozna nejvice. Tento pristup se ukazal byt nejvyhodnejsi
 * ze vsech uvazovanych.
 * 
 *
 * @param input_filename
 */
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
    std::size_t filesize = input.tellg();
    input.seekg(0, std::ios::beg);
    //input.read(&in_s_[0], in_s_.size());

    const std::size_t lim = 2048;

    using word_helper = std::tuple<std::string, bool, std::string>;
    std::vector<word_helper> buffer(lim);

    std::atomic<size_t> producent = ATOMIC_VAR_INIT(0);
    std::atomic<size_t> consument = ATOMIC_VAR_INIT(0);
    std::condition_variable cv;
    std::mutex m;
    bool producing = true;

    // Pripravit misto - umoznuje se vyhnout budoucimu kopirovani pri nafukovani
    in_s_.reserve(filesize);

    std::thread p([&] {
        std::unique_lock<std::mutex> lock(m);
        while (input) {
            std::string word;
            while (!std::isspace(input.peek()) && input)
                word += input.get();
            bool upper = std::isupper(word[0]);
            word = to_lower(word);
            std::string spaces;
            
            while (std::isspace(input.peek()))
                spaces += input.get();

            while (producent - consument >= lim) cv.wait(lock);

            buffer[producent % buffer.size()] = std::make_tuple(
                    std::move(word),
                    std::move(upper),
                    std::move(spaces));

            ++producent;
        }

        producing = false;
    });

    std::thread c([&] {
        while (producing || producent > consument) {
            while (producent == consument) cv.notify_one();
            
            auto item = std::move(buffer[consument % lim]);
            auto ins = dict_.emplace(std::get<0>(item), std::get<0>(item));
            in_v_.emplace_back(
                    std::get<0>(ins),
                    std::get<1>(item), 
                    std::get<2>(item));
            
            ++consument;
        }
    });

    p.join();
    c.join();
    input.close();

    in_s_.clear();
}

void translator::end(std::string output_filename)
{
    if (output_filename.empty())
        output_filename = default_output_filename_;

    std::ofstream output(output_filename);

    if (! output.is_open())
        throw std::runtime_error("unable to open file : " + output_filename);

    for (auto & t : in_v_) {
        // Dodelej velikost pismen
        std::string s = std::get<0>(t)->second;
        if (std::get<1>(t))
            s[0] = std::toupper(s[0]);
        else
            s[0] = std::tolower(s[0]);

        output << s + std::get<2>(t);
    }

    output.close();
    set_should_end();
}

void translator::show()
{
    for (auto & i : dict_) 
        std::cout << i.first << " : " << i.second << std::endl;
}

void translator::translate(const std::string& s) {
    std::cout << s << " : " << dict_[s] << std::endl;
}

/**
 * Pomocna funkce pro nacitani slovniku
 *
 * Ironii je, ze nejlepsi vykon poskytla implementace jeko jednovlaknova funkce.
 * Ukazalo ze, ze vetsinu case spotrebuje vkladani do mapy a to nejde delat
 * najednou nekolika vlakny. Navic potrebna synchronizace pouze spotrebovava
 * prostredky a celou aplikaci zpomaluje

 //##NOTE## mozno keby ste skusil unorderedmap tak by vam to dalo ine vysledky :)
 //##NOTE## preco ste si napr neskusil naimplementovat hashmapu s rw lockom, ta by bola efektivnejsia nez strom

 *
 * Statistika (534 MB slovnik unikatnich vyrazu stejne delky):
 *   1 vlakno: 49s
 *   2 vlakna lock free aktivni cekani: 42s (nasobne vice vytezuje CPU)
 *   2 vlakna synchronizace mutexem: 1min 53s
 *   4 vlakna lock free aktivni cekani: 2min 13s (nasobne vice vytezuje CPU)
 *   4 vlakna synchronizace mutexem: 1min 25s
 *
 * Pri volbe mezi mizernym vykonem s relativne malou narocnosti a o neco malo
 * mene mizernym vykonem pro velmi vysoke zatezi volim prvni mizernou variantu.
 * Abych ale ukazal, ze umim pouzivat vlakna ponechal jsem ponekud odlehcenou
 * (jeden by rekl az zbytecnou vzhledem k rychlosti produkce producenta)
 * implementaci s pouzitim vlaken.
 *
 * Velikost zasobniku (promenna lim) pro rozumne velka cisla neovlivnuje rychlost;
 * Rekneme, ze nad 1024 uz byla rychlost konstantni. Mensi hodnoty zpusobovaly
 * caste prepinani condition_variable a castecne omezovaly vykon. Doporucuji
 * tuto hodnotu ponechat relativne malou aby zvytecne neubirala pamet.
 *
 * Poznamka: Po dalsim experimentovani se ukazalo byt vyhodne presunout veskerou
 * praci do producenta a konzumentovi ponechat pouze vkladani do mapy (nezdrzuje
 * se zbytecnostmi vlakno, ktere ma prace hodne)
 *
 * @param file
 * @return
 */
std::size_t translator::update_dictionary_helper(std::ifstream && file)
{
    const std::size_t lim = 2048;
    std::vector<std::pair<std::string, std::string>> v(lim);
    bool producing = true;
    std::atomic<std::size_t> producer = ATOMIC_VAR_INIT(0);
    std::atomic<std::size_t> consumer = ATOMIC_VAR_INIT(0);
    std::condition_variable produce;
    std::mutex m;

    std::thread p([&] {
        std::unique_lock<std::mutex> lock(m);
        std::string line, key, value;
        std::size_t o;
        while (file) {
            // Is buffer full?
            while (producer - consumer >= lim) produce.wait(lock);
            std::getline(file, line);

            if ((o = line.find(':')) == std::string::npos)
                continue;

            key = trim(line.substr(0, o));
            value = trim(line.substr(o + 1, std::string::npos));
            
            v[producer % lim] = std::move(make_pair(key, value));
            ++producer;
        }
        
        producing = false;
    });

    std::thread c([&]() {
        decltype(v)::value_type pair;
        while (true) {
            if (producing || producer > consumer) {
                while (producer == consumer) produce.notify_one();
                pair = std::move(v[consumer % lim]);
                ++consumer;
            } else break;

            dict_.insert(std::move(pair));
        }
    });


	//##NOTE## a ina kuzelna moznost by bola si to rovno ten subor namapovat do pamete a priamo s nim operovat. :)
    p.join();
    c.join();

    file.close();

    return producer;
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

void translator::updater()
{
    while (true) {
        /* Na tomto miste bych mel delat update slovniku ale vzhledem k tomu
         * ze autorovi zadani se perfektne podarilo zamlzit co ma tento
         * pravidelny update vlastne delat tak tu nechavam pouze kostru
         * abych ukazal ze vim jak se to ma delat, ale nic toto vlakno
         * nedela.
         */

		//##NOTE## - zaujimave ze to vasi kolegovia pochopili, autor sa na prednaske ptal ci tomu vsetci rozumeju, existuje moznost napsat mail alebo napisat do fora.
		//##NOTE## - skoda ze student neuvazil to ze autor chcel simulovat to ze napr prekladate obrovsku kus textu a v 1/2 prekladu niekomu napadne ze zle prelozil 
		//##NOTE## - hromadku slov. to je tak zamlzeno?
		//##NOTE## - student nevyuzil moznost zeptat se na prednaske a teda jeho zatim docela znesitelny zdrojak dostane docela bodovou ujmu

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void translator::exec()
{
    job todo;
    while (true) {
        jobs_mutex_.lock();
        if (jobs_.size()) {
            todo = jobs_.front();
            jobs_.pop();
            jobs_mutex_.unlock();
        } else {
            jobs_mutex_.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        try {
            switch (todo.first) {
                case job_kind::dictionary:
                    new_dictionary(todo.second);
                    break;
                case job_kind::update:
                    update_dictionary(todo.second);
                    break;
                case job_kind::begin:
                    begin(todo.second);
                    break;
                case job_kind::end:
                    end(todo.second);
                    break;
                case job_kind::show:
                    show();
                    break;
                case job_kind::translate:
                    translate(todo.second);
                    break;
            }
        } catch(std::runtime_error & e) {
            print(e.what());
        } catch(...) {
            print("unknown exception - exiting");
            set_should_end();
        }

        
    }
}

int translator::run(std::string input_filename, std::string output_filename)
{
    default_input_filename_ = input_filename;
    default_output_filename_ = output_filename;
    
    std::vector<std::string> commands;

    std::cout << " -- Welcome User! I am high-speed translator" << std::endl;

    std::thread updater(&translator::updater, this);
    updater.detach();

    std::thread exec(&translator::exec, this);
    exec.detach();

    std::string command, arg, read;
	while (! should_end()) {
            // Prompt
            std::cout << "# ";
            
            // Read line
            std::getline(std::cin, read);
            
            // Load command from history
            if ('!' == trim(read)[0]) {
                auto command_number = std::stoi(
                    read.substr(1, std::string::npos));

                if (command_number > commands.size())
                    print("unknown command number");
                else
                    read = commands[command_number];
            }
            
            // Save command to history as raw data
            commands.push_back(read);
            read = trim(read);
            
            // Command is separated by space from argument
            // commands are unary operators
            std::size_t p = read.find(' ');
            command = to_lower(read.substr(0, p));
            
            // Does the command have an argument?
            if (read.length() > p) 
                arg = trim(read.substr(p + 1, std::string::npos));
            else 
                arg.empty();

            // Command processing
            std::lock_guard<std::mutex> _(jobs_mutex_);

            if (".dictionary" == command) 
                jobs_.push(std::make_pair(job_kind::dictionary, arg));
            else if (".update" == command) 
                jobs_.push(std::make_pair(job_kind::update, arg));
            else if (".begin" == command) 
                jobs_.push(std::make_pair(job_kind::begin, arg));
            else if (".end" == command)
                jobs_.push(std::make_pair(job_kind::end, arg));
            else if (".show" == command) 
                jobs_.push(std::make_pair(job_kind::show, arg));
            else if (".translate" == command)
                jobs_.push(std::make_pair(job_kind::translate, arg));
            else if (".history" == command) {
                for (unsigned i = 0; i < commands.size(); ++i)
                    std::cout << i << ": " << commands[i] << std::endl;
            } else print("unknown command " + command);

	}

}
