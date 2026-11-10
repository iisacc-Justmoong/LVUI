#include <LVUI/Card.h>
#include <stdexcept>
int main(){lvui::CardConfiguration c;if(c.type!=lvui::CardType::file||c.padding!=18)throw std::runtime_error("Card design defaults changed");}
