#include <stdio.h>
void test_game(void);
void test_editor(void);
void test_storage(void);
void test_content(void);
void test_regression(void);
int main(void) {
    test_game();puts("PASS engine");
    test_editor();puts("PASS editor");
    test_storage();puts("PASS storage / codec");
    test_content();puts("PASS campaign / endless replay");
    test_regression();puts("PASS edge-case regressions");
    puts("ALL TESTS PASSED");return 0;
}
