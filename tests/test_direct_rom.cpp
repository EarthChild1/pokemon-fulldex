#define STB_IMAGE_IMPLEMENTATION
#include "direct_rom.h"
#include <cassert>
#include <fstream>
int main(int argc,char** argv) {
    assert(argc==2);namespace fs=std::filesystem;fs::path root=argv[1];
    fs::create_directories(root/"other");fs::create_directories(root/"saves");
    auto make=[&](const fs::path& path,const char* code){std::vector<char>b(4096);std::copy(code,code+4,b.begin()+12);std::ofstream f(path,std::ios::binary);f.write(b.data(),b.size());};
    make(root/"Game.nds","CPUE");make(root/"other/Game.nds","CPUE");
    {std::ofstream f(root/"saves/platinum.sav");f<<"existing save";}
    std::vector<Launcher::Game> games;
    assert(DirectRom::Ds((root/"Game.nds").string(),(root/"saves").string(),root.string(),games)==0);
    assert(DirectRom::Ds((root/"other/../Game.nds").string(),(root/"saves").string(),root.string(),games)==0);
    assert(DirectRom::Ds((root/"other/Game.nds").string(),(root/"saves").string(),root.string(),games)==1);
    assert(games.size()==2&&games[0].rom!=games[1].rom&&games[1].folder==(root/"saves").string());
    assert(DirectRom::Ds((root/"missing.nds").string(),root.string(),root.string(),games)==-1);
    make(root/"Unknown.nds","XXXX");assert(DirectRom::Ds((root/"Unknown.nds").string(),root.string(),root.string(),games)==-1);
    assert(games.size()==2&&fs::file_size(root/"Game.nds")==4096);
    std::ifstream f(root/"saves/platinum.sav");std::string s;std::getline(f,s);assert(s=="existing save");
    // A generic game (Pokemon Mystery Dungeon: Explorers of Sky): listed as known and generic, its own save name, no hack.
    make(root/"Explorers.nds","C2SE");
    {const int at=DirectRom::Ds((root/"Explorers.nds").string(),(root/"saves").string(),root.string(),games);
     assert(at==2&&games[at].known&&games[at].generic&&!games[at].hack&&games[at].saveName=="pmd-explorers-of-sky.sav");
     assert(!games[0].generic);} // (Platinum is not)
    assert(IsGenericGame("C2SE")&&!IsGenericGame("CPUE")&&!IsGenericGame("IPGE"));
    assert(DsSaveBytes("C2SE")==131072&&DsSaveBytes("CPUE")==524288);
    {std::vector<unsigned char> save(131072,0x5A); // a 128 KB save is right for Explorers of Sky and wrong for a Pokemon game
     assert(Launcher::SaveHealthOfData(save,true,true,DsSaveBytes("C2SE")).state==1);
     assert(Launcher::SaveHealthOfData(save,true,true).state==2);
     save.assign(524288,0x5A);assert(Launcher::SaveHealthOfData(save,true,true).state==1);
     save.assign(131072,0xFF);assert(Launcher::SaveHealthOfData(save,true,true,DsSaveBytes("C2SE")).state==2);} // (blank)
}
