#pragma once
struct NativeCardOpenModel final {
    int clicks=0,opens=0;
    void select(){++clicks;}
    void open(){++opens;}
};
