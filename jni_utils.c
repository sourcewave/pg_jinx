
#include <pg_jinx.h>

jstring makeJavaString(const char* cp) {
  if (cp == NULL) return NULL;
  char *utf8 = (char *)pg_do_encoding_conversion((unsigned char*)cp, strlen(cp), GetDatabaseEncoding(), PG_UTF8);
  jstring result = (*env)->NewStringUTF(env, utf8);
  if (utf8 != cp) pfree(utf8);
  return result;
}

char* fromJavaString(jstring javaString) {
  if (javaString == NULL) return NULL;
  const char *utf8 = (*env)->GetStringUTFChars(env, javaString, 0);
  char *result = (char*)pg_do_encoding_conversion((unsigned char *)utf8, strlen(utf8), PG_UTF8, GetDatabaseEncoding());
  if (result == utf8) result = pstrdup(result);
  (*env)->ReleaseStringUTFChars(env, javaString, utf8);
  return result;
}

