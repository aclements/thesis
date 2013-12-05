void setup_rename_rename_path_ec_test0(void) {
  close(open("__i0", O_CREAT|O_RDWR, 0666));
  link("__i0", "f0");
  link("__i0", "f1");
  unlink("__i0");
}
int test_rename_rename_path_ec_test0_op0(void) {
  return rename("f0", "f0");
}
int test_rename_rename_path_ec_test0_op1(void) {
  return rename("f1", "f0");
}
