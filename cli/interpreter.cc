// TODO: add header
#include <iostream>

void interpret_input(const std::istream& is)
{
	std::cout << "VATA-CODE START\n";
	std::cout << is.rdbuf();
	std::cout << "VATA-CODE END\n";
}


