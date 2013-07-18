/* 
 * File:   translator.h
 * Author: tomaskrejci
 *
 * Created on May 1, 2013, 10:25 AM
 */

//##NOTE## - mate pocitac apple s mac os x :) - nenechavajte v taroch bordel co tam nepatri

#ifndef TRANSLATOR_H
#define    TRANSLATOR_H

#include <utility>
#include <map>
#include <string>
#include <vector>
#include <tuple>
#include <thread>
#include <fstream>
#include <utility>
#include <stdexcept>
#include <atomic>
#include <mutex>
#include <queue>

/**
 * Trida prekladace
 *
 * Datove struktury:
 *  - Slovnik je ukladan do mapy - tady asi jeden nic chytrejsiho nevymysli.

 //##NOTE## ale vymyslel by keby chcel...hash map, bucket hash map, trie, toho by bolo...

 *  - Nacteny text je ulozeny ve stringu
 *  - Prelozeny text je vektor wordu
 *  - word je (iterator, bool, string) kde
 *     - iterator je iterator do slovniku na prekladane slovo. Pokud toto slovo
 *       predtim ve slovniku nebylo, tak ho tam pridam. Tim mi muze nabobtnavat
 *       slovnik, ale predpokladam, ze pro prumerna vstupni data (skutecny text
 *       v cizim jazyce s patricnym slovnikem) bude slovnik obsahovat drtivou
 *       vetsinu vyrazu v textu. Na oplatku ziskam konstantni update slovniku co
 *       do velikosti vstupniho textu (ostatni implementace ktere jsem uvazoval
 *       by potrebovali prakticky znova zacit prekladat cely text).
 *     - bool urcuje zda-li ma byt prvni pismeno slova velke
 *     - string je posloupnost whitespace symbolu (v zadani pozadavek na jejich
 *       zachovani)
 *
 * Pouzite datove striktury by mely poskytovat relativne dobry kompromis mezi
 * pametovou a casovou narocnosti aplikace. Alespon pro "prumerny pripad" (i
 * trouba by vymyslel, ze pro dlouhy text a maly slovnik to neni nic moc).
 *
 * Zasadni invariant : vzdy ma maximalne jedno vlakno exkluzivni pristup ke
 *    slovniku a ostatni vlakna jej nikdy nesmi menit
 *
 * Cely zdrojovy kod prosel refactoringem a nemely by se v nem vyskytovat
 * redundantni kusy kodu.
 * 
 * Na zaver chci podotknout, ze tato uloha neni prilis vhodna pro implementaci
 * jako vicevlaknova, protoze po vyzkouseni mnoha odlisnych zpusobu jak mohou
 * jednotliva vlakna pracovat a jakymi prostredky se mohou synchronizovat jsem
 * dospel k (celkem jednoznacnemu) zaveru, ze tato aplikace by byla vhodnejsi 
 * jako jednovlaknova. 
 * Tento zaver tvrdim na zaklade nekolika pozorovani:
 *  - Vsechny operace trvaji zanedbatelny cas oproti praci s std::map
 *  - Synchronizace pomoci cehokoliv co vyvolava syscall (mutex, condition
 *    variable) je velmi pomala.
 *  - Synchronizace je treba provadet velmi casto
 *  - Vsechna vlakna krome toho pracujiciho s std::map vetsinu casu nemaji co 
 *    delat a cekaji na to s std::map. Aktivni synchronizace (pomoci aktivniho
 *    cekani a atomickych promennych) se ukazala byt sice relativne schudna
 *    varianta, ale VELMI vytezovala CPU (radove vice nez je treba) protoze
 *    se vetsinu casu stalo ve smycce.
 *  - Operace insert nad std::map se ukazala byt jako temer 
 *    nereentrantizovatelna protoze nejsem schopen dostatecne zarucit vkladani
 *    do ruznych podstromu a norma nezarucuje thread-safe kontejnery
 *  - Nejlepsi odhad pro rychlost prace s mapou co se mi povedl je cca.
 *    mapa : vse ostatni = 7 : 1 az 8 : 1
 */
class translator {    
    using dictionary = std::map<std::string, std::string>;
	//##NOTE## pozrite si unordered map
    using word = std::tuple<dictionary::iterator, bool, std::string>;

	//##NOTE## tu by stacil aj enum
    enum class job_kind {
        dictionary,
        update,
        begin,
        end,
        show,
        translate
    };
    using job = std::pair<job_kind, std::string>;

	//##NOTE## z osobneho hladiska mi "neco_" zapis hodne vadi, pretoze to podtrzitko na konci sa hrozne blbo cte.
    dictionary dict_;
    std::string in_s_;
    std::vector<word> in_v_;

    std::string default_input_filename_;
    std::string default_output_filename_;
   
    std::mutex jobs_mutex_;
    std::queue<job> jobs_;
    
    void new_dictionary(std::string dict_filename);
    void update_dictionary(std::string dict_filename);
    void begin(std::string input_filename);
    void end(std::string output_filenames);
    void show();
    void translate(const std::string & s);
    
    std::size_t update_dictionary_helper(std::ifstream && file);
                   
    void print(std::string msg) const;
    std::string trim(const std::string & str) const;
    std::string to_lower(const std::string & str) const;
        
    bool should_end_var_ = false;
    void set_should_end();
    bool should_end() const;

    void updater();
    void exec();

public:
    translator();
    int run(std::string input, std::string output);
};

#endif	/* TRANSLATOR_H */

