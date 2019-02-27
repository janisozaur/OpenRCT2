struct a {
  ~a();
};
struct b {
  a c;
  a d;
};
struct e {
  b f[4];
};
class g {
  e h{};
};
void i() { g(); }
