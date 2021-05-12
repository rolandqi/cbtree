#include "meta.h"

meta *meta::clone() {
  meta *ptr = new meta(*this);
  return ptr;
}
