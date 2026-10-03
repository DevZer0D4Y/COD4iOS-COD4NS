#include "FastfileWorld.hpp"
#include <iostream>
int main(int argc,char** argv) {
    if(argc<2) {std::cerr<<"Usage: kisakcod_map_inspect map.ff [map2.ff ...]\n";return 2;}
    int failed=0;
    for(int i=1;i<argc;++i) {
        try {
            auto mesh=kisakcod::assets::loadMap(argv[i]);
            std::cout<<argv[i]<<": "<<mesh.vertices.size()<<" vertices, "<<mesh.indices.size()/3
                     <<" triangles, "<<mesh.surfaceCount<<" surfaces, spawn="<<(mesh.spawn?"yes":"no")<<'\n';
        } catch(const std::exception& e) {std::cerr<<argv[i]<<": "<<e.what()<<'\n';++failed;}
    }
    return failed?1:0;
}
