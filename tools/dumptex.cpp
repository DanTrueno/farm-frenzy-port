// Dump every texture from the packs to PNG for inspection.
#include "../src/vfs.h"
#include "../src/image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <cstdio>
#include <string>
#include "../src/portable.h"
static void mkdirs(std::string p){ std::error_code ec; std::filesystem::create_directories(u8path(p).parent_path(), ec); }
int main(int argc,char**argv){
    g_vfs.mount(std::string(argv[1])+"/data.pack"); g_vfs.mount(std::string(argv[1])+"/ru.pack");
    int ok=0,fail=0;
    for(auto& f: g_vfs.list("")){
        if(!(f.size()>4 && (f.substr(f.size()-4)==".dds"||f.substr(f.size()-4)==".jng"||f.substr(f.size()-4)==".jpg"))) continue;
        std::vector<uint8_t> d; g_vfs.read(f,d); Image im;
        if(!decodeImage(d.data(),d.size(),im)){ printf("FAIL %s\n",f.c_str()); fail++; continue; }
        std::string o=std::string(argv[2])+"/"+f+".png"; mkdirs(o);
        stbi_write_png(o.c_str(),im.w,im.h,4,im.rgba.data(),im.w*4); ok++;
    }
    printf("ok=%d fail=%d\n",ok,fail);
}
