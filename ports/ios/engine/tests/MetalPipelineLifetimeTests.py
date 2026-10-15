#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[4];s=(r/'ports/ios/d3d9/metal/metal_backend.mm').read_text()
def function(signature):
 a=s.index(signature);return s[a:s.index('\n}\n',a)+3]
production=function('void DestroyShader(')+function('void DestroyVertexLayout(')
code=r'''
#include <cassert>
#include <mutex>
#include <map>
#include <string>
#include <set>
#include <memory>
struct Shader{};struct VertexLayout{};
struct Context{std::recursive_mutex lock;std::map<std::string,std::shared_ptr<int>> pipelines;std::set<std::string> failedPipelines;};Context *g=nullptr;
'''+production+r'''
int main(){DestroyShader(new Shader);DestroyVertexLayout(new VertexLayout);Context context;g=&context;
 auto resource=std::make_shared<int>(7);std::weak_ptr<int> weak=resource;g->pipelines["old-map"]=resource;resource.reset();g->failedPipelines.insert("old-failure");
 DestroyShader(new Shader);assert(g->pipelines.empty() && g->failedPipelines.empty() && weak.expired());
 g->pipelines["new-map"]=std::make_shared<int>(8);g->failedPipelines.insert("layout");DestroyVertexLayout(new VertexLayout);assert(g->pipelines.empty() && g->failedPipelines.empty());
}
'''
with tempfile.TemporaryDirectory(prefix='cod4-metal-lifetime-') as d:
 p=Path(d);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Production shader/layout destruction releases cached pipelines and failed keys, preventing cross-map address reuse.')
