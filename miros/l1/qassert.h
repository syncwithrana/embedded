#ifndef QASSERT_H
#define QASSERT_H

#define Q_DEFINE_THIS_FILE     static char const Q_this_file_[] = __FILE__;

#define Q_ASSERT(test_) ((test_)     ? (void)0 : Q_onAssert(&Q_this_file_[0], __LINE__))

#define Q_REQUIRE(test_) Q_ASSERT(test_)
#define Q_ERROR()        Q_onAssert(&Q_this_file_[0], __LINE__)
#define Q_DIM(array_)    (sizeof(array_) / sizeof((array_)[0]))

void Q_onAssert(char const *module, int loc);

#endif /* QASSERT_H */
