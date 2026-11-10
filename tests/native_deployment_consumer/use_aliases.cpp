extern "C" int lvui_alias_a();
extern "C" int lvui_alias_b();
int lvui_force_alias_links = lvui_alias_a() + lvui_alias_b();
