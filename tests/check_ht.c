#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include "ht.h"
#include "ht_builtins.h"

START_TEST(test_hash_put)
{
	hash *h = hash_create(hash_string, hash_string_comparator);
	char *k1 = "key1";
	char *k2 = "key2";
	char *v1 = "value1";
	char *v2 = "value2";

	hash_put(h, k1, v1);
	fail_unless(h->size == 1, "hash_put() did not increment size");
	fail_unless(hash_get(h, k1) == v1, "hash_get() didn't return the expected value");
	hash_put(h, k2, v2);
	fail_unless(h->size == 2, "hash_put() did not increment size");
	fail_unless(hash_get(h, k2) == v2, "hash_get() didn't return the expected value");
	fail_unless(hash_get(h, "key3") == NULL, "hash_get() should have returned null");
}
END_TEST

START_TEST(test_hash_overwrite)
{
	hash *h = hash_create(hash_string, hash_string_comparator);
	char *k1 = "key1";
	char *v1 = "value1";
	char *v1_1 = "value1_1";
	hash_put(h, k1, v1);
	hash_put(h, k1, v1_1);
	fail_unless(h->size == 1, "Overwriting a key changed the hash size");
	fail_unless(hash_get(h, k1) == v1_1, "Overwriting a key didn't set the correct value");
}
END_TEST

START_TEST(test_hash_iterator)
{
	const int count = 1024;
	int indexes[count];
	char keys[count][16];
	bool marks[count];
	hash *h = hash_create(hash_string, hash_string_comparator);
	for (int i = 0; i < count; i++) {
		indexes[i] = i;
		marks[i] = false;
		sprintf(keys[i], "key-%d", i);
		hash_put(h, keys[i], &indexes + i);
	}

	fail_unless(h->size == count, "Failed to create all elements");

	hash_iterator *iter = hash_iterator_create(h);
	int marked = 0;
	while (iter->current_key) {
		int *val = (int *) iter->current_value;
		int index = *val;
		marks[*val] = true;
		++marked;
		hash_iterator_next(iter);
	}
	hash_iterator_destroy(iter);
	fail_unless(marked == count, "hash_iterator didn't yield enough values");
	for (int i = 0; i < count; i++) {
		fail_unless(marks[i], "missed index %i, key %s", i, keys[i]);
	}
}
END_TEST

Suite *ht_suite(void)
{
	Suite *s = suite_create("ht");
	TCase *tc_core = tcase_create("Core");
	tcase_add_test(tc_core, test_hash_put);
	tcase_add_test(tc_core, test_hash_overwrite);
	tcase_add_test(tc_core, test_hash_iterator);
	suite_add_tcase(s, tc_core);
	return s;
}

int main(void)
{
	int number_failed;
	Suite *s = ht_suite();
	SRunner *sr = srunner_create(s);
	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return number_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
