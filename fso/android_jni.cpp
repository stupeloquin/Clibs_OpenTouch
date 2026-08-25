// Pyro Touch is not a licensed Play Store build, so the certificate check is
// compiled out (-DNO_SEC) and these are only here to satisfy the shared
// JNI/base sources.
static const char *key = "";
static const char *pkg = "com.opentouchgaming.pyrotouch";
static unsigned char sha_data[20] = {0};

#include "../android_jni_inc.cpp"

// Included here rather than built separately, matching what descent3 does.
#include "../touch_interface_base.cpp"
