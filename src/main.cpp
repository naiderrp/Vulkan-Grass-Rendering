#include "render_system.hpp"

int main() {
	render_system app{};
	
	try {
		app.run();
	}
	catch (std::exception err) {
		std::cout << err.what();
	}
	
	return 0;
}
