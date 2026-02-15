#include "app_runner.hpp"
#include "config.hpp"

int main(int argc, char** argv) {
    CAppRunner app(argc, argv);
    return app.Run();
}