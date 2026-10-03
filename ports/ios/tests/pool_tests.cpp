#include <universal/q_shared.h>
#include <universal/pool_allocator.h>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>
static void check(bool ok,const char *message) {if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
int main()
{
    constexpr std::size_t count=257,stride=13;
    std::vector<char> storage(count*stride+2,static_cast<char>(0x5a));
    char *pool=storage.data()+1;
    pooldata_t state{};
    Pool_Init(pool,&state,stride,count);
    check(Pool_FreeCount(&state)==count && state.activeCount==0,"initial free count");
    std::array<freenode*,count> allocated;
    for(std::size_t i=0;i<count;++i) {
        allocated[i]=Pool_Alloc(&state);
        check(reinterpret_cast<char*>(allocated[i])==pool+i*stride,"native address or slot stride corrupted");
        std::fill_n(reinterpret_cast<char*>(allocated[i]),stride,static_cast<char>(i));
    }
    check(Pool_Alloc(&state)==nullptr&&Pool_FreeCount(&state)==0&&state.activeCount==count,"pool exhaustion");
    std::array<unsigned,count> order;std::iota(order.begin(),order.end(),0);
    std::mt19937 generator(410);std::shuffle(order.begin(),order.end(),generator);
    for(auto i:order) Pool_Free(allocated[i],&state);
    check(Pool_FreeCount(&state)==count&&state.activeCount==0,"random free sequence");
    for(auto i=order.rbegin();i!=order.rend();++i)
        check(Pool_Alloc(&state)==allocated[*i],"free-list LIFO order");
    check(storage.front()==char(0x5a)&&storage.back()==char(0x5a),"pool buffer overrun");
    Pool_Init(pool,&state,stride,count);
    check(Pool_FreeCount(&state)==count&&state.activeCount==0,"level-pool reset");
    std::cout<<"ARM64 pool allocation, exhaustion, randomized reuse and packed storage passed\n";
}
