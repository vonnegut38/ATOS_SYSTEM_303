#ifdef COMMANDER_SELFTEST
/* Native integration test: real GUI IPC, drawing, directory syscalls and
 * actual handlers; mouse events are injected, not physical IKBD packets. */
static uint32_t bench_page,bench_single;
static void test_mouse(unsigned x,unsigned y,unsigned buttons){gui_message_t m;zero(&m,sizeof(m));m.v[0]=0x4b4d4f55;m.v[1]=(x<<16)|y;m.v[2]=buttons<<24;handle(&m);}
#define REQUIRE(c,n) do{if(!(c)){fault=(n);return;}}while(0)
static void commander_selftest(void){uint32_t start;unsigned i,j;int n;entry page[17],one;
 render();REQUIRE(!fault&&used==1000,101);render();REQUIRE(!used,117);
 start=(uint32_t)ticks();n=gui_call(52,10,17,"\\SYSTEM\\TEST",page);bench_page=(uint32_t)ticks()-start;REQUIRE(n==17,102);
 start=(uint32_t)ticks();for(i=0;i<17;i++){
  REQUIRE(gui_call(13,10+i,0,"\\SYSTEM\\TEST",&one)==0,103);
  for(j=0;j<sizeof(entry);j++){/* bytes beyond name terminator are not ABI data */
   if(j<13){if(one.name[j]==0){REQUIRE(page[i].name[j]==0,104);break;}REQUIRE(one.name[j]==page[i].name[j],104);}}
  REQUIRE(one.attr==page[i].attr&&one.size==page[i].size,105);
 }bench_single=(uint32_t)ticks()-start;
 /* Two clicks on SYSTEM, followed by a coalesced right click back to root. */
 test_mouse(20,68,2);test_mouse(20,68,0);last_click_tick=(uint32_t)ticks();test_mouse(20,68,2);test_mouse(20,68,0);
 REQUIRE(panels[0].path[1]=='S',106);render();REQUIRE(!fault,107);
 test_mouse(20,68,12);REQUIRE(!panels[0].path[1],108);render();
 test_mouse(180,390,20);REQUIRE(modal==7,109);render();action(0,'A');action(0,'B');action(0,'C');render();REQUIRE(length==3&&used==2,110);
 test_mouse(350,240,20);REQUIRE(modal==0,111);render();
 test_mouse(340,84,20);REQUIRE(active==1&&panels[1].sel==1,112);render();REQUIRE(used>80&&used<250,113);
 test_mouse(30,390,20);REQUIRE(modal==5,114);render();test_mouse(350,240,20);REQUIRE(!modal,115);render();
 test_mouse(500,390,20);REQUIRE(quit,116);
}
#undef REQUIRE
#endif
