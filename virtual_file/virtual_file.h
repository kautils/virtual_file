#ifndef KAUTIL_CACHE_VIRTUAL_FILE_H
#define KAUTIL_CACHE_VIRTUAL_FILE_H

#include "stdint.h"



namespace kautil{
namespace cache{

using pos_type = uint64_t;
using size_type = uint64_t;


struct InternalVFData;
struct VFData final{
    static VFData factory();
    ~VFData();
    void * data();
    size_type size();
    void move_to(VFData * dst);

private:
    friend class VirtualFile;
    VFData(VFData&& v);
    VFData(bool);
    InternalVFData * m = nullptr;
};




struct VirtualFileBase{
    virtual ~VirtualFileBase() = default;
    virtual void add(pos_type const& ins_s,pos_type const& ins_e,void * data) = 0;
    virtual VFData copy(pos_type const& want_s, pos_type const& want_e) = 0;
    virtual void arrange() = 0;
};


/** @brief
this class uses local file system internally.
by inheriting struct VirtualFileBase, possilbe to use other library such as hdf5 internally.
it may be faster to use hdf5,sqlite3 or other than use local file system.
**/

struct Internal;
struct VirtualFile final : public VirtualFileBase{
    VirtualFile(const char * output_dir, size_type const& block_size );
    ~VirtualFile() override ;
    bool load(const char * output_dir, size_type const& block_size);
    void add(pos_type const& ins_s,pos_type const& ins_e,void * data) override ;

    size_t const& block_size();

    VFData copy(pos_type const& want_s, pos_type const& want_e) override ;
    void arrange() override ;

    VFData gap();
    VFData gap(pos_type const& want_s,pos_type const& want_e);
    size_type gap(void * dst,pos_type const& want_s,pos_type const& want_e);

private:
    Internal * m = nullptr;
};



} //namespace cache{
} //namespace kautil{


#endif