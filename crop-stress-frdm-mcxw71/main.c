#if __has_include("APP/app.h")
#include "APP/app.h"
#else
#include "app.h"
#endif

int main(void)
{
    return app_run();
}
