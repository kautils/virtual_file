#include "virtual_file.h"
#include "kautil/cache/cache.h"
#include <vector>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <iostream>
#include <algorithm>

namespace kautil{
namespace cache{


    struct InternalVFData{
        void * offset_data(pos_type const& offset, size_type const& size){
            if(data.size() <= (offset + size)) data.resize(offset+size);
            return &data[offset];
        }
        std::vector<uint8_t> data;
    };
    struct vf_elem{
        kautil::cache::size_type s;
        kautil::cache::size_type e;
        std::filesystem::path p;
    };

    void VFData::move_to(VFData * dst){
        if(dst->m) delete dst->m;
        dst->m = m;
        m = nullptr;
    }

    VFData VFData::factory(){ return VFData{false}; };
    VFData::~VFData(){ if(m)delete m; }
    void * VFData::data(){
        if(!m) return nullptr;
        return m->data.data();
    }
    size_type VFData::size(){
        if(!m) return 0;
        return m->data.size();
    }
    VFData::VFData(VFData&& v){
        if(m) delete m;
        m = v.m;
        v.m = nullptr;
    }
    VFData::VFData(bool) : m(new InternalVFData) {};

    struct Internal{
        Internal(const char * o,size_type const& block_size) /*: o(o),block_size(block_size)*/{
            init(o,block_size);
        }
        bool conjuct_with_existing_elem(pos_type const& ins_s,pos_type const& ins_e,void * data,size_type const& data_size){
            namespace fs = std::filesystem;
            struct {
                vf_elem * t = nullptr;
                bool is_former = false;
            } conjuction;

            for(auto itr = v.begin(); itr != v.end(); ++itr){
                if(ins_s - itr->e == 1){ conjuction.t = itr.base(); break; }
                if(itr->s - ins_e == 1){ conjuction.t = itr.base(); conjuction.is_former=true; break; }
            }

            if(!conjuction.t) return false;
            if(!conjuction.is_former){
                {
                    auto ofs = std::ofstream {conjuction.t->p,std::ios::binary|std::ios::app};
                    ofs.write((char *)data,data_size);
                }
                auto p = fs::path(o)/(std::to_string(conjuction.t->s) + "_" + std::to_string(ins_e));
                fs::rename(conjuction.t->p,p);
                conjuction.t->p = p;
                conjuction.t->e = ins_e;
                return true;
            }else{
                auto p = fs::path(o)/(std::to_string(ins_s) + "_" + (std::to_string(conjuction.t->e)));
                {
                    std::ofstream{p,std::ios::binary}
                        .write((char *)data,data_size)
                        << std::ifstream{conjuction.t->p,std::ios::binary}.rdbuf();
                }

                fs::remove(conjuction.t->p);
                conjuction.t->p = p;
                conjuction.t->s = ins_s;

                return true;
            }
        }

        void clear(){
            c.clear();
            v.clear();
            o.clear();
            block_size=npos;
        }


        void init(const char * output_dir,size_type const& size){
            o = output_dir;
            block_size = size;
            namespace fs = std::filesystem;
            if(!fs::exists(o)) fs::create_directories(o);
            for(auto const& p : fs::directory_iterator{o}){
                if(fs::is_directory(p)) continue;
                v.resize(v.size() + 1);
                auto & t = v.back();
                t.p = fs::canonical(p.path());
                auto fn = p.path().filename().string();
                t.s = static_cast<size_type>(std::atoll(fn.c_str()));
                t.e  = atoll(strpbrk(fn.c_str(),"_")+1);
                c.add(t.s,t.e);
            }
            /*c.arrange_stack();*/
        }


        kautil::cache::Cache c;
        uint64_t block_size = npos;
        std::vector<vf_elem> v;
        std::string o;

    };




    size_type VirtualFile::gap(void * dst,uint64_t const& want_s,uint64_t const& want_e){ return m->c.make_gap(dst,want_s,want_e); }
    static bool sortf(vf_elem const& l, vf_elem const& r ){ return l.s < r.s; }
    VirtualFile::~VirtualFile(){ if(m) delete m; }
    VirtualFile::VirtualFile(const char * output_dir, size_type const& block_size ) : m(new Internal(output_dir,block_size)){

    }
    bool VirtualFile::load(const char *output_dir, const size_type &block_size) {
        m->clear();
        m->init(output_dir,block_size);
        return true;
    }


const size_t &VirtualFile::block_size() { return m->block_size; }


    VFData VirtualFile::gap(){
        auto res = VFData{false};
        auto current = m->c.stack();
        { // get blanck
            auto const& bsize =current->byte_size();
            static auto const& esize = sizeof(pos_type);
            if(bsize > esize){
                auto s = current->data() + esize;
                res.m->data.assign((uint8_t*)s, (uint8_t*)s + (bsize - esize));
            }
        }
        return res;
    }


    VFData VirtualFile::gap(pos_type const& s, pos_type const& e){
        auto res = VFData{false};
        res.m->data.resize(m->c.gap_bytes_max()); // allocate the size of (minimum requirement + 2 elments)
        res.m->data.resize(m->c.make_gap(res.m->data.data(),s,e)); // reallocate minimum
        return res;
    }


    void VirtualFile::add(
            pos_type const& ins_s
            ,size_type const& ins_e
            ,void * data
            ){
        namespace fs = std::filesystem;
        if(m->c.count(ins_s,ins_e)) return;
        auto const& data_size = (ins_e-ins_s+1)*m->block_size;
        if(!m->conjuct_with_existing_elem(ins_s,ins_e,data,data_size)){
            // simply generate new file as an independent block.
            m->v.resize(m->v.size() + 1);
            auto & t = m->v.back();
            {
                t.p = fs::path(m->o) / (std::to_string(ins_s) + "_" + std::to_string(ins_e));
                std::ofstream{t.p, std::ios::binary}.write((char *) data, data_size);
            }
            auto fn = t.p.filename().string();
            t.s = static_cast<kautil::cache::size_type>(std::atoll(fn.c_str()));
            t.e  = atoll(strpbrk(fn.c_str(),"_")+1);
        }
        m->c.add(ins_s,ins_e);
    }


    VFData VirtualFile::copy(pos_type const& want_s, pos_type const& want_e){
        namespace fs = std::filesystem;
        static auto lmb_elem_cnt1 =[this](auto & res,auto beg,auto end,auto const& want_s,auto const& want_e){
            auto const& fsize = fs::file_size(beg->p);
            auto const& offset_s = (want_s - beg->s) * m->block_size;
            auto const& size = ((end->e > want_e) ? fsize - ((end->e - want_e) * m->block_size) : fsize) - offset_s;
            offset_s ?
                std::ifstream{beg->p, std::ios::binary}.seekg(offset_s).read((char *) res.m->offset_data(0, size) , size)
                : std::ifstream{beg->p, std::ios::binary}.read((char *) res.m->offset_data(0, size) , size);
        };

        auto res = VFData::factory();
        if(m->v.size() == 1){
            lmb_elem_cnt1(res, m->v.begin(), m->v.begin(), want_s, want_e);
        }else{
            auto beg = m->v.begin();
            for (auto itr = beg; itr != m->v.end(); ++itr) {
                if ((itr->s <= want_s && want_s <= itr->e) || (want_s < itr->s)) { beg = itr;break; }
            }

            auto end = m->v.end() - 1;
            for (auto itr = end;; --itr) {
                if ((itr->s <= want_e && want_e <= itr->e) || (itr->e < want_e)) { end = itr;break; }
                if (itr == m->v.begin()) break;
            }

            if(beg == end) {
                lmb_elem_cnt1(res, m->v.begin(), m->v.end(), want_s, want_e);
            } else{
                // copy result between below
                for (auto itr = beg;; ++itr) {
                    auto const &fsize = fs::file_size(itr->p);
                    auto cur = res.size();
                    if (itr == beg) {
                        auto ifs = std::ifstream{itr->p, std::ios::binary};
                        if (want_s > beg->s) {
                            auto const &offset_s = (want_s - beg->s) * m->block_size;
                            auto const &size = fsize - offset_s;
                            std::ifstream{itr->p, std::ios::binary}.seekg(offset_s).read(
                                    (char *) res.m->offset_data(cur, size), size);
                        } else {
                            std::ifstream{itr->p, std::ios::binary}.read((char *) res.m->offset_data(cur, fsize), fsize);
                        }
                    } else if (itr == end) {
                        auto const &size = (end->e > want_e) ? fsize - ((end->e - want_e) * m->block_size) : fsize;
                        std::ifstream{itr->p, std::ios::binary}.read((char *) res.m->offset_data(cur, size), size);
                    } else {
                        std::ifstream{itr->p, std::ios::binary}.read((char *) res.m->offset_data(cur, fsize), fsize);
                    }
                    if (itr == end) break;
                }
            }
        }
        return res;
    }

    void VirtualFile::arrange(){
        namespace fs = std::filesystem;
        if(!std::is_sorted(m->v.begin(), m->v.end(), sortf)) std::sort(m->v.begin(), m->v.end(), sortf);
        auto count = int(0);
        for(auto l = m->v.end()-1;;--l){
            if(l == m->v.begin()) break;
            auto f = l-1;
            if(l->s - f->e == 1){
                {
                    std::ofstream {f->p, std::ios::binary | std::ios::app} << std::ifstream{l->p, std::ios::binary}.rdbuf();
                }
                auto const& p = fs::path(m->o)/(std::to_string(f->s) + "_" + std::to_string(l->e));
                fs::rename(f->p, p);
                f->p = p;
                f->e = l->e;
                fs::remove(l->p);
                l->s = kautil::cache::npos;
                ++count;
            }
        }
        if(count){
            if(!std::is_sorted(m->v.begin(), m->v.end(), sortf)) std::sort(m->v.begin(), m->v.end(), sortf);
            m->v.resize(m->v.size() - count);
        }
        m->c.arrange_stack();
    }

} //namespace cache{
} //namespace kautil{
