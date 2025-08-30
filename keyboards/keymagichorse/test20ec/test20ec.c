#include "quantum.h"
#include "km_printf.h"

void board_init(void) {
#   if defined(KM_DEBUG)
    km_printf_init();
    km_printf("hello rtt log1111111\r\n");
#   endif
}

void housekeeping_task_kb(void) {

}
