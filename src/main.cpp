#include "core/application.hpp"

int main(int /* argc */, char* /* argv */[]) {
    ProjectionMapper app(1600, 900);

    if (!app.initialize()) {
        return -1;
    }

    app.run();
    return 0;
}

