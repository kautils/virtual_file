
#ifdef TMAIN_KAUTIL_CACHE_VIRTUAL_FILE_SHARED

#include "virtual_file.h"
#include <vector>
#include <filesystem>
#include <iostream>
#include <cassert>

int main(){

    auto generate_data=[](auto  const&s , auto const&e){
        auto buf = std::vector<uint64_t>{};
        for(auto i = s; i <= e; ++i)buf.push_back(i);
        return buf;
    };

    auto lmb =[](auto data,auto const& size){
        auto arr = (uint64_t *) data;
        auto count = size / sizeof(uint64_t);
        for(int i = 0; i < count; ++i){
            std::cout << arr[i] << std::endl;
        }
    };


    namespace fs = std::filesystem;
    auto o = std::string("accessor");
    auto x = kautil::cache::VirtualFile{o.c_str(), sizeof(uint64_t)};
    for(auto range : {
        std::pair{50,100}
        ,{170,200}
        ,{120,150}
    }){
        auto d = generate_data(range.first,range.second);
        x.add(range.first,range.second,d.data());
    }
    x.arrange();


    for(auto range : {
        std::pair{0,49}
        ,{101,119}
        ,{151,169}
        ,{201,220}
        ,{221,250}
    }){
        auto d = generate_data(range.first,range.second);
        x.add(range.first,range.second,d.data());
    }
    x.arrange();
    auto data = x.copy(123,456);
    lmb(data.data(),data.size());

    std::cout << "check whether between 50 and 100 is cached." << std::endl;
    x.load(o.c_str(), sizeof(uint64_t));
    auto buf = std::vector<int8_t>((100-50+1)*sizeof(uint64_t),0);
    buf.resize(x.gap(buf.data(),50,100));
    if(buf.empty()) {
        std::cout << "cached" << std::endl;
    }else{
        std::cout << "must be cached but dosen't" << std::endl;
        assert(false);
    }

    // virtual file < LeapManager




    return 0;

}


#endif
