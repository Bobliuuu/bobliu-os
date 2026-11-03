/* stand-in for musl's features.h (only what libm needs) */
#ifndef _FEATURES_H
#define _FEATURES_H
#define hidden __attribute__((__visibility__("hidden")))
#define weak __attribute__((__weak__))
#define weak_alias(old, new) extern __typeof(old) new __attribute__((__weak__, __alias__(#old)))
#endif
