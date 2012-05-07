#include "tests.h"
#include "jsobj.h"
#include "vm/String.h"

BEGIN_TEST(testConservativeGC)
{
    jsval v2;
    EVAL("({foo: 'bar'});", &v2);
    CHECK(JSVAL_IS_OBJECT(v2));
    char objCopy[sizeof(JSObject)];
    js_memcpy(&objCopy, JSVAL_TO_OBJECT(v2), sizeof(JSObject));

    jsval v3;
    EVAL("String(Math.PI);", &v3);
    CHECK(JSVAL_IS_STRING(v3));
    char strCopy[sizeof(JSString)];
    js_memcpy(&strCopy, JSVAL_TO_STRING(v3), sizeof(JSString));

    jsval tmp;
    EVAL("({foo2: 'bar2'});", &tmp);
    CHECK(JSVAL_IS_OBJECT(tmp));
    JSObject *obj2 = JSVAL_TO_OBJECT(tmp);
    char obj2Copy[sizeof(JSObject)];
    js_memcpy(&obj2Copy, obj2, sizeof(JSObject));

    EVAL("String(Math.sqrt(3));", &tmp);
    CHECK(JSVAL_IS_STRING(tmp));
    JSString *str2 = JSVAL_TO_STRING(tmp);
    char str2Copy[sizeof(JSString)];
    js_memcpy(&str2Copy, str2, sizeof(JSString));

    tmp = JSVAL_NULL;

    JS_GC(rt);

    EVAL("var a = [];\n"
         "for (var i = 0; i != 10000; ++i) {\n"
         "a.push(i + 0.1, [1, 2], String(Math.sqrt(i)), {a: i});\n"
         "}", &tmp);

    JS_GC(rt);

    checkObjectFields((JSObject *)objCopy, JSVAL_TO_OBJECT(v2));
    CHECK(!memcmp(strCopy, JSVAL_TO_STRING(v3), sizeof(strCopy)));

    checkObjectFields((JSObject *)obj2Copy, obj2);
    CHECK(!memcmp(str2Copy, str2, sizeof(str2Copy)));

    return true;
}

bool checkObjectFields(JSObject *savedCopy, JSObject *obj)
{
    /* Ignore fields which are unstable across GCs. */
    CHECK(savedCopy->lastProperty() == obj->lastProperty());
    CHECK(savedCopy->getProto() == obj->getProto());
    return true;
}

END_TEST(testConservativeGC)

BEGIN_TEST(testDerivedValues)
{
  JSString *str = JS_NewStringCopyZ(cx, "once upon a midnight dreary");
  JS::Anchor<JSString *> str_anchor(str);
  static const jschar expected[] = { 'o', 'n', 'c', 'e' };
  const jschar *ch = JS_GetStringCharsZ(cx, str);
  str = NULL;

  /* Do a lot of allocation and collection. */
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 1000; j++)
      JS_NewStringCopyZ(cx, "as I pondered weak and weary");
    JS_GC(rt);
  }

  CHECK(!memcmp(ch, expected, sizeof(expected)));
  return true;
}
END_TEST(testDerivedValues)
