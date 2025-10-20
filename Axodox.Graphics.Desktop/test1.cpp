#include "pch.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace CppUnitTest01
{
TEST_CLASS(Test01){public : TEST_METHOD(method1){auto x = 1;
auto y = 2;
auto z = x + y;
Assert::AreEqual(3, z);
}
}
;
}
