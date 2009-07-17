#include "mozilla/test/TestChild.h"

using mozilla::test::TestChild;

// C++ file contents
nsresult TestChild::RecvPing()
{
    return SendPong(42);
}

nsresult TestChild::RecvPong(const int& status)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult TestChild::RecvTellValue(
            const String& key,
            const String& val)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult TestChild::RecvTellValues(
            const StringArray& keys,
            const StringArray& vals)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

TestChild::TestChild()
{
}

TestChild::~TestChild()
{
}
