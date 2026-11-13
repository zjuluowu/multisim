#include "ffrt_runtime.h"
#include <pthread.h>
#include <future>
#include <iostream>
struct Gate { std::promise<void> entered, release; std::shared_future<void> ready = release.get_future().share(); };
void *Run(void *p) { auto &g=*static_cast<Gate *>(p); g.entered.set_value(); g.ready.wait(); return nullptr; }
int main() {
 pthread_attr_t attributes; if(pthread_attr_init(&attributes) || pthread_attr_setdetachstate(&attributes,PTHREAD_CREATE_DETACHED)) return 2;
 for(unsigned i=0;i<100;++i) {
  Gate g; pthread_t id;
  if(pthread_create(&id,&attributes,Run,&g)) return 3;
  g.entered.get_future().wait();
  auto active=SimHost::GetDetachedThreadEvidence().active;
  bool early=SimHost::AwaitDetachedThreadsIdle(std::chrono::milliseconds(0));
  g.release.set_value();
  bool done=SimHost::AwaitDetachedThreadsIdle(std::chrono::seconds(3));
  if(active!=1 || early || !done) { std::cerr<<"failure iteration="<<i<<" active="<<active<<" early="<<early<<" done="<<done<<"\n"; return 1; }
 }
 pthread_attr_destroy(&attributes); std::cout<<"100 attribute-detached completion checks PASS\n";
}
