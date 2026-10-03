#include "FastfileWorld.hpp"
#include <zlib.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string_view>

namespace kisakcod::assets {
namespace {
constexpr std::uint32_t kInline=0xffffffffu, kInsert=0xfffffffeu;
constexpr std::size_t kCompressedLimit=128u<<20, kExpandedLimit=256u<<20;
constexpr std::size_t kWorldSize=732, kVertexLimit=1'000'000, kIndexLimit=6'000'000;
[[noreturn]] void fail(const std::string& text) {throw BspError("Fastfile: "+text);}
bool inlined(std::uint32_t p) {return p==kInline || p==kInsert;}

struct Reader {
    const std::vector<std::uint8_t>& bytes;
    std::size_t cursor=0, records=0;
    void range(std::size_t at,std::size_t size) const {
        if(at>bytes.size() || size>bytes.size()-at) fail("truncated data at byte "+std::to_string(at));
    }
    std::uint8_t u8(std::size_t at) const {range(at,1);return bytes[at];}
    std::uint16_t u16(std::size_t at) const {range(at,2);return bytes[at]|(std::uint16_t(bytes[at+1])<<8);}
    std::uint32_t u32(std::size_t at) const {
        range(at,4);return bytes[at]|(std::uint32_t(bytes[at+1])<<8)|
            (std::uint32_t(bytes[at+2])<<16)|(std::uint32_t(bytes[at+3])<<24);
    }
    float f32(std::size_t at) const {
        const auto bits=u32(at);float value;std::memcpy(&value,&bits,4);
        if(!std::isfinite(value) || std::abs(value)>1e7f) fail("invalid geometry component");
        return value;
    }
    std::size_t take(std::size_t size) {
        range(cursor,size);auto at=cursor;cursor+=size;return at;
    }
    std::size_t array(std::size_t count,std::size_t stride) {
        if(count>kIndexLimit || stride==0 || count>(bytes.size()-cursor)/stride)
            fail("invalid array size");
        if(++records>1'000'000) fail("too many serialized records");
        return take(count*stride);
    }
    void string(std::uint32_t ptr) {
        if(ptr!=kInline) return;
        auto start=cursor;
        while(u8(cursor)!=0) {take(1);if(cursor-start>4096) fail("oversized string");}
        take(1);
    }
    // Main-stream file bytes are packed. DB_AllocStreamPos aligns destination
    // memory, NOT this file cursor. Stream 1 consumes no file bytes at all.
    void image(std::uint32_t ptr) {
        if(!inlined(ptr)) return;
        auto at=take(36);string(u32(at+32));
        if(inlined(u32(at+4))) {auto load=take(16);take(u32(load+12));}
    }
    void material(std::uint32_t ptr) {
        if(!inlined(ptr)) return;
        auto at=take(80);string(u32(at));
        if(inlined(u32(at+64))) fail("inline technique sets are not supported by the geometry reader");
        if(u32(at+68)==kInline) {
            auto table=array(u8(at+58),12);
            for(std::size_t i=0;i<u8(at+58);++i) {
                auto t=table+i*12;
                if(u8(t+7)==11) {
                    if(u32(t+8)==kInline) {
                        auto water=take(68);
                        auto m=u32(water+12),n=u32(water+16);
                        if(m>4096 || n>4096) fail("invalid water dimensions");
                        if(u32(water+4)) array(std::size_t(m)*n,8);
                        if(u32(water+8)) array(std::size_t(m)*n,4);
                        image(u32(water+64));
                    }
                } else image(u32(t+8));
            }
        }
        if(u32(at+72)==kInline) array(u8(at+59),32);
        if(u32(at+76)==kInline) array(u8(at+60),8);
    }
    void cell(std::size_t at) {
        if(u32(at+28)) {
            auto count=u32(at+24);auto table=array(count,44);
            for(std::size_t i=0;i<count;++i) {
                auto t=table+i*44;
                if(u32(t+36)==kInline) array(u16(t+34),2);
            }
        }
        if(u32(at+36)) {
            auto count=u32(at+32);auto table=array(count,68);
            for(std::size_t i=0;i<count;++i) {
                auto t=table+i*68;
                if(u32(t+32)==kInline) fail("inline portal cells are unsupported");
                if(u32(t+36)) array(u8(t+40),12);
            }
        }
        if(u32(at+44)) array(u32(at+40),4);
        if(u32(at+52)) array(u8(at+48),1);
    }
};

std::vector<std::uint8_t> expand(const std::vector<std::uint8_t>& compressed) {
    if(compressed.size()<14 || compressed.size()>kCompressedLimit) fail("invalid compressed file size (maximum 128 MiB)");
    Reader header{compressed};
    if(std::memcmp(compressed.data(),"IWffu100",8) || header.u32(8)!=5)
        fail("expected a PC COD4 IWffu100 version 5 map");
    z_stream stream{};
    stream.next_in=const_cast<Bytef*>(compressed.data()+12);
    stream.avail_in=static_cast<uInt>(compressed.size()-12);
    if(inflateInit(&stream)!=Z_OK) fail("cannot initialize zlib");
    struct Cleanup {z_stream* stream;~Cleanup(){inflateEnd(stream);}} cleanup{&stream};
    std::vector<std::uint8_t> result;
    std::array<std::uint8_t,65536> block{};
    int code=Z_OK;
    while(code!=Z_STREAM_END) {
        stream.next_out=block.data();stream.avail_out=block.size();
        code=inflate(&stream,Z_NO_FLUSH);
        if(code!=Z_OK && code!=Z_STREAM_END) fail("corrupt or incomplete zlib stream");
        auto used=block.size()-stream.avail_out;
        if(used>kExpandedLimit-result.size()) fail("expanded data exceeds 256 MiB");
        result.insert(result.end(),block.begin(),block.begin()+used);
        if(!used && code!=Z_STREAM_END) fail("incomplete zlib stream");
    }
    if(stream.avail_in!=0) fail("unexpected data after the zlib stream");
    return result;
}

std::size_t findWorld(const Reader& r,const std::string& base) {
    if(base.empty() || base.size()>64 || base.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")!=std::string::npos)
        fail("keep the original map filename, for example mp_shipment.ff");
    std::string needle=base;needle.push_back('\0');
    std::string_view view(reinterpret_cast<const char*>(r.bytes.data()),r.bytes.size());
    std::size_t found=std::string::npos,candidates=0;
    for(std::size_t at=view.find(needle);at!=std::string::npos;at=view.find(needle,at+1)) {
        if(++candidates>4096) fail("excessive map-name candidates");
        // Usually GfxWorld.name aliases the preceding clipMap name. Also allow
        // it inline immediately before the inline baseName.
        std::string fullName=(base.compare(0,3,"mp_")==0?"maps/mp/":"maps/")+base+".d3dbsp";
        std::array<std::size_t,2> gaps{0,fullName.size()+1};
        for(auto gap:gaps) {
            if(at<kWorldSize+gap) continue;
            auto w=at-kWorldSize-gap;
            if(r.u32(w+4)!=kInline || r.u32(w+20)!=kInline || r.u32(w+52)!=kInline || r.u32(w+660)!=kInline) continue;
            if((gap==0 && r.u32(w)==kInline) || (gap!=0 && r.u32(w)!=kInline)) continue;
            if(gap && view.substr(w+kWorldSize,gap-1)!=fullName) continue;
            auto vc=r.u32(w+48),ic=r.u32(w+16),sc=r.u32(w+24);
            if(!vc || vc>kVertexLimit || !ic || ic>kIndexLimit || ic%3 || !sc || sc>65536) continue;
            if(r.u32(w+584)>sc || r.u32(w+588)>sc) continue;
            if(found!=std::string::npos && found!=w) fail("ambiguous GfxWorld records");
            found=w;
        }
    }
    if(found==std::string::npos) fail("no supported GfxWorld found; select the map .ff, keep its original name, and avoid _load.ff packages");
    return found;
}

BspMesh worldMesh(Reader& r,std::size_t at) {
    auto w=[&](std::size_t offset){return r.u32(at+offset);};
    r.cursor=at+kWorldSize;r.string(w(0));r.string(w(4));
    auto indices=r.array(w(16),2);
    if(w(36)) r.array(w(32),4);
    r.image(w(40));
    if(w(200)==kInline) {
        auto sun=r.take(64);
        if(inlined(r.u32(sun+60))) {auto def=r.take(16);r.string(r.u32(def));r.image(r.u32(def+4));}
    }
    if(w(232)) {
        auto probes=r.array(w(228),16);
        for(std::size_t i=0;i<w(228);++i) r.image(r.u32(probes+i*16+12));
    }
    if(w(244)==kInline) r.array(w(8),20);
    if(w(248)) r.array(w(12),2);
    if(w(260)) {
        auto cells=r.array(w(240),56);
        for(std::size_t i=0;i<w(240);++i) r.cell(cells+i*56);
    }
    if(w(268)) {
        auto lightmaps=r.array(w(264),8);
        for(std::size_t i=0;i<w(264);++i) {r.image(r.u32(lightmaps+i*8));r.image(r.u32(lightmaps+i*8+4));}
    }
    auto g=at+272;
    if(r.u32(g+28)) {
        auto axis=r.u32(g+20);
        if(axis>2) fail("invalid light grid row axis");
        auto low=r.u16(g+8+2*axis),high=r.u16(g+14+2*axis);
        if(high<low) fail("invalid light grid range");
        r.array(std::size_t(high)-low+1,2);
    }
    if(r.u32(g+36)) r.take(r.u32(g+32));
    if(r.u32(g+44)) r.array(r.u32(g+40),4);
    if(r.u32(g+52)) r.array(r.u32(g+48),168);
    if(w(340)) r.array(w(336),56);
    if(w(376)) {
        auto memory=r.array(w(372),8);
        for(std::size_t i=0;i<w(372);++i) r.material(r.u32(memory+i*8));
    }
    auto vertices=r.array(w(48),44);
    if(w(64)) r.take(w(60));
    r.material(w(384));r.material(w(388));r.image(w(540));
    if(w(572)) {
        auto shadows=r.array(w(220),12);
        for(std::size_t i=0;i<w(220);++i) {
            auto t=shadows+i*12;
            if(r.u32(t+4)) r.array(r.u16(t),2);
            if(r.u32(t+8)) r.array(r.u16(t+2),2);
        }
    }
    if(w(576)) {
        auto regions=r.array(w(220),8);
        for(std::size_t i=0;i<w(220);++i) {
            auto t=regions+i*8;
            if(r.u32(t+4)) {
                auto count=r.u32(t);auto hulls=r.array(count,80);
                for(std::size_t j=0;j<count;++j) {
                    auto h=hulls+j*80;
                    if(r.u32(h+76)) r.array(r.u32(h+72),20);
                }
            }
        }
    }
    auto dp=at+580;
    if(r.u32(dp+72)) r.array(std::size_t(r.u32(dp+4))+r.u32(dp+8),2);
    if(r.u32(dp+76)) r.array(r.u32(dp),28);
    auto surfaces=r.array(w(24),48);
    // No further subobjects are needed. External material/model aliases remain
    // disk values and are neither cast to pointers nor called by this reader.
    BspMesh mesh;mesh.surfaceCount=w(24);
    mesh.vertices.reserve(w(48));mesh.indices.reserve(w(16));
    for(std::size_t i=0;i<w(48);++i) {
        auto v=vertices+i*44;BspVertex vertex;
        for(int axis=0;axis<3;++axis) vertex.position[axis]=r.f32(v+axis*4);
        float scale=(r.u8(v+39)+192.0f)/32385.0f;
        for(int axis=0;axis<3;++axis) vertex.normal[axis]=(r.u8(v+36+axis)-127.0f)*scale;
        vertex.color={r.u8(v+18)/255.0f,r.u8(v+17)/255.0f,r.u8(v+16)/255.0f,1.0f};
        mesh.vertices.push_back(vertex);
    }
    mesh.minBounds.fill(std::numeric_limits<float>::max());
    mesh.maxBounds.fill(std::numeric_limits<float>::lowest());
    for(std::size_t i=0;i<w(24);++i) {
        auto s=surfaces+i*48;
        auto base=r.u32(s+4),count=std::uint32_t(r.u16(s+8)),num=std::uint32_t(r.u16(s+10))*3,first=r.u32(s+12);
        if(!count || base>w(48) || count>w(48)-base || !num || first>w(16) || num>w(16)-first)
            fail("surface range exceeds world geometry");
        if(num>kIndexLimit-mesh.indices.size()) fail("expanded index budget exceeded");
        for(std::uint32_t j=0;j<num;++j) {
            auto relative=r.u16(indices+(std::size_t(first)+j)*2);
            if(relative>=count) fail("surface-local vertex index is out of bounds");
            auto absolute=base+relative;mesh.indices.push_back(absolute);
            for(int axis=0;axis<3;++axis) {
                float p=mesh.vertices[absolute].position[axis];
                mesh.minBounds[axis]=std::min(mesh.minBounds[axis],p);
                mesh.maxBounds[axis]=std::max(mesh.maxBounds[axis],p);
            }
        }
    }
    return mesh;
}
}

BspMesh loadFastfileWorld(const std::vector<std::uint8_t>& compressed,const std::string& mapBaseName) {
    auto bytes=expand(compressed);Reader reader{bytes};
    auto at=findWorld(reader,mapBaseName);
    auto mesh=worldMesh(reader,at);
    // Optional MapEnts: match the same map-name reference as GfxWorld, the
    // inline text marker, a bounded length, opening brace and trailing NUL.
    // Do not interpret arbitrary script strings as entity records.
    std::array<std::uint8_t,8> pattern{};
    std::copy_n(bytes.begin()+at,4,pattern.begin());
    std::fill(pattern.begin()+4,pattern.end(),255);
    auto next=bytes.begin();std::size_t candidates=0;
    while(next!=bytes.end()) {
        auto found=std::search(next,bytes.end(),pattern.begin(),pattern.end());
        if(found==bytes.end()) break;
        next=found+1;auto offset=static_cast<std::size_t>(found-bytes.begin());
        if(++candidates>100000) fail("excessive entity record candidates");
        if(bytes.size()-offset<13 || reader.u8(offset+12)!='{') continue;
        auto length=reader.u32(offset+8);
        if(length<3 || length>(4u<<20) || length>bytes.size()-offset-12 || reader.u8(offset+11+length)!=0) continue;
        auto text=std::string_view(reinterpret_cast<const char*>(bytes.data()+offset+12),length);
        if(text.find("\"worldspawn\"")==std::string_view::npos) continue;
        mesh.spawn=parseBspSpawn(text);break;
    }
    return mesh;
}

BspMesh loadMap(const std::string& path) {
    std::ifstream stream(path,std::ios::binary|std::ios::ate);
    if(!stream) fail("cannot open map file");
    auto size=stream.tellg();
    if(size<0 || static_cast<std::uintmax_t>(size)>kCompressedLimit) fail("invalid map file size (maximum 128 MiB)");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));stream.seekg(0);
    if(!bytes.empty() && !stream.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()))) fail("file changed while reading");
    if(stream.peek()!=std::char_traits<char>::eof()) fail("file changed while reading");
    if(bytes.size()>=4 && !std::memcmp(bytes.data(),"IBSP",4)) return loadBsp(bytes);
    return loadFastfileWorld(bytes,std::filesystem::path(path).stem().string());
}
}
